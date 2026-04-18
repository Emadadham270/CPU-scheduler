#include "../data_structures/PCB/Sch_PCB.h"
#include "scheduler.h"
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <stdbool.h>
#include <errno.h>

#define MSGQ_SUB1_PROJ     75
#define MSGQ_SUB2_PROJ     76
#define MSGQ_RESPONSE_PROJ 77
#define LOAD_SHM_PROJ      80
#define LOAD_SEM_PROJ      85
#define THRESHOLD_SEM_PROJ 86

/* Load SHM layout: 4 ints = [count1, totalRT1, count2, totalRT2] */
#define LOAD_SHM_SIZE          (4 * sizeof(int))
#define LOAD_SEM_NUM           1
#define LOAD_SHM_SLOT_COUNT1   0
#define LOAD_SHM_SLOT_TOTALRT1 1
#define LOAD_SHM_SLOT_COUNT2   2
#define LOAD_SHM_SLOT_TOTALRT2 3

/* Message types for the 3 message queues */
#define MTYPE_NEW_PROCESS     1   /* main → sub: here's a new process          */
#define MTYPE_TERMINATE       5   /* main → sub: no more processes, finish up  */
#define MTYPE_STEAL_CMD      10   /* main → sub: remove your rear process      */
#define MTYPE_STEAL_RESPONSE 11   /* sub → main: here's the stolen processData */
#define MTYPE_STEAL_EMPTY    12   /* sub → main: queue is empty, nothing to steal */

/* Keyfile path — same one used everywhere in the project */
#define KEYFILE_PATH "../keyFile"

void destroyClk(bool terminateAll);
int getClk(void);
void check_threshold(int M,int N);

void down(int sem)
{
    struct sembuf op;

    op.sem_num = 0;
    op.sem_op = -1;
    op.sem_flg = !IPC_NOWAIT;

    while (semop(sem, &op, 1) == -1)
    {
        if (errno == EINTR)
            continue; // retry if interrupted by signal
        perror("Error in down()");
        exit(-1);
    }
}

void up(int sem)
{
    struct sembuf op;

    op.sem_num = 0;
    op.sem_op = 1;
    op.sem_flg = !IPC_NOWAIT;

    if (semop(sem, &op, 1) == -1)
    {
        perror("Error in up()");
        exit(-1);
    }
}

static void clear_pending_msgs(int qid)
{
    processData tmp;
    while (msgrcv(qid, &tmp, sizeof(processData) - sizeof(long), 0, IPC_NOWAIT) != -1)
        ;
}

struct PCB createPCB(processData p)
{
    struct PCB pcb;

    pcb.id = p.id;
    pcb.arrival = p.arrival;
    pcb.runtime = p.runtime;
    pcb.priority = p.priority;
    pcb.start_time = -1;
    pcb.finish_time = -1;
    pcb.last_stopped = -1;
    pcb.remaining_time = p.runtime;
    pcb.waiting_time = 0;
    pcb.state = 'W';
    pcb.pid = -1;
    pcb.next = NULL;

    return pcb;
}

struct PerfVars initialize_perf() {
    struct PerfVars perf;

    perf.avg_Waiting = 0.0;
    perf.avg_WTA = 0.0;
    perf.first_arrival = -1;
    perf.num_procs = 0;
    perf.std_WTA = 0.0;
    perf.total_runtime = 0;
    perf.finish_time = -1;
    perf.M2_WTA = 0.0;
    perf.welford_mean_WTA = 0.0;

    return perf;
}

