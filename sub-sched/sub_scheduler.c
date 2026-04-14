#include "../headers.h"
#include "sub_scheduler.h"
#include <string.h>

// function predefinitions
void stall_sig(int signum); // This function needs a global variable stalled, so it can't be in sub_scheduler.h
void onProcessFinished(int signum);
void update_load_shm(void);
static processData pcb_to_processData(PCB *pcb, long mtype);
void steal_handler(int signum);


// global vars

int cpu_id;
int sem_id;       // tick-gate semaphore (ftok 81 or 82)
int shmRT_id;     // remaining-time shm  (ftok 83 or 84)
int *shmRT_addr;  // mapped address
int my_msgq_id;  // main → this sub (ftok 75 or 76)
int msgq_resp_id; // this sub → main (ftok 77)
int *load_shm;    // [count1,totalRT1,count2,totalRT2] (ftok 80)

int sem_proj; //sem key sent by the main 1-81 , 2-82
int shmRT_proj; //shm key for remaining time sent by main 1-83 , 2-84

// stall
int stalled = 0;
int stall_end_time = -1;

// process
Queue *readyQueue;
int processFinishedSignal = 0;
int receivingProcesses = 1;
struct PCB *currProcess = NULL;
int dispatched_this_tick = 0;

// logs
FILE *log_file;
FILE *perf_file;

int main(int argc, char *argv[])
{
    signal(SIGUSR2, stall_sig);
    signal(SIGUSR1, onProcessFinished);
    signal(SIGURG, steal_handler);

    // we need to choose another signal for the steal command, because SIGUSR1 is used for the process finished signal, and we need to make sure that the steal command signal handler will not interfere with the process finished signal handler.
    if (argc < 2)
    {
        fprintf(stderr, "Usage: sub_scheduler <cpu_id>\n");
        return 1;
    }

    /*
    1-it will take a the address from the msgq
    2-push it to the ready queue
    3- call the function (FCFS)

    4-implement the signal handler for the stall signal, and make sure that the scheduler will not schedule any process during the stall time, and after the stall time is over, it will resume scheduling the processes in the ready queue.
    5-implement the signal handler if the main scheduler needds the last element of us

    6-signal handler to notify that the process has finished .


    */
    // We need to include which cpu is running in args to write to log1 | log2 / perf1 | perf2


    to_int(argv[1], &cpu_id);

    create_ipcs(cpu_id);

    readyQueue = createQueue();
    if (attach_2cpu_ipcs(cpu_id) == -1)
    {
        return 1;
    }

    create_log_files(&log_file, &perf_file, cpu_id);
    struct PerfVars perf = initialize_perf();

    // Assume address msgq exists and get a dummy address for now
    initClk();

    int last_tick = -1;

    while (!isEmpty(readyQueue) || receivingProcesses || currProcess)
    {
        int now = getClk();

        if (now == last_tick)
            continue;
        int base = (cpu_id == 1) ? 0 : 2;
        printf("I am cpu %d and i have %d at time %d\n", cpu_id, load_shm[base], now);
        last_tick = now;

        if (stalled && now >= stall_end_time)
        {
            stalled = 0;
        }

        if (currProcess && processFinishedSignal)
        {
            processFinishedSignal = 0;
            int status;
            while (waitpid(currProcess->pid, &status, 0) == -1)
                if (errno != EINTR)
                {
                    perror("waitpid");
                    break;
                }

            currProcess->finish_time = now;
            currProcess->remaining_time = 0;
            currProcess->state = 'F';
            currProcess->lState = FINISH;
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
        else if (!processFinishedSignal)
        {
            processData pd;
            while (msgrcv(my_msgq_id, &pd, sizeof(processData) - sizeof(long),
                          0, IPC_NOWAIT) != -1)
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
                    enqueue(readyQueue, pcb);
                }
            }

            if(!stalled)
                FCFS_algo(readyQueue, &currProcess, log_file);

            if (currProcess && !dispatched_this_tick && !stalled)
            {
                union Semun s;
                s.val = 0;
                semctl(sem_id, 0, SETVAL, s);
                up(sem_id);
            }

            dispatched_this_tick = 0;
        }
        update_load_shm();
    }
    printf("sub %d finished--------------------------\n", cpu_id);
    update_load_shm();

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

    stalled = 1;
    stall_end_time = getClk() + 3;
    return;
}

void steal_handler(int signum)
{
    (void)signum;

    // this sends the rear of the ready queue to the main scheduler.
    //send the rear through the msgq_resp_id
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
    processFinishedSignal = 1;
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
    int count = readyQueue->size + (currProcess != NULL ? 1 : 0);
    int totalRT = total_remaining_time(readyQueue) + (currProcess != NULL ? *shmRT_addr : 0);
    load_shm[base] = count;
    load_shm[base + 1] = totalRT;
}
