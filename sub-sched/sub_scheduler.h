#ifndef SUB_SCHEDULER_H
#define SUB_SCHEDULER_H

#include <sys/types.h>

#include "../data structs/structs.h"
#include "../data_structures/PCB/Sch_PCB.h"

#include <signal.h>
#include <stdio.h>

extern int load_sem_id;
extern int threshold_sem_id;
extern int cpu_id;
extern int sem_id;
extern int shmRT_id;
extern int *shmRT_addr;
extern int my_msgq_id;
extern int msgq_resp_id;
extern int *load_shm;
extern int current_tick_time;

struct PCB createPCB(processData p);
struct PerfVars initialize_perf();
void runProcess(struct PCB *pcb, FILE *log_file);
void FCFS_algo(Queue *readyQueue, struct PCB **currProcess, FILE *log_file);
void create_log_files(FILE **log_file, FILE **perf_file, int which);
void write_comment_line(FILE *log_file);
void log_data(FILE *log_file, PCB *pcb);
void write_perf(struct PerfVars perf, FILE *perf_file);
void up(int sem);
void down(int sem);



int attach_2cpu_ipcs(int cpu_id);
void write_load_shm(int *load_shm_addr, int cpu_id, int count, int totalRT);
void create_ipcs(int cpu_id);
void steal_handler(int signum);
void cleanup(int signum);
#endif // SUB_SCHEDULER_H
