#ifndef SCH_PCB_H
#define SCH_PCB_H

#include <sys/types.h>  




typedef struct PCB {
    int id;
    int arrival_time;
    int runtime;
    int priority;

    int remaining_time;
    int waiting_time;
    int start_time;
    int finish_time;

    pid_t pid;
    char state;           // 'R' running, 'W' waiting, 'F' finished

    struct PCB *next;     // for the PCB table LL
} PCB;





typedef struct Node {
    PCB *pcb;            
    struct Node *next;
} Node;



typedef struct {
    Node *front;
    Node *rear;
    int size;
} Queue;

Queue *createQueue();
int    isEmpty(Queue *q);

void   enqueue(Queue *q, PCB *pcb);             
void   enqueue_priority(Queue *q, PCB *pcb);    

PCB   *dequeue(Queue *q);
PCB   *peek(Queue *q);

void   update_waiting_times(Queue *q);          

      

void   freeQueue(Queue *q);

#endif