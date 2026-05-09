#include "rr_scheduler.h"

int msgq_id;
int req_msgq;
int sem_id;
int receivingProcesses = 1;
Queue *readyQueue;
Queue *currentPCBs;
Queue *blockQueue;
reqQueue *requests;
struct PCB *currProcess = NULL;
int quantum;
int k = 0;
int quantums_passed = 0;
int processFinishedSignal = 0;
int next_preemtion_time = -1;
int *shmRT_addr;
int shmRT_id;
int context_switch_until = -1;
int last_print = -1;
perfVars perf;
FILE *log_file, *perf_file;

void onProcessFinished(int signum)
{
    (void)signum;
    processFinishedSignal = 1;
}
int lag = 0;
int last_received_sync = -1;
int main(int argc, char *argv[])
{
    (void)argc;
    key_t key_id, req_key_id;
    signal(SIGINT, cleanup);
    signal(SIGUSR1, onProcessFinished);
    key_id = ftok("../keyFile", 65);
    req_key_id = ftok("../keyFile", 70);
    msgq_id = msgget(key_id, 0666);
    req_msgq = msgget(req_key_id, 0666 | IPC_CREAT);
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
    currentPCBs = createQueue();
    blockQueue = createQueue();
    requests= createReqQueue();
    char *e;
    quantum = strtol(argv[1], &e, 10);
    char *e2;
    k = strtol(argv[2], &e2, 10);

    FILE *memory_file;
    create_log_files(&log_file, &perf_file);
    write_comment_line(log_file);

    mkdir("../logs", 0755);
    memory_file = fopen("../logs/memory.log", "w");
    if (memory_file == NULL)
    {
        perror("Error opening memory log");
        exit(1);
    }
    setvbuf(memory_file, NULL, _IONBF, 0);
    set_memory_log(memory_file);

    int last_tick = -1;

    initClk();

    while (!isEmpty(readyQueue) || !isEmpty(blockQueue) || receivingProcesses || currProcess)
    {
        int now = getClk();
        if (now == last_tick)
            continue; // spin until next tick

        last_tick = now;

        // 0. receive requests for the memory
        handleRequests(&lag);

        // 1. check blocked processes
        checkBlockEnd();

        // 2. Handle finished process (before algo, so we don't preempt a dead process)
        handleFinishedProcesses();

        if (context_switch_until == -1 || now >= context_switch_until)
        {
            context_switch_until = -1;
            // 3. Receive new arrivals synchronously with process_generator
            // Only call when now strictly exceeds the last sync tick we already
            // received — avoids the double-call that consumed tick 1 at tick 0
            // and forced dispatch to tick 2.
            if (receivingProcesses && now > last_received_sync)
                receiveProcesses();
            
            // 4. Run scheduling algorithm (preempt → re-enqueue → dispatch)
            if (now > 0)
                RR_algo(readyQueue, &currProcess, quantum, &next_preemtion_time, log_file);
            // Check if we should clear R bits every k quantums
            if (k > 0 && quantums_passed > 0 && quantums_passed % k == 0&& quantums_passed > last_print){
                clear_recent();
                last_print = quantums_passed;
                //printAllFrames();
            }

            if (currProcess != NULL&&now >= context_switch_until)
            {
                union Semun s;
                s.val = 0;
                semctl(sem_id, 0, SETVAL, s);
                up(sem_id);
            }
        }     
    }
    
    // upon termination release the clock resources.
    msgctl(req_msgq, IPC_RMID, NULL);
    msgctl(msgq_id, IPC_RMID, NULL);
    semctl(sem_id, 0, IPC_RMID);
    write_perf(perf, perf_file);
    shmdt(shmRT_addr);
    shmctl(shmRT_id, IPC_RMID, NULL);
    fclose(log_file);
    fclose(perf_file);
    fclose(memory_file);
    destroyClk(false);
}
