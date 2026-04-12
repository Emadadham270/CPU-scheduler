#include "../data_structures/PCB/Sch_PCB.h"
#include "scheduler.h"
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/sem.h>

typedef short bool;
void destroyClk(bool terminateAll);
int getClk(void);

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

processData receive(int msgq_id)
{
    processData p;
    int rec_val = msgrcv(msgq_id, &p, sizeof(processData) - sizeof(long), 0, 0);
    if (rec_val == -1)
    {
        perror("Error in receive");
        exit(1);
    }
    return p;
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

void runProcess(struct PCB *pcb, FILE *log_file)
{
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
            snprintf(runtime_str, sizeof(runtime_str), "%d", pcb->remaining_time);
            execl("../outFiles/process.out", "process.out", runtime_str,
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
    extern int dispatched_this_tick;
    dispatched_this_tick = 1;
    // printf("process %d runnig \n", pcb->pid);
}

void cleanup(int signum)
{
    (void)signum;
    // Release all IPC resources
    msgctl(msgq_id, IPC_RMID, NULL);
    semctl(sem_id, 0, IPC_RMID);
    semctl(ready_sem,0,IPC_RMID);
    // destroyClk(1);
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

        if (getClk() >= *next_preemtion_time)
        {
            /* Preempt: stop the current process and put it back in the queue */
            kill((*currProcess)->pid, SIGSTOP);
            (*currProcess)->state = 'W';
            (*currProcess)->lState = STOP;
            log_data(log_file, *currProcess);
            printf("process %d stoped \n", (*currProcess)->pid);
            enqueue(readyQueue, (*currProcess));

            /* Pick the next process from the queue */
            *currProcess = dequeue(readyQueue);
            runProcess(*currProcess, log_file);
            *next_preemtion_time = getClk() + q;
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
            kill((*currProcess)->pid, SIGSTOP);
            (*currProcess)->state = 'W';
            (*currProcess)->lState = STOP;

            log_data(log_file, *currProcess);

            enqueue_priority(readyQueue, (*currProcess));
            printf("process %d stoped \n", (*currProcess)->pid);
            //  here supposed to call context switch ? ?
            wait_N_secs(1);
            (*currProcess) = dequeue(readyQueue);
            runProcess(*currProcess, log_file);
        }
    }
    if ((*currProcess) == NULL && !isEmpty(readyQueue))
    {

        *currProcess = dequeue(readyQueue);
        runProcess(*currProcess, log_file);
    }
}

void FCFS_algo(Queue *readyQueue, struct PCB **currProcess, int N, int M, FILE *log_file)
{
    (void)N;
    (void)M;

    if (*currProcess == NULL && !isEmpty(readyQueue))
    {
        *currProcess = dequeue(readyQueue);
        runProcess(*currProcess, log_file);
    }
}

void handle_context_switch(struct PCB *oldProcess, struct PCB *newProcess, FILE *log_file)
{
    if (oldProcess != NULL && oldProcess->state == 'R')
    {
        kill(oldProcess->pid, SIGSTOP);
        oldProcess->state = 'W';
        // Log "stopped"
    }
    wait_N_secs(1);
    runProcess(newProcess, log_file);
}

void wait_N_secs(int N)
{
    int curr = getClk() + N;
    while (curr > getClk())
        ;
}

void create_log_files(FILE **log_file, FILE **perf_file)
{
    mkdir("../logs/", 0755);

    *log_file = fopen("../logs/scheduler.log", "w");
    *perf_file = fopen("../logs/scheduler.perf", "w");

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

    fprintf(log_file, "At\ttime\t%d\tprocess\t%d\t%s\tarr\t%d\ttotal\t%d\tremain\t%d\twait\t%d", getClk(), pcb->id, stateStr, pcb->arrival, pcb->runtime, pcb->remaining_time, pcb->waiting_time);
    if (finishFlag)
    {
        int TA = pcb->finish_time - pcb->arrival;
        float WTA = ((float)TA) / pcb->runtime;
        fprintf(log_file, "\tTA\t%d\tWTA\t%.2f", TA, WTA);
    }
    fprintf(log_file, "\n");
}

void write_perf(struct PerfVars perf, FILE* perf_file) {
    printf("finish time: %d\narrival: %d\ntotal_runtime: %d\n", perf.finish_time, perf.first_arrival, perf.total_runtime);
    float cpu_util = (float)perf.total_runtime * 100.0 / (float)(perf.finish_time - perf.first_arrival);
    perf.avg_WTA /= perf.num_procs;
    perf.avg_Waiting /= perf.num_procs;

    // TODO get the array of WTA's for standard deviation (or we can use a rolling standard deviation)
    // DONE: Decided on rolling standard deviation (Welford's)
    float std_WTA = sqrtf(perf.M2_WTA / (perf.num_procs - 1));
    fprintf(perf_file, "CPU utilization = %.2f%%\n", cpu_util);
    fprintf(perf_file, "Avg WTA = %.2f\n", perf.avg_WTA);
    fprintf(perf_file, "Avg Waiting = %.2f\n", perf.avg_Waiting);
    fprintf(perf_file, "Std WTA = %.2f\n", std_WTA);
}
