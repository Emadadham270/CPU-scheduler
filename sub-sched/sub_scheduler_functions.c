#include "../data_structures/PCB/Sch_PCB.h"
#include "sub_scheduler.h"
// #include <math.h> // Do we need this?
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/sem.h>

#define MSGQ_SUB1_PROJ     75
#define MSGQ_SUB2_PROJ     76
#define MSGQ_RESPONSE_PROJ 77
#define LOAD_SHM_PROJ      80

/* Load SHM layout: 4 ints = [count1, totalRT1, count2, totalRT2] */
#define LOAD_SHM_SIZE      (4 * sizeof(int))
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
#define KEYFILE_PATH "../keyfile"


struct PCB createPCB(processData p)
{
    struct PCB pcb;

    pcb.id = p.id;
    pcb.arrival = p.arrival;
    pcb.runtime = p.runtime;
    pcb.priority = p.priority;
    pcb.start_time = -1;
    pcb.finish_time = -1;
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

    write_comment_line(log_file);

    return;
}

void FCFS_algo(Queue *readyQueue, struct PCB **currProcess, FILE *log_file)
{
    if (*currProcess == NULL && !isEmpty(readyQueue))
    {
        *currProcess = dequeue(readyQueue);
        runProcess(*currProcess, log_file);
    }
}

void write_comment_line(FILE *log_file)
{
    char *comment = "#At\ttime\tx\tprocess\ty\tstate\tarr\tw\ttotal\tz\tremain\ty\twait\tk\n";
    fprintf(log_file, "%s", comment);
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

// extern int msgq_id;
// extern int sem_id,ready_sem;
// extern int *shmRT_addr;
// extern int *load_shm_addr;
// extern int my_msgq_id,msgq_resp_id;

int attach_2cpu_ipcs(int cpu_id, int *my_msgq_id,
                     int *msgq_resp_id, int **load_shm_addr)
{
    /* --- Attach to this sub-scheduler's msgq --- */
    int proj_id = (cpu_id == 1) ? MSGQ_SUB1_PROJ : MSGQ_SUB2_PROJ;
    key_t key_sub = ftok(KEYFILE_PATH, proj_id);
    *my_msgq_id = msgget(key_sub, 0666);
    if (*my_msgq_id == -1)
    {
        perror("Sub-scheduler: Error attaching to my msgq");
        return -1;
    }

    /* --- Attach to the response msgq --- */
    key_t key_resp = ftok(KEYFILE_PATH, MSGQ_RESPONSE_PROJ);
    *msgq_resp_id = msgget(key_resp, 0666);
    if (*msgq_resp_id == -1)
    {
        perror("Sub-scheduler: Error attaching to response msgq");
        return -1;
    }

    /* --- Attach to the Load SHM --- */
    key_t key_shm = ftok(KEYFILE_PATH, LOAD_SHM_PROJ);
    int load_shm_id = shmget(key_shm, LOAD_SHM_SIZE, 0666);
    if (load_shm_id == -1)
    {
        perror("Sub-scheduler: Error attaching to Load SHM");
        return -1;
    }
    *load_shm_addr = (int *)shmat(load_shm_id, NULL, 0);
    if ((long)*load_shm_addr == -1)
    {
        perror("Sub-scheduler: Error mapping Load SHM");
        return -1;
    }

    return 0;
}

void write_load_shm(int *load_shm_addr, int cpu_id, int count, int totalRT)
{
    //we may change this to only change to one slot at a time .
    if (cpu_id == 1)
    {
        load_shm_addr[LOAD_SHM_SLOT_COUNT1]   = count;
        load_shm_addr[LOAD_SHM_SLOT_TOTALRT1] = totalRT;
    }
    else
    {
        load_shm_addr[LOAD_SHM_SLOT_COUNT2]   = count;
        load_shm_addr[LOAD_SHM_SLOT_TOTALRT2] = totalRT;
    }
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
            execl("../outFiles/process.out", "process.out", runtime_str,shm_str,sem_str,
                  (char *)NULL);
            perror("execl failed");
            _exit(1);
        }
        
        pcb->pid = pid;
        pcb->lState = START;

        log_data(log_file, pcb);
    }
    else
    {
        pcb->lState = RESUME;
        log_data(log_file, pcb);
        kill(pcb->pid, SIGCONT);
    }
    pcb->state = 'R';
   
    // printf("process %d runnig \n", pcb->pid);
}