void runProcess(struct PCB *pcb, FILE *log_file)
{

    *shmRT_addr=pcb->remaining_time;
    if (pcb->start_time == -1)
    {
        pcb->start_time = getClk();
        pcb->waiting_time = pcb->start_time - pcb->arrival;
    }
    
    if (pcb->pid == -1)
    {
        pcb->lState = START;
        log_data(log_file, pcb);
        pid_t pid = fork();
        if (pid == -1)
        {
            perror("fork failed");
            exit(1);
        }

        if (pid == 0)
        {
            char runtime_str[16];
            char shm_str[16];
            char sem_str[16];

            snprintf(runtime_str, sizeof(runtime_str), "%d", pcb->remaining_time);
            snprintf(shm_str,sizeof(shm_str), "%d", shmRT_id);
            snprintf(sem_str,sizeof(sem_str), "%d", sem_id);
            execl("../outFiles/process.out", "process.out", runtime_str, shm_str, sem_str,
                  (char *)NULL);
            perror("execl failed");
            _exit(1);
        }

        pcb->pid = pid;
    }
    else
    {
        if(pcb->last_stopped >= 0)
            pcb->waiting_time += (getClk() - pcb->last_stopped);
        pcb->lState = RESUME;
        log_data(log_file, pcb);
        kill(pcb->pid, SIGCONT);
    }
    pcb->state = 'R';
}

// Release all IPC resources
void cleanup(int signum)
{
    (void)signum;
    msgctl(msgq_id, IPC_RMID, NULL);
    semctl(sem_id, 0, IPC_RMID);
    shmdt(shmRT_addr);
    shmctl(shmRT_id, IPC_RMID, NULL);
    semctl(load_sem_id, 0, IPC_RMID);
    if (subCpu_created)
        destroy_2cpu_ipcs();
    destroyClk(false);
    exit(0);
}

void RR_algo(Queue *readyQueue, struct PCB **currProcess, int q,
             int *next_preemtion_time, FILE *log_file)
{
    if (*currProcess != NULL)
    {
        /* Set up the preemption deadline when a process first starts its slice */
        if (*next_preemtion_time == -1)
        {
            *next_preemtion_time = getClk() + q;
            return;
        }

        /* Check if the quantum has expired */

        if((isEmpty(readyQueue)|| readyQueue->front->pcb->arrival == getClk()) && getClk() >= *next_preemtion_time )
        {
            *next_preemtion_time = getClk() + q;
            return;
        }

        if (getClk() >= *next_preemtion_time )
        {
            /* Preempt: stop the current process and put it back in the queue */
            (*currProcess)->lState = STOP;
            (*currProcess)->remaining_time = *shmRT_addr;
            kill((*currProcess)->pid, SIGSTOP);

            log_data(log_file, *currProcess);
            (*currProcess)->last_stopped = getClk();

            (*currProcess)->remaining_time = *shmRT_addr;
            (*currProcess)->state = 'W';

            enqueue(readyQueue, (*currProcess));
            processStopped=1;
            wait_N_secs(1, 1);
            *next_preemtion_time = getClk() + q;

            /* Pick the next process from the queue */
            *currProcess = dequeue(readyQueue);
            runProcess(*currProcess, log_file);
            return;
        }
    }
    else if (!isEmpty(readyQueue))
    {
        *currProcess = dequeue(readyQueue);
        runProcess(*currProcess, log_file);
        *next_preemtion_time = getClk() + q;
        return;
    }
}

void HPF_algo(Queue *readyQueue, struct PCB **currProcess, FILE *log_file)
{

    if (*currProcess != NULL && !isEmpty(readyQueue))
    {
        PCB *top = peek(readyQueue);

        if (top->priority < (*currProcess)->priority)
        {
            (*currProcess)->last_stopped = getClk();
            kill((*currProcess)->pid, SIGSTOP);

            (*currProcess)->remaining_time = *shmRT_addr;
            (*currProcess)->state = 'W';
            (*currProcess)->lState = STOP;

            log_data(log_file, *currProcess);

            enqueue_priority(readyQueue, (*currProcess));
            (*currProcess) = NULL;
            wait_N_secs(1,1);
        }
    }
    else if ((*currProcess) == NULL && !isEmpty(readyQueue))
    {

        *currProcess = dequeue(readyQueue);
        runProcess(*currProcess, log_file);
    }
}


