#include "../data_structures/PCB/Sch_PCB.h"
#include "sub_scheduler.h"

#include <errno.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <unistd.h>

#define MSGQ_SUB1_PROJ 75
#define MSGQ_SUB2_PROJ 76
#define MSGQ_RESPONSE_PROJ 77
#define LOAD_SHM_PROJ 80
#define LOAD_SEM_PROJ 85

#define Tick_semaphore1 81
#define Tick_semaphore2 82
#define Shm_Rt1 83
#define Shm_Rt2 84

#define LOAD_SHM_SIZE (4 * sizeof(int))
#define LOAD_SHM_SLOT_COUNT1 0
#define LOAD_SHM_SLOT_TOTALRT1 1
#define LOAD_SHM_SLOT_COUNT2 2
#define LOAD_SHM_SLOT_TOTALRT2 3

#define KEYFILE_PATH "../keyFile"

int getClk(void);

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

struct PerfVars initialize_perf()
{
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

void create_log_files(FILE **log_file, FILE **perf_file, int which)
{
    mkdir("../logs/", 0755);

    char log_path[64], perf_path[64];
    snprintf(log_path, sizeof(log_path), "../logs/scheduler_%d.log", which);
    snprintf(perf_path, sizeof(perf_path), "../logs/scheduler_%d.perf", which);

    *log_file = fopen(log_path, "w");
    *perf_file = fopen(perf_path, "w");

    write_comment_line(*log_file);
}

void FCFS_algo(Queue *readyQueue, struct PCB **currProcess, FILE *log_file)
{
    if (*currProcess == NULL && !isEmpty(readyQueue))
    {
        int size = readyQueue->size;
            printf("we got the point \n");
            printf("cpu %d , size %d\n", cpu_id, size);
        *currProcess = dequeue(readyQueue);
        int totalRT = total_remaining_time(readyQueue);
        size = readyQueue->size;
        printf("cpu %d , size %d , RT %d\n", cpu_id, size, totalRT);
        printf("we out of the point \n");

        write_load_shm(load_shm, cpu_id, size, totalRT);
        runProcess(*currProcess, log_file);
    }
}

void write_comment_line(FILE *log_file)
{
    char *comment = "#At\ttime\tx\tprocess\ty\tstate\tarr\tw\ttotal\tz\tremain\ty\twait\tk\n";
    fprintf(log_file, "%s", comment);
}

void log_data(FILE *log_file, PCB *pcb)
{
    char stateStr[100];
    int finishFlag = 0;

    // Special case for 2 CPU's
    if(pcb->lState == STOLEN) {
        fprintf(log_file, "At\ttime\t%d\tprocess\t%d\twas\tstolen\n", getClk(), pcb->id);
        return;
    }

    switch (pcb->lState)
    {
    case START:
        strcpy(stateStr, "started");
        break;
    case FINISH:
        strcpy(stateStr, "finished");
        finishFlag = 1;
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

    fprintf(log_file,
            "At\ttime\t%d\tprocess\t%d\t%s\tarr\t%d\ttotal\t%d\tremain\t%d\twait\t%d",
            getClk(), pcb->id, stateStr, pcb->arrival, pcb->runtime,
            pcb->remaining_time, pcb->waiting_time);

    if (finishFlag)
    {
        int TA = pcb->finish_time - pcb->arrival;
        float WTA = ((float)TA) / pcb->runtime;
        fprintf(log_file, "\tTA\t%d\tWTA\t%.2f", TA, WTA);
    }
    fprintf(log_file, "\n");
}

void write_perf(struct PerfVars perf, FILE *perf_file)
{
    if (perf.num_procs == 0 || perf.finish_time <= perf.first_arrival)
    {
        fprintf(perf_file, "CPU utilization = 0.00%%\n");
        fprintf(perf_file, "Avg WTA = 0.00\n");
        fprintf(perf_file, "Avg Waiting = 0.00\n");
        fprintf(perf_file, "Std WTA = 0.00\n");
        return;
    }

    float cpu_util = (float)perf.total_runtime * 100.0f /
                     (float)(perf.finish_time - perf.first_arrival);
    perf.avg_WTA /= perf.num_procs;
    perf.avg_Waiting /= perf.num_procs;

    float std_WTA = 0.0f;
    if (perf.num_procs > 1)
        std_WTA = sqrtf(perf.M2_WTA / (perf.num_procs - 1));

    fprintf(perf_file, "CPU utilization = %.2f%%\n", cpu_util);
    fprintf(perf_file, "Avg WTA = %.2f\n", perf.avg_WTA);
    fprintf(perf_file, "Avg Waiting = %.2f\n", perf.avg_Waiting);
    fprintf(perf_file, "Std WTA = %.2f\n", std_WTA);
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

int attach_2cpu_ipcs(int cpu_id)
{
    int proj_id = (cpu_id == 1) ? MSGQ_SUB1_PROJ : MSGQ_SUB2_PROJ;
    key_t key_sub = ftok(KEYFILE_PATH, proj_id);
    my_msgq_id = msgget(key_sub, 0666);
    if (my_msgq_id == -1)
    {
        perror("Sub-scheduler: Error attaching to my msgq");
        return -1;
    }

    key_t key_resp = ftok(KEYFILE_PATH, MSGQ_RESPONSE_PROJ);
    msgq_resp_id = msgget(key_resp, 0666);
    if (msgq_resp_id == -1)
    {
        perror("Sub-scheduler: Error attaching to response msgq");
        return -1;
    }

    key_t key_shm = ftok(KEYFILE_PATH, LOAD_SHM_PROJ);
    int load_shm_id = shmget(key_shm, LOAD_SHM_SIZE, 0666);
    if (load_shm_id == -1)
    {
        perror("Sub-scheduler: Error attaching to Load SHM");
        return -1;
    }

    load_shm = (int *)shmat(load_shm_id, NULL, 0);
    if ((long)load_shm == -1)
    {
        perror("Sub-scheduler: Error mapping Load SHM");
        return -1;
    }

    key_t semKey = ftok(KEYFILE_PATH, LOAD_SEM_PROJ);
    load_sem_id = semget(semKey, 1, 0666);
    if (load_sem_id == -1)
    {
        perror("Error in retrieve sem");
        exit(-1);
    }

    

    return 0;
}

void write_load_shm(int *load_shm_addr, int cpu_id, int count, int totalRT)
{
    if (cpu_id == 1)
    {
        down(load_sem_id);
        load_shm_addr[LOAD_SHM_SLOT_COUNT1] = count;
        load_shm_addr[LOAD_SHM_SLOT_TOTALRT1] = totalRT;
        up(load_sem_id);
    }
    else
    {
        down(load_sem_id);
        load_shm_addr[LOAD_SHM_SLOT_COUNT2] = count;
        load_shm_addr[LOAD_SHM_SLOT_TOTALRT2] = totalRT;
        up(load_sem_id);
    }
}

void runProcess(struct PCB *pcb, FILE *log_file)
{
    *shmRT_addr = pcb->remaining_time;
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
            snprintf(shm_str, sizeof(shm_str), "%d", shmRT_id);
            snprintf(sem_str, sizeof(sem_str), "%d", sem_id);
            execl("../outFiles/process.out", "process.out", runtime_str, shm_str, sem_str,
                  (char *)NULL);
            perror("execl failed");
            _exit(1);
        }

        pcb->pid = pid;
    }
    else
    {
        pcb->lState = RESUME;
        log_data(log_file, pcb);
        kill(pcb->pid, SIGCONT);
    }

    pcb->state = 'R';
}

void create_ipcs(int cpu_id)
{
    int semKey;
    int shmKey;

    if (cpu_id == 1)
    {
        semKey = ftok(KEYFILE_PATH,Tick_semaphore1);
        shmKey = ftok(KEYFILE_PATH, Shm_Rt1);
    }
    else
    {
        semKey = ftok(KEYFILE_PATH,Tick_semaphore2);
        shmKey = ftok(KEYFILE_PATH, Shm_Rt2);
    }

    shmRT_id = shmget(shmKey, 4, IPC_CREAT | 0666);
    if (shmRT_id == -1)
    {
        perror("Error in creating remaining time shm");
        exit(-1);
    }

    shmRT_addr = (int *)shmat(shmRT_id, (void *)0, 0);
    if ((long)shmRT_addr == -1)
    {
        perror("Error in attaching the shm of RT");
        exit(-1);
    }

    sem_id = semget(semKey, 1, 0666 | IPC_CREAT);
    if (sem_id == -1)
    {
        perror("Error in create sem");
        exit(-1);
    }

    union Semun semun;
    semun.val = 0;
    if (semctl(sem_id, 0, SETVAL, semun) == -1)
    {
        perror("Error in semctl");
        exit(-1);
    }


}

void cleanup(int signum)
{
    (void)signum;
    
    shmdt(shmRT_addr);
    shmdt(load_shm);
    semctl(sem_id, 0, IPC_RMID);
    shmctl(shmRT_id, IPC_RMID, NULL);
    exit(0);
}