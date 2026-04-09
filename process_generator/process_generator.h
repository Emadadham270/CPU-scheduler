#ifndef PROCESS_QUEUE_H
#define PROCESS_QUEUE_H

typedef struct processData
{
    long mtype;
    int id;
    int arrival;
    int runtime;
    int priority;
} processData;

// processes queue
typedef struct Node
{
    processData data;
    struct Node *next;
} Node;

typedef struct Queue
{
    Node *front;
    Node *rear;
    int size;
} Queue;

Queue *createQueue();
int isEmpty(Queue *q);
void enqueue(Queue *q, processData p);
processData dequeue(Queue *q);
processData peek(Queue *q);
void freeQueue(Queue *q);

#endif