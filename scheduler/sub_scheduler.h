#ifndef SUB_SCHEDULER_H
#define SUB_SCHEDULER_H

#include <sys/types.h>

#include "../data structs/structs.h"
#include "../data_structures/PCB/Sch_PCB.h"

#include <signal.h>
#include <stdio.h>

void stall_sig(int signum);
void FCFS_algo();

#endif // SUB_SCHEDULER_H