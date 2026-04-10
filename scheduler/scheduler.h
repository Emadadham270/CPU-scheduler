#ifndef SCHEDULER_H
#define SCHEDULER_H


#include <sys/types.h>  

#include "../data structs/structs.h"
#include "../data_structures/PCB/Sch_PCB.h"


processData receive(int msgq_id);
struct PCB createPCB(processData p);
void RR_algo(Queue* readyQueue, struct PCB* currProcess, int q);
void HPF_algo(Queue* readyQueue, struct PCB* currProcess);
void FCFS_algo(Queue* readyQueue, struct PCB* currProcess, int N, int M);
void handle_context_switch();
void wait_one_sec();
void cleanup(int signum);

#endif // SCHEDULER_H