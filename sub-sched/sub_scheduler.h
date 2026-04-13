#ifndef SUB_SCHEDULER_H
#define SUB_SCHEDULER_H

#include <sys/types.h>

#include "../data structs/structs.h"
#include "../data_structures/PCB/Sch_PCB.h"
#include "../scheduler/scheduler.h"

#include <signal.h>
#include <stdio.h>

// struct PCB createPCB(processData p);
// struct PerfVars initialize_perf();

extern int msgq_id;
extern int sem_id,ready_sem;
extern int shmRT_id;
extern int *shmRT_addr;
extern int *load_shm_addr;
extern int msgq_sub1_id, msgq_sub2_id,msgq_resp_id;

void runProcess(struct PCB *pcb, FILE *log_file);
void FCFS_algo(Queue *readyQueue, struct PCB **currProcess, FILE *log_file);
void create_log_files(FILE **log_file, FILE **perf_file, int which);
#endif // SUB_SCHEDULER_H