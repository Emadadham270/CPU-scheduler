#include "rr_scheduler.h"


int msgq_id;
int req_msgq;
int sem_id;
int receivingProcesses = 1;
Queue *readyQueue;
Queue *currentPCBs;

struct PCB *currProcess = NULL;
int quantum;
int k = 0;
int quantums_passed = 0;
int processFinishedSignal = 0;
int next_preemtion_time = -1;
int *shmRT_addr;
int shmRT_id;
int context_switch_until = -1;
perfVars perf;
void onProcessFinished(int signum)
{
    (void)signum;
    processFinishedSignal = 1;
}
int lag = 0;
int main(int argc, char *argv[])
{
    (void)argc;
    key_t key_id,req_key_id;
    signal(SIGINT, cleanup);
    signal(SIGUSR1, onProcessFinished);
    key_id = ftok("../keyFile", 65);
    req_key_id = ftok("../keyFile", 70);
    msgq_id = msgget(key_id, 0666);
    req_msgq = msgget(req_key_id, 0666|IPC_CREAT);
    perf = initialize_perf();

    shmRT_id = shmget(ftok("../keyfile", 70), 4, IPC_CREAT | 0666);
    if ((long)shmRT_id == -1)
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

    sem_id = semget(ftok("../keyfile", 66), 1, 0666 | IPC_CREAT);
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

    if (msgq_id == -1 || req_msgq == -1)
    {
        perror("Error in receive message queue ========");
        exit(-1);
    }
    readyQueue = createQueue();
    char *e;
    quantum = strtol(argv[1], &e, 10);
    char *e2;
    k = strtol(argv[2], &e2, 10);

    FILE *log_file, *perf_file;
    create_log_files(&log_file, &perf_file);
    write_comment_line(log_file);

    int last_tick = -1;

    initClk();

    while (!isEmpty(readyQueue) || receivingProcesses || currProcess)
    {

        int now = getClk();
        if (now == last_tick)
            continue; // spin until next tick

        last_tick = now;

        // 0. receive requests for the memory
        //---------- may change if we will put it at the end of the while loop and refuse req logic will be added then -------------
        handleRequests(&lag);

        // 1. Handle finished process (before algo, so we don't preempt a dead process)
        if (currProcess != NULL && processFinishedSignal)
        {
            int status;
            processFinishedSignal = 0;

            while (waitpid(currProcess->pid, &status, 0) == -1)
            {
                if (errno != EINTR)
                {
                    perror("waitpid failed");
                    break;
                }
            }

            currProcess->finish_time = getClk();
            currProcess->remaining_time = 0;
            currProcess->state = 'F';
            context_switch_until = currProcess->finish_time + 1;

            currProcess->remaining_time = *shmRT_addr;
            // log data to scheduler.log
            currProcess->lState = FINISH;
            log_data(log_file, currProcess);

            // Add WTA and Waiting to perf struct
            float WTA = (float)(currProcess->finish_time - currProcess->arrival) / (float)currProcess->runtime;
            perf.avg_WTA += WTA;

            // perform rolling standard deviation
            // https://en.wikipedia.org/wiki/Algorithms_for_calculating_variance#Welford's_online_algorithm
            perf.num_procs++;
            float delta = WTA - perf.welford_mean_WTA;
            perf.welford_mean_WTA += delta / perf.num_procs;
            float delta2 = WTA - perf.welford_mean_WTA;
            perf.M2_WTA += delta * delta2;

            perf.avg_Waiting += currProcess->waiting_time;
            perf.total_runtime += currProcess->runtime;
            perf.finish_time = currProcess->finish_time;
            dequeue_by_id(currentPCBs, currProcess->id);
            free(currProcess);
            currProcess = NULL;
            next_preemtion_time = -1;
        }
        if (context_switch_until == -1 || now >= context_switch_until)
        {
            context_switch_until = -1;
            // 2. Receive new arrivals synchronously with process_generator
            if (receivingProcesses)
            {
                while (1)
                {
                    processData msg;
                    if (msgrcv(msgq_id, &msg, sizeof(processData) - sizeof(long), -5, 0) != -1)
                    {
                        if (msg.mtype == 1)
                        {
                            struct PCB *pcb = (struct PCB *)malloc(sizeof(struct PCB));
                            *pcb = createPCB(msg);
                            // add the logic of the first arrival

                            pcb->frame_index = -1;
                            if (perf.first_arrival == -1)
                                perf.first_arrival = pcb->arrival;
                            enqueue(readyQueue, pcb);
                            enqueue(currentPCBs,pcb);
                        }
                        else if (msg.mtype == 2)
                        {
                            if (msg.arrival >= now)
                            {
                                break;
                            }
                        }
                        else if (msg.mtype == 5)
                        {
                            receivingProcesses = 0;
                            break;
                        }
                    }
                    else
                    {
                        if (errno != EINTR)
                        {
                            perror("msgrcv error");
                            break;
                        }
                    }
                }
            }

            // 3. Run scheduling algorithm (preempt → re-enqueue → dispatch)
            if(lag)
            {
                lag = 0;
            }else if(now > 0)
                RR_algo(readyQueue, &currProcess, quantum, &next_preemtion_time, log_file);

            // Check if we should clear R bits every k quantums
            if (k > 0 && quantums_passed > 0 && quantums_passed % k == 0)
            {
                clear_recent();
            }

            if (currProcess != NULL)
            {
                union Semun s;
                s.val = 0;
                semctl(sem_id, 0, SETVAL, s);
                up(sem_id);
            }
        }
    }
    // upon termination release the clock resources.
    msgctl(msgq_id, IPC_RMID, NULL);
    semctl(sem_id, 0, IPC_RMID);
    write_perf(perf, perf_file);
    shmdt(shmRT_addr);
    shmctl(shmRT_id, IPC_RMID, NULL);
    fclose(log_file);
    fclose(perf_file);
    destroyClk(false);
}
