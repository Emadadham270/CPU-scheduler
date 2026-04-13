#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <sys/types.h>

#include "../data structs/structs.h"
#include "../data_structures/PCB/Sch_PCB.h"
#include <stdio.h>

extern int msgq_id;
extern int sem_id;
extern int shmRT_id;
extern int *shmRT_addr;
extern int *load_shm_addr;
extern int msgq_sub1_id, msgq_sub2_id,msgq_resp_id;
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
void down(int sem);

int create_2cpu_ipcs();
void destroy_2cpu_ipcs();
void read_load_shm(int *load_shm_addr, int cpu_id, int *count, int *totalRT);
void read_all_load_shm(int *load_shm_addr,
                       int *count1, int *totalRT1,
                       int *count2, int *totalRT2);
void detach_2cpu_ipcs();
int select_cpu();
processData pcb_to_processData(PCB *pcb);
#endif // SCHEDULER_H