void FCFS_algo(Queue *readyQueue, struct PCB **currProcess, int N, int M, FILE *log_file)
{
    (void)currProcess;
    (void)log_file;

    /* Route all queued processes to the appropriate sub-scheduler.
     * Distribution is based on READY-queue counts only (running process excluded). */
    if (!isEmpty(readyQueue))
    {
        int c1, c2, rt1, rt2;
        read_all_load_shm(load_shm_addr, &c1, &rt1, &c2, &rt2);
        int projected_ready_1 = c1;
        int projected_ready_2 = c2;
        int projected_total_1 = rt1;
        int projected_total_2 = rt2;

        while (!isEmpty(readyQueue))
        {
            PCB *pcb = dequeue(readyQueue);
            processData p = pcb_to_processData(pcb);

            int cpu = (projected_ready_1 <= projected_ready_2) ? 1 : 2;
            if (cpu == 1)
            {
                send_process_msg(msgq_sub1_id, &p, MTYPE_NEW_PROCESS);
                projected_ready_1++;
                projected_total_1 += p.runtime;
            }
            else
            {
                send_process_msg(msgq_sub2_id, &p, MTYPE_NEW_PROCESS);
                projected_ready_2++;
                projected_total_2 += p.runtime;
            }

            free(pcb);
        }

        /* Publish projected load so global SHM stays consistent immediately,
         * even before sub-schedulers drain their message queues. */
        down(load_sem_id);
        load_shm_addr[LOAD_SHM_SLOT_COUNT1] = projected_ready_1;
        load_shm_addr[LOAD_SHM_SLOT_TOTALRT1] = projected_total_1;
        load_shm_addr[LOAD_SHM_SLOT_COUNT2] = projected_ready_2;
        load_shm_addr[LOAD_SHM_SLOT_TOTALRT2] = projected_total_2;
        up(load_sem_id);
    }

    /* Every N ticks: check load balance and steal if needed */
    if (subCpu_created)
    {
        N_time++;
        if (N_time >= N)
        {
            N_time = 0;
             
            check_threshold(M, N);
        }
            /* Allow sub-schedulers to execute after main finishes this tick's
             * FCFS routing/threshold decision (one permit per sub-scheduler). */
            union Semun gate;
            gate.val = 2;
            semctl(threshold_sem_id, 0, SETVAL, gate);
    }
}


void wait_N_secs(int pen,int N)
{
    int curr = getClk() + pen;
    if (N > 0)
    {
        /* The current tick was already counted before entering this wait.
         * Only account for the additional skipped ticks. */
        int skipped_ticks = (pen > 0) ? (pen - 1) : 0;
        N_time = (N_time + skipped_ticks) % N;
    }
    while (curr > getClk()){}
}

void create_log_files(FILE **log_file, FILE **perf_file)
{
    mkdir("../logs/", 0755);

    *log_file = fopen("../logs/scheduler.log", "w");
    *perf_file = fopen("../logs/scheduler.perf", "w");

    setvbuf(*log_file, NULL, _IONBF, 0);

    return;
}

void write_comment_line(FILE *log_file)
{
    char *comment = "#At\ttime\tx\tprocess\ty\tstate\tarr\tw\ttotal\tz\tremain\ty\twait\tk\n";
    fprintf(log_file, "%s", comment);
}

void log_data(FILE *log_file, PCB *pcb)
{
    char stateStr[100];
    bool finishFlag = 0;
    int log_time = getClk();

    // #At time x process y state arr w total z remain y wait k
    // time: getClk(), process: id, state: lState, arr: arrival, total: runtime, remain: remaining_time, wait: waiting_time

    switch (pcb->lState)
    {
    case START:
        strcpy(stateStr, "started");
        break;
    case FINISH:
        strcpy(stateStr, "finished");
        finishFlag = 1;
        if (pcb->finish_time != -1)
            log_time = pcb->finish_time;
        break;
    case STOP:
        strcpy(stateStr, "stopped");
        break;
    case RESUME:
        strcpy(stateStr, "resumed");
        break;
    default:
        strcpy(stateStr, "state");
        break;
    }

    fprintf(log_file, "At\ttime\t%d\tprocess\t%d\t%s\tarr\t%d\ttotal\t%d\tremain\t%d\twait\t%d", log_time, pcb->id, stateStr, pcb->arrival, pcb->runtime, pcb->remaining_time, pcb->waiting_time);
    if (finishFlag)
    {
        int TA = pcb->finish_time - pcb->arrival;
        float WTA = ((float)TA) / pcb->runtime;
        fprintf(log_file, "\tTA\t%d\tWTA\t%.2f", TA, WTA);
    }
    fprintf(log_file, "\n");
}

