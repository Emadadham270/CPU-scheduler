#include <stdio.h>
#include <stdlib.h>
#include "process_generator.h"

Queue *createQueue()
{
    Queue *q = (Queue *)malloc(sizeof(Queue));
    q->front = q->rear = NULL;
    q->size = 0;
    return q;
}

int isEmpty(Queue *q)
{
    return q->front == NULL;
}

void enqueue(Queue *q, processData p)
{
    Node *newNode = (Node *)malloc(sizeof(Node));
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

processData dequeue(Queue *q)
{
    if (isEmpty(q))
    {
        fprintf(stderr, "Queue underflow!\n");
        exit(1);
    }
    Node *temp = q->front;
    processData p = temp->data;
    q->front = q->front->next;
    if (q->front == NULL)
        q->rear = NULL;
    free(temp);
    q->size--;
    return p;
}

processData peek(Queue *q)
{
    if (isEmpty(q))
    {
        fprintf(stderr, "Queue is empty!\n");
        exit(1);
    }
    return q->front->data;
}

void freeQueue(Queue *q)
{
    while (!isEmpty(q))
        dequeue(q);
    free(q);
    //
}