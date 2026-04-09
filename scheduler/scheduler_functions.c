#include "../headers.h"
#include "scheduler.h"
#include "../data_structures/PCB/Sch_PCB.h"

struct processData receive(int msgq_id)
{
    struct processData p;
    int rec_val = msgrcv(msgq_id, &p, sizeof(processData) - sizeof(long), p.mtype, !IPC_NOWAIT );
    if (rec_val == -1)
    {
        perror("Error in receive");
        exit(1);
    } 
    return p;
}

struct  PCB createPCB(struct processData p)
{
    struct PCB pcb;
    pid_t pid = fork();
    execl("../outFiles/process.out",pcb.p.runtime);
    pcb.p=p;
    pcb.start_time=-1;
    pcb.finish_time=-1;
    pcb.remaining_time=pcb.p.runtime;
    pcb.pid=pid;
    // pcb->state="ready";        ------to be changed---- 
    return pcb;
    
}

void cleanup(int signum) {
    // Release all IPC resources 
    msgctl(msgq_id, IPC_RMID, NULL);
    destroyClk(true);
    exit(0);
}