void write_perf(struct PerfVars perf, FILE* perf_file) {
    if (perf.num_procs == 0 || perf.finish_time <= perf.first_arrival)
    {
        fprintf(perf_file, "CPU utilization = 0.00%%\n");
        fprintf(perf_file, "Avg WTA = 0.00\n");
        fprintf(perf_file, "Avg Waiting = 0.00\n");
        fprintf(perf_file, "Std WTA = 0.00\n");
        return;
    }

    float cpu_util = (float)perf.total_runtime * 100.0 / (float)(perf.finish_time - perf.first_arrival);
    perf.avg_WTA /= perf.num_procs;
    perf.avg_Waiting /= perf.num_procs;

    float std_WTA = sqrtf(perf.M2_WTA / perf.num_procs);
    fprintf(perf_file, "CPU utilization = %.2f%%\n", cpu_util);
    fprintf(perf_file, "Avg WTA = %.2f\n", perf.avg_WTA);
    fprintf(perf_file, "Avg Waiting = %.2f\n", perf.avg_Waiting);
    fprintf(perf_file, "Std WTA = %.2f\n", std_WTA);
}


int create_2cpu_ipcs()
{
    /* --- msgq_sub1: main scheduler → sub-scheduler #1 --- */

    key_t key_sub1 = ftok(KEYFILE_PATH, MSGQ_SUB1_PROJ);
    msgq_sub1_id = msgget(key_sub1, 0666 | IPC_CREAT);
    if (msgq_sub1_id== -1)
    {
        perror("Error creating msgq_sub1");
        return -1;
    }

    /* --- msgq_sub2: main scheduler → sub-scheduler #2 --- */
    key_t key_sub2 = ftok(KEYFILE_PATH, MSGQ_SUB2_PROJ);
    msgq_sub2_id = msgget(key_sub2, 0666 | IPC_CREAT);
    if (msgq_sub2_id == -1)
    {
        perror("Error creating msgq_sub2");
        return -1;
    }

    /* --- msgq_response: sub-schedulers → main scheduler --- */
    key_t key_resp = ftok(KEYFILE_PATH, MSGQ_RESPONSE_PROJ);
    msgq_resp_id = msgget(key_resp, 0666 | IPC_CREAT);
    if (msgq_resp_id == -1)
    {
        perror("Error creating msgq_response");
        return -1;
    }

    /* Start each run from clean queues to avoid replaying stale messages. */
    clear_pending_msgs(msgq_sub1_id);
    clear_pending_msgs(msgq_sub2_id);
    clear_pending_msgs(msgq_resp_id);

    /* --- Load SHM: [count1, totalRT1, count2, totalRT2] --- */
    key_t key_shm = ftok(KEYFILE_PATH, LOAD_SHM_PROJ);
    load_shm_id = shmget(key_shm, LOAD_SHM_SIZE, 0666 | IPC_CREAT | IPC_EXCL);
    if (load_shm_id == -1)
    {
        if (errno == EEXIST || errno == EINVAL)
        {
            int old_id = shmget(key_shm, 1, 0666);
            if (old_id != -1)
                shmctl(old_id, IPC_RMID, NULL);

            load_shm_id = shmget(key_shm, LOAD_SHM_SIZE, 0666 | IPC_CREAT | IPC_EXCL);
        }

        if (load_shm_id == -1)
        {
            perror("Error creating Load SHM");
            return -1;
        }
    }
    load_shm_addr = (int *)shmat(load_shm_id, NULL, 0);
    if ((long)load_shm_addr == -1)
    {
        perror("Error attaching Load SHM");
        return -1;
    }


    /* Zero-initialize all 4 slots */
    (load_shm_addr)[LOAD_SHM_SLOT_COUNT1]   = 0;
    (load_shm_addr)[LOAD_SHM_SLOT_TOTALRT1] = 0;
    (load_shm_addr)[LOAD_SHM_SLOT_COUNT2]   = 0;
    (load_shm_addr)[LOAD_SHM_SLOT_TOTALRT2] = 0;

    key_t semKey = ftok(KEYFILE_PATH, LOAD_SEM_PROJ);

    load_sem_id = semget(semKey, 1, 0666 | IPC_CREAT |IPC_EXCL);
    if (load_sem_id == -1)
    {
        load_sem_id = semget(semKey, 1, 0666);
    }

    union Semun semun;
    semun.val = 1;
    if (semctl(load_sem_id, 0, SETVAL, semun) == -1)
    {
        perror("Error in semctl");
        exit(-1);
    }

    key_t thresholdSemKey = ftok(KEYFILE_PATH, THRESHOLD_SEM_PROJ);
    threshold_sem_id = semget(thresholdSemKey, 1, 0666 | IPC_CREAT | IPC_EXCL);
    if (threshold_sem_id == -1)
    {
        threshold_sem_id = semget(thresholdSemKey, 1, 0666);
    }

    semun.val = 0;
    if (semctl(threshold_sem_id, 0, SETVAL, semun) == -1)
    {
        perror("Error in threshold semctl");
        exit(-1);
    }

    return 0;
}
/*
 * Read BOTH CPUs' load info at once (used by main scheduler for comparison).
 */
