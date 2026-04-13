#include "../data_structures/PCB/Sch_PCB.h"
#include "sub_scheduler.h"
// #include <math.h> // Do we need this?
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/sem.h>

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
    pcb.pid = -1;
    pcb.next = NULL;

    return pcb;
}

struct PerfVars initialize_perf() {
    struct PerfVars perf;
    
    perf.avg_Waiting = 0.0;
    perf.avg_WTA = 0.0;
    perf.first_arrival = -1;
    perf.num_procs = 0;
    perf.std_WTA = 0.0;
    perf.total_runtime = 0;
    perf.finish_time = -1;

    return perf;
}

void create_log_files(FILE **log_file, FILE **perf_file, int which)
{
    mkdir("../logs/", 0755);

    char log_path[64], perf_path[64];
    snprintf(log_path, sizeof(log_path), "../logs/scheduler_%d.log", which);
    snprintf(perf_path, sizeof(perf_path), "../logs/scheduler_%d.perf", which);

    *log_file = fopen(log_path, "w");
    *perf_file = fopen(perf_path, "w");

    write_comment_line(log_file);

    return;
}

void FCFS_algo(Queue *readyQueue, struct PCB **currProcess, FILE *log_file)
{
    if (*currProcess == NULL && !isEmpty(readyQueue))
    {
        *currProcess = dequeue(readyQueue);
        runProcess(*currProcess, log_file);
    }
}

void write_comment_line(FILE *log_file)
{
    char *comment = "#At\ttime\tx\tprocess\ty\tstate\tarr\tw\ttotal\tz\tremain\ty\twait\tk\n";
    fprintf(log_file, "%s", comment);
}