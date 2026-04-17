#include "sub_scheduler.h"
#include "../headers.h"
#include <string.h>

// function predefinitions
void stall_sig(int signum); // This function needs a global variable stalled, so it can't be in sub_scheduler.h
void onProcessFinished(int signum);
void update_load_shm(void);
static processData pcb_to_processData(PCB *pcb, long mtype);
void steal_handler(int signum);
void dump_local_processes(int signum);
static int get_running_remaining_time(void);
static int try_down_nonblocking(int sem);
static void refresh_load_shm_nonblocking(void);

// global vars

int load_sem_id;
int threshold_sem_id = -1;
int cpu_id;
int sem_id;       // tick-gate semaphore (ftok 81 or 82)
int shmRT_id;     // remaining-time shm  (ftok 83 or 84)
int *shmRT_addr;  // mapped address
int my_msgq_id;   // main → this sub (ftok 75 or 76)
int msgq_resp_id; // this sub → main (ftok 77)

int current_tick_time = 0;
int *load_shm;    // [count1,totalRT1,count2,totalRT2] (ftok 80)

int sem_proj;   // sem key sent by the main 1-81 , 2-82
int shmRT_proj; // shm key for remaining time sent by main 1-83 , 2-84

// stall
int stalled = 0;
int stall_end_time = -1;

// process
Queue *readyQueue;

int receivingProcesses = 1;
struct PCB *currProcess = NULL;
int context_switch_until = -1;
int dispatched_this_tick = 0;
int pass = 0;

// logs
FILE *log_file;
FILE *perf_file;
struct PerfVars perf;

int main(int argc, char *argv[])
{
    signal(SIGINT, cleanup);
    signal(SIGUSR2, stall_sig);
    signal(SIGUSR1, onProcessFinished);
    signal(SIGURG, steal_handler);

    // we need to choose another signal for the steal command, because SIGUSR1 is used for the process finished signal, and we need to make sure that the steal command signal handler will not interfere with the process finished signal handler.
    if (argc < 2)
    {
        fprintf(stderr, "Usage: sub_scheduler <cpu_id>\n");
        return 1;
    }

    // We need to include which cpu is running in args to write to log1 | log2 / perf1 | perf2

    to_int(argv[1], &cpu_id);

    create_ipcs(cpu_id);

    readyQueue = createQueue();
    if (attach_2cpu_ipcs(cpu_id) == -1)
    {
        return 1;
    }

    create_log_files(&log_file, &perf_file, cpu_id);
    perf = initialize_perf();

    // Assume address msgq exists and get a dummy address for now
    initClk();

    int last_tick = -1;
    int base2nd = (cpu_id == 1) ? 2 : 0; // don't terminate untill all the processes are done
    
    // printf("\tSUB %d remaining_processes init DOWN\n", cpu_id);
    down(load_sem_id);
    int remaining_processes = load_shm[base2nd + 1];
    up(load_sem_id);
    // printf("\tSUB %d remaining_processes init UP\n", cpu_id);
    while (!isEmpty(readyQueue) || receivingProcesses || currProcess || remaining_processes)
    {
        // Always drain the message queue immediately (not just once per tick)
        processData pd;
        while (msgrcv(my_msgq_id, &pd, sizeof(processData) - sizeof(long), 0, IPC_NOWAIT) != -1)
        {
            if (pd.mtype == 5)
            {
                receivingProcesses = 0;
                break;
            }
            if (pd.mtype == 1)
            {
                PCB *pcb = malloc(sizeof(PCB));
                *pcb = createPCB(pd);
                if (perf.first_arrival == -1)
                    perf.first_arrival = pcb->arrival;
                // printf("iam cpu %d and i recieved proceess %d-----------------------\n", cpu_id, pcb->id);
                enqueue(readyQueue, pcb);
                // if (!stalled)
                //     FCFS_algo(readyQueue, &currProcess, log_file);
            }
        }

        int now = getClk();
        current_tick_time = now;
        if (now == last_tick)
        {
            // Still update load_shm for monitoring
            update_load_shm();
            continue;
        }

        /* Threshold gate: consume one permit for this tick.
         * Main posts permits only after finishing threshold decision. */
        down(threshold_sem_id);
        last_tick = now;


        if (stalled && now >= stall_end_time)
        {
            stalled = 0;
            //////////////////////
            if (currProcess != NULL && currProcess->pid != -1)
            {
                if(currProcess->last_stopped >= 0)
                    currProcess->waiting_time += (current_tick_time - currProcess->last_stopped);
                currProcess->lState = RESUME;
                log_data(log_file, currProcess);
                kill(currProcess->pid, SIGCONT);
                currProcess->state = 'R';
            }
        }

      // print the while variables
    //   printf("cpu %d at time %d: currProcess=%d, readyQueue_size=%d, receivingProcesses=%d, stalled=%d\n",
    //          cpu_id, now, currProcess ? currProcess->id : -1, readyQueue->size, receivingProcesses, stalled);

        if (!currProcess && !isEmpty(readyQueue) && !stalled &&
            (context_switch_until == -1 || now >= context_switch_until))
        {
            // Dispatch next process when CPU becomes idle.
            FCFS_algo(readyQueue, &currProcess, log_file);
            context_switch_until = -1;
        }

        if (currProcess && !stalled)
        {
            // Grant exactly one tick of execution to the running process.
            union Semun s;
            s.val = 0;
            semctl(sem_id, 0, SETVAL, s);
            up(sem_id);
        }
        update_load_shm();
        // printf("\tSUB %d remaining_processes loop DOWN\n", cpu_id);
        down(load_sem_id);
        remaining_processes = load_shm[base2nd + 1];
        up(load_sem_id);
        // printf("\tSUB %d remaining_processes loop UP\n", cpu_id);
    }
    printf("\tSUB %d finished main loop! Exiting cleanly.\n", cpu_id);
    update_load_shm();
    
    processData ack;
    memset(&ack, 0, sizeof(ack));
    ack.mtype = 6;
    msgsnd(msgq_resp_id, &ack, sizeof(processData) - sizeof(long), 0);


    write_perf(perf, perf_file);
    fclose(log_file);
    fclose(perf_file);

    shmdt(shmRT_addr);
    shmdt(load_shm);
    semctl(sem_id, 0, IPC_RMID);
    shmctl(shmRT_id, IPC_RMID, NULL);

    destroyClk(false);
    return 0;
}

void stall_sig(int signum)
{
    (void)signum;
    
    processData pd;
    ///////////////////
    printf("\tSUB %d STALL SIGNALRECEIVED at %d\n", cpu_id, getClk());

    if (!stalled && currProcess != NULL && currProcess->pid != -1)
    {
        currProcess->last_stopped = current_tick_time;
        kill(currProcess->pid, SIGSTOP);
        currProcess->state = 'W';
        currProcess->lState = STOP;
        log_data(log_file, currProcess);
    }

    stalled = 1;
    while (msgrcv(my_msgq_id, &pd, sizeof(processData) - sizeof(long), 0, IPC_NOWAIT) != -1)
        {
            if (pd.mtype == 5)
            {
                receivingProcesses = 0;
                break;
            }
            if (pd.mtype == 1)
            {
                PCB *pcb = malloc(sizeof(PCB));
                *pcb = createPCB(pd);
                if (perf.first_arrival == -1)
                    perf.first_arrival = pcb->arrival;
                // printf("iam cpu %d and i recieved proceess %d-----------------------\n", cpu_id, pcb->id);
                enqueue(readyQueue, pcb);
                
            }
        }
    stall_end_time = current_tick_time + 3;
    return;
}

void steal_handler(int signum)
{
    (void)signum;

    processData pd;
    while (msgrcv(my_msgq_id, &pd, sizeof(processData) - sizeof(long), 0, IPC_NOWAIT) != -1)
    {
        if (pd.mtype == 5)
        {
            receivingProcesses = 0;
            break;
        }
        else if (pd.mtype == 1)
        {
            struct PCB *pcb = (struct PCB *)malloc(sizeof(struct PCB));
            *pcb = createPCB(pd);
            enqueue(readyQueue, pcb);
        }
    }

    // this sends the rear of the ready queue to the main scheduler.
    // send the rear through the msgq_resp_id
    PCB *stolen = dequeue_rear(readyQueue);
    processData resp;
    if (stolen)
    {
        stolen->lState = STOLEN;
        log_data(log_file, stolen);
        resp = pcb_to_processData(stolen, 11);
        free(stolen);
    }
    else
    {
        /* Nothing to steal — running process can't be taken */
        memset(&resp, 0, sizeof(resp));
        resp.mtype = 12;
    }
    if (msgsnd(msgq_resp_id, &resp,
               sizeof(processData) - sizeof(long), 0) == -1)
        perror("msgsnd steal resp");
}

