#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/msg.h>
#include "scheduler.h"
#include "../data_structures/PCB/Sch_PCB.h"

typedef short bool;
void destroyClk(bool terminateAll);

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
    pcb.next = NULL;

    pid_t pid = fork();
    if (pid == -1)
    {
        perror("fork failed");
        exit(1);
    }

    if (pid == 0)
    {
        char runtime_str[16];
        snprintf(runtime_str, sizeof(runtime_str), "%d", p.runtime);
        execl("../outFiles/process.out", "process.out", runtime_str, (char *)NULL);
        perror("execl failed");
        _exit(1);
    }

    pcb.pid = pid;
    return pcb;
    
}

void cleanup(int signum) {
    (void)signum;
    // Release all IPC resources 
    msgctl(msgq_id, IPC_RMID, NULL);
    destroyClk(1);
    exit(0);
}

void RR_algo(Queue* readyQueue, struct PCB* currProcess, int q)
{
    (void)readyQueue;
    (void)currProcess;
    (void)q;
}

void HPF_algo(Queue* readyQueue, struct PCB* currProcess)
{
    (void)readyQueue;
    (void)currProcess;
}

void FCFS_algo(Queue* readyQueue, struct PCB* currProcess, int N, int M)
{
    (void)readyQueue;
    (void)currProcess;
    (void)N;
    (void)M;
}

void handle_context_switch(void)
{
}

void wait_one_sec(void)
{
    sleep(1);
}