void read_all_load_shm(int *load_shm_addr,
                       int *count1, int *totalRT1,
                       int *count2, int *totalRT2)
{
    down(load_sem_id);
    *count1   = load_shm_addr[LOAD_SHM_SLOT_COUNT1];
    *totalRT1 = load_shm_addr[LOAD_SHM_SLOT_TOTALRT1];
    *count2   = load_shm_addr[LOAD_SHM_SLOT_COUNT2];
    *totalRT2 = load_shm_addr[LOAD_SHM_SLOT_TOTALRT2];
    up(load_sem_id);
}

void destroy_2cpu_ipcs()
{
    /* Remove the 3 message queues */
    if (msgq_sub1_id != -1)
        msgctl(msgq_sub1_id, IPC_RMID, NULL);
    if (msgq_sub2_id != -1)
        msgctl(msgq_sub2_id, IPC_RMID, NULL);
    if (msgq_resp_id != -1)
        msgctl(msgq_resp_id, IPC_RMID, NULL);

    /* Detach and remove the Load SHM */
    if (load_shm_addr != NULL && (long)load_shm_addr != -1)
    {
        shmdt(load_shm_addr);
        key_t key_shm = ftok(KEYFILE_PATH, LOAD_SHM_PROJ);
        int shm_id = shmget(key_shm, LOAD_SHM_SIZE, 0666);
        if (shm_id != -1)
            shmctl(shm_id, IPC_RMID, NULL);
    }

    if (threshold_sem_id != -1)
    {
        semctl(threshold_sem_id, 0, IPC_RMID);
        threshold_sem_id = -1;
    }
}

int select_cpu()
{
    int c1,c2,rt1,rt2;
    read_all_load_shm(load_shm_addr, &c1, &rt1, &c2, &rt2);
    if (c1<=c2)
        return 1;
    else
        return 2;

}

int send_process_msg(int msgq_id, processData *p, long mtype)
{
    p->mtype = mtype;
    if (msgsnd(msgq_id, p, sizeof(processData) - sizeof(long), 0) == -1)
    {
        perror("Error in send_process_msg");
        return -1;
    }
    return 0;
}


processData pcb_to_processData(PCB *pcb)
{
    processData p;
    p.mtype = MTYPE_STEAL_RESPONSE;
    p.id = pcb->id;
    p.arrival = pcb->arrival;
    p.runtime = pcb->runtime;
    p.priority = pcb->priority;
    return p;
}

