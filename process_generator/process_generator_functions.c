#include <stdio.h>
#include <stdlib.h>
#include "process_generator.h"

PGQueue *pg_createQueue()
{
    PGQueue *q = (PGQueue *)malloc(sizeof(PGQueue));
    if (q == NULL)
    {
        perror("malloc failed");
        exit(1);
    }
    q->front = q->rear = NULL;
    q->size = 0;
    return q;
}

int pg_isEmpty(PGQueue *q)
{
    return q->front == NULL;
}

void pg_enqueue(PGQueue *q, processData p)
{
    PGNode *newNode = (PGNode *)malloc(sizeof(PGNode));
    if (newNode == NULL)
    {
        perror("malloc failed");
        exit(1);
    }
    newNode->data = p;
    newNode->next = NULL;

    if (q->rear == NULL)
    {
        q->front = q->rear = newNode;
    }
    else
    {
        q->rear->next = newNode;
        q->rear = newNode;
    }
    q->size++;
}

processData pg_dequeue(PGQueue *q)
{
    if (pg_isEmpty(q))
    {
        fprintf(stderr, "Queue underflow!\n");
        exit(1);
    }
    PGNode *temp = q->front;
    processData p = temp->data;
    q->front = q->front->next;
    if (q->front == NULL)
        q->rear = NULL;
    free(temp);
    q->size--;
    return p;
}

processData pg_peek(PGQueue *q)
{
    if (pg_isEmpty(q))
    {
        fprintf(stderr, "Queue is empty!\n");
        exit(1);
    }
    return q->front->data;
}

void pg_freeQueue(PGQueue *q)
{
    while (!pg_isEmpty(q))
        pg_dequeue(q);
    free(q);
    //
}