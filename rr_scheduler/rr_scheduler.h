#ifndef RR_SCHEDULER_H
#define RR_SCHEDULER_H

#include <sys/types.h>
#include "../data structs/structs.h"
#include "../data_structures/PCB/Sch_PCB.h"
#include "../headers.h"
#include "../MMU/mmu.h"
#include <stdio.h>

extern int msgq_id;
extern int req_msgq;
extern int sem_id;
extern int receivingProcesses;
extern Queue *readyQueue;
extern Queue *currentPCBs;
extern struct PCB *currProcess;
extern int quantum;
extern int k;
extern int quantums_passed;
extern int processFinishedSignal;
extern int next_preemtion_time;
extern int *shmRT_addr;
extern int shmRT_id;
extern int context_switch_until;
extern perfVars perf;

struct PCB createPCB(processData p);
struct PerfVars initialize_perf();
void runProcess(struct PCB *pcb, FILE *log_file);
void RR_algo(Queue *readyQueue, struct PCB **currProcess, int q,
             int *next_preemtion_time, FILE *log_file);
void wait_N_secs(int pen, int N);
void cleanup(int signum);
void create_log_files(FILE **log_file, FILE **perf_file);
void write_comment_line(FILE *log_file);
void log_data(FILE *log_file, PCB *pcb);
void write_perf(struct PerfVars perf, FILE *perf_file);
void down(int sem);
void up(int sem);
void handleRequests();
void initialize_PCB(PCB *pcb);

#endif // RR_SCHEDULER_H
