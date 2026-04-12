#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <sys/types.h>

#include "../data structs/structs.h"
#include "../data_structures/PCB/Sch_PCB.h"
#include <stdio.h>

extern int msgq_id;

processData receive(int msgq_id);
struct PCB createPCB(processData p);
struct PerfVars initialize_perf();
void runProcess(struct PCB *pcb, FILE *log_file);
void RR_algo(Queue *readyQueue, struct PCB **currProcess, int q,
             int *next_preemtion_time, FILE *log_file);
void HPF_algo(Queue *readyQueue, struct PCB **currProcess, FILE *log_file);
void FCFS_algo(Queue *readyQueue, struct PCB **currProcess, int N, int M, FILE *log_file);
void handle_context_switch();
void wait_N_secs(int N);
void cleanup(int signum);

void create_log_files(FILE **log_file, FILE **perf_file);
void write_comment_line(FILE *log_file);
void log_data(FILE *log_file, PCB *pcb);
void write_perf(struct PerfVars perf, FILE* perf_file);
void up(int sem);

#endif // SCHEDULER_H