void onProcessFinished(int signum)
{
    (void)signum;
    // printf("cpu %d received process finished signal at time %d\n", cpu_id, getClk() + 1);
    int status;
    while (waitpid(currProcess->pid, &status, 0) == -1)
        if (errno != EINTR)
        {
            perror("waitpid");
            break;
        }

    // Remaining time reaches zero during this tick; completion is at end of tick.
    currProcess->finish_time = current_tick_time + 1;
    currProcess->remaining_time = 0;
    currProcess->state = 'F';
    currProcess->lState = FINISH;
    context_switch_until = currProcess->finish_time + 1;
    log_data(log_file, currProcess);

    float WTA = (float)(currProcess->finish_time - currProcess->arrival) / (float)currProcess->runtime;
    perf.avg_WTA += WTA;
    perf.num_procs++;
    float delta = WTA - perf.welford_mean_WTA;
    perf.welford_mean_WTA += delta / perf.num_procs;
    float delta2 = WTA - perf.welford_mean_WTA;
    perf.M2_WTA += delta * delta2;
    perf.avg_Waiting += currProcess->waiting_time;
    perf.total_runtime += currProcess->runtime;
    perf.finish_time = currProcess->finish_time;

    free(currProcess);
    currProcess = NULL;
}

static processData pcb_to_processData(PCB *pcb, long mtype)
{
    processData pd;
    pd.mtype = mtype;
    pd.id = pcb->id;
    pd.arrival = pcb->arrival;
    pd.runtime = pcb->runtime;
    pd.priority = pcb->priority;
    return pd;
}

void update_load_shm(void)
{
    /* slots: cpu 1 → indices 0,1 ; cpu 2 → indices 2,3 */
    int base = (cpu_id == 1) ? 0 : 2;
    int running_rt = 0;

    if (currProcess != NULL)
    {
        /* Remaining time is advanced by the child process in shm each tick. */
        running_rt = get_running_remaining_time();
        if (running_rt < 0)
            running_rt = 0;
    }

    int count = readyQueue->size;
    int totalRT = total_remaining_time(readyQueue) + running_rt;
    down(load_sem_id);
    load_shm[base] = count;
    load_shm[base + 1] = totalRT;
    up(load_sem_id);
}

static int get_running_remaining_time(void)
{
    if (currProcess == NULL)
        return 0;

    int rt = currProcess->remaining_time;
    if (shmRT_addr != NULL && (long)shmRT_addr != -1)
        rt = *shmRT_addr;

    return rt;
}

static int try_down_nonblocking(int sem)
{
    struct sembuf op;
    op.sem_num = 0;
    op.sem_op = -1;
    op.sem_flg = IPC_NOWAIT;

    while (semop(sem, &op, 1) == -1)
    {
        if (errno == EINTR)
            continue;
        return -1;
    }

    return 0;
}

static void refresh_load_shm_nonblocking(void)
{
    if (readyQueue == NULL || load_shm == NULL || (long)load_shm == -1)
        return;

    int base = (cpu_id == 1) ? 0 : 2;
    int running_rt = 0;
    if (currProcess != NULL)
    {
        running_rt = get_running_remaining_time();
        if (running_rt < 0)
            running_rt = 0;
    }

    int count = readyQueue->size;
    int totalRT = total_remaining_time(readyQueue) + running_rt;

    /* Signal handlers must never block while trying to refresh counters. */
    if (try_down_nonblocking(load_sem_id) == 0)
    {
        load_shm[base] = count;
        load_shm[base + 1] = totalRT;
        up(load_sem_id);
    }
}

void dump_local_processes(int signum)
{
    (void)signum;

    /* Best-effort refresh; never block in this signal handler. */
    refresh_load_shm_nonblocking();

    printf("\n[SUB %d] -------- Threshold Snapshot --------\n", cpu_id);

    if (currProcess != NULL)
    {
        int running_rt = get_running_remaining_time();
        if (running_rt < 0)
            running_rt = 0;
        printf("[SUB %d] RUNNING: id=%d, remaining=%d\n", cpu_id, currProcess->id, running_rt);
    }
    else
        printf("[SUB %d] RUNNING: none\n", cpu_id);

    if (readyQueue == NULL || isEmpty(readyQueue))
    {
        printf("[SUB %d] READY QUEUE: empty\n", cpu_id);
        return;
    }

    printf("[SUB %d] READY QUEUE:\n", cpu_id);
    Node *it = readyQueue->front;
    while (it != NULL)
    {
        printf("[SUB %d]   id=%d, remaining=%d\n", cpu_id, it->pcb->id, it->pcb->remaining_time);
        it = it->next;
    }
}