void check_threshold(int M,int N)
{
    int c1, c2, rt1, rt2;

    read_all_load_shm(load_shm_addr, &c1, &rt1, &c2, &rt2);
    int diff = abs(rt1 - rt2);

    /* No snapshot signal: use a second synchronous read as post-refresh view. */
    read_all_load_shm(load_shm_addr, &c1, &rt1, &c2, &rt2);
    diff = abs(rt1 - rt2);

    while (diff > M)
    {
        int busier_idx, lighter_msgq;
        if (rt1 > rt2)
        {
            busier_idx = 0;            /* idArr[0] = CPU1 */
            lighter_msgq = msgq_sub2_id; /* send stolen process to CPU2 */
        }
        else
        {
            busier_idx = 1;            /* idArr[1] = CPU2 */
            lighter_msgq = msgq_sub1_id; /* send stolen process to CPU1 */
        }

        /* 1. Signal busier sub-scheduler to steal its rear process */
        kill(idArr[busier_idx], SIGURG);
        

        /* 2. Wait for response (blocking) */
        processData resp;
        if (msgrcv(msgq_resp_id, &resp, sizeof(processData) - sizeof(long), 0, 0) == -1)
        {
            perror("msgrcv steal response");
            return;
        }


        /* 3. If nothing to steal (queue was empty), stop */
        if (resp.mtype == 12)
            return;

        /* 4. Send stolen process to the lighter CPU */
        send_process_msg(lighter_msgq, &resp, MTYPE_NEW_PROCESS);

        /* Reflect the steal transfer immediately in load SHM so the very next
         * threshold iteration does not operate on stale counts/RT totals. */
        int moved_rt = resp.runtime;
        if (busier_idx == 0)
        {
            if (c1 > 0)
                c1--;
            rt1 -= moved_rt;
            if (rt1 < 0)
                rt1 = 0;
            c2++;
            rt2 += moved_rt;
        }
        else
        {
            if (c2 > 0)
                c2--;
            rt2 -= moved_rt;
            if (rt2 < 0)
                rt2 = 0;
            c1++;
            rt1 += moved_rt;
        }

        down(load_sem_id);
        load_shm_addr[LOAD_SHM_SLOT_COUNT1] = c1;
        load_shm_addr[LOAD_SHM_SLOT_TOTALRT1] = rt1;
        load_shm_addr[LOAD_SHM_SLOT_COUNT2] = c2;
        load_shm_addr[LOAD_SHM_SLOT_TOTALRT2] = rt2;
        up(load_sem_id);

        /* 5. Stall both CPUs for 3-second overhead */
        kill(idArr[0], SIGUSR2);
        kill(idArr[1], SIGUSR2);
        wait_N_secs(3, N);
        
        /* 6. Re-read and check again */
        
        read_all_load_shm(load_shm_addr, &c1, &rt1, &c2, &rt2);
        diff = abs(rt1 - rt2);
    }

}

int receiveFirstProcess(Queue *readyQueue,processData process,int type)
{
    if (msgrcv(msgq_id, &process, sizeof(processData) - sizeof(long), 0,0) != -1)
      {

        if (process.mtype == 5)
        {
          receivingProcesses = 0;
          return -1;
        }

        struct PCB *pcb = (struct PCB *)malloc(sizeof(struct PCB));
        *pcb = createPCB(process);


        // we need the first arrival to calculate CPU utilization (= Finish - first_arrival / total_runtime)
        if(perf.first_arrival == -1) {
          perf.first_arrival = pcb->arrival;
        }
        
        enqueue(readyQueue, pcb);
        
          return 0;
      }
      else return -1;
}

int receiveProcesses(Queue *readyQueue,processData process,int type)
{
    if (msgrcv(msgq_id, &process, sizeof(processData) - sizeof(long), 0,
                    IPC_NOWAIT) != -1)
      {

        if (process.mtype == 5)
        {
          receivingProcesses = 0;
          return -1;
        }

        struct PCB *pcb = (struct PCB *)malloc(sizeof(struct PCB));
        *pcb = createPCB(process);


        // we need the first arrival to calculate CPU utilization (= Finish - first_arrival / total_runtime)
        if(perf.first_arrival == -1) {
          perf.first_arrival = pcb->arrival;
        }
        if (type == 2) // HPF
        {
            enqueue_priority(readyQueue, pcb);
        }
        else
        {
            enqueue(readyQueue, pcb);
        }
          return 0;
      }
      else return -1;
}