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

void FCFS_algo(Queue *readyQueue, struct PCB **currProcess, FILE *log_file);
void create_log_files(FILE **log_file, FILE **perf_file, int which);
#endif // SUB_SCHEDULER_H