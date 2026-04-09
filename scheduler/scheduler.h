#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <sys/types.h>  
int msgq_id;
struct processData receive(int msgq_id);
struct  PCB  createPCB(struct processData p);
void RR_algo(Queue* readyQueue,struct PBC* currProcess,int q);
void HPF_algo(Queue* readyQueue,struct PBC* currProcess);
void FCFS_algo(Queue* readyQueue,struct PBC* currProcess,int N,int M);
void handle_context_switch();
void wait_one_sec();
void cleanup(int signum);

#endif // SCHEDULER_H