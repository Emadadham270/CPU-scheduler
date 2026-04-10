#ifndef SCH_PCB_H
#define SCH_PCB_H

#include "../../data structs/structs.h"

typedef PCBNode Node;
typedef PCBQueue Queue;



Queue *createQueue();
int    isEmpty(Queue *q);

void   enqueue(Queue *q, PCB *pcb);             
void   enqueue_priority(Queue *q, PCB *pcb);    

PCB   *dequeue(Queue *q);
PCB   *peek(Queue *q);

void   update_waiting_times(Queue *q);          
int    total_remaining_time(Queue *q);


void   freeQueue(Queue *q);

#endif