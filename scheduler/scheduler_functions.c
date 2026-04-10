#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/msg.h>
#include <signal.h>
#include "scheduler.h"
#include "../data_structures/PCB/Sch_PCB.h"

typedef short bool;
void destroyClk(bool terminateAll);
int getClk(void);

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

void runProcess(struct PCB *pcb)
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
            execl("../outFiles/process.out", "process.out", runtime_str, (char *)NULL);
            perror("execl failed");
            _exit(1);
        }

        pcb->pid = pid;
    }
    else
    {
        kill(pcb->pid, SIGCONT);
    }
    pcb->state = 'R';
    printf("process %d runnig \n", pcb->pid);
}

void cleanup(int signum)
{
    (void)signum;
    // Release all IPC resources
    msgctl(msgq_id, IPC_RMID, NULL);
    destroyClk(1);
    exit(0);
}

void RR_algo(Queue *readyQueue, struct PCB *currProcess, int q)
{
    (void)readyQueue;
    (void)currProcess;
    (void)q;
}

void HPF_algo(Queue *readyQueue, struct PCB **currProcess)
{
    if (*currProcess != NULL && !isEmpty(readyQueue))
    {
        PCB *top = peek(readyQueue);

        if (top->priority < (*currProcess)->priority)
        {
            kill((*currProcess)->pid, SIGSTOP);
            (*currProcess)->state = 'W';
            enqueue(readyQueue, (*currProcess));
            printf("process %d stoped \n", (*currProcess)->pid);
            // here supposed to call context switch ??

            (*currProcess) = dequeue(readyQueue);
            runProcess(*currProcess);
        }
    }

    if ((*currProcess) == NULL && !isEmpty(readyQueue))
    {
        *currProcess = dequeue(readyQueue);
        runProcess(*currProcess);
    }
}

void FCFS_algo(Queue *readyQueue, struct PCB **currProcess, int N, int M)
{
    (void)N;
    (void)M;

    if (*currProcess == NULL && !isEmpty(readyQueue))
    {
        *currProcess = dequeue(readyQueue);
        runProcess(*currProcess);
    }
}

void handle_context_switch(void)
{
}

void wait_one_sec(void)
{
    sleep(1);
}