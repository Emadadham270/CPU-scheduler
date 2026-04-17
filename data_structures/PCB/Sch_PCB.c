#include <stdio.h>
#include <stdlib.h>
#include "Sch_PCB.h"

Queue *createQueue()
{
    Queue *q = (Queue *)malloc(sizeof(Queue));
    if (q == NULL)
    {
        perror("malloc failed");
        exit(1);
    }
    q->front = q->rear = NULL;
    q->size = 0;
    return q;
}

int isEmpty(Queue *q)
{
    return q->front == NULL;
}

void enqueue(Queue *q, PCB *pcb)
{
    Node *newNode = (Node *)malloc(sizeof(Node));
    if (newNode == NULL)
    {
        perror("malloc failed");
        exit(1);
    }
    newNode->pcb = pcb;
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

PCB *dequeue(Queue *q)
{
    if (isEmpty(q))
    {
        fprintf(stderr, "Queue underflow!\n");
        exit(1);
    }
    Node *temp = q->front;
    PCB *p = temp->pcb;
    q->front = q->front->next;
    if (q->front == NULL)
        q->rear = NULL;
    free(temp);
    q->size--;
    return p;
}

PCB   *dequeue_rear(Queue* q) {
    if(isEmpty(q)) {
        return NULL;  /* nothing to steal */
    }

    if(q->front == q->rear) {
        PCB *p = q->front->pcb;
        free(q->front);
        q->front = q->rear = NULL;
        q->size--;
        return p;
    }

    Node *trav = q->front;
    while(trav->next != q->rear) {
        trav = trav->next;
    }
    PCB *p = q->rear->pcb;
    free(q->rear);
    q->rear = trav;
    q->rear->next = NULL;
    q->size--;
    return p;
}

PCB *peek(Queue *q)
{
    if (isEmpty(q))
    {
        fprintf(stderr, "Queue is empty!\n");
        exit(1);
    }
    return q->front->pcb;
}

PCB   *peek_rear(Queue *q) {
    if (isEmpty(q))
    {
        fprintf(stderr, "Queue is empty!\n");
        exit(1);
    }
    return q->rear->pcb;
}

void freeQueue(Queue *q)
{
    while (!isEmpty(q))
        dequeue(q);
    free(q);
}

void enqueue_priority(Queue *q, PCB *pcb)
{
    Node *node = malloc(sizeof(Node));
    if (node == NULL)
    {
        perror("malloc failed");
        exit(1);
    }
    node->pcb = pcb;
    node->next = NULL;

    if (q->front == NULL || pcb->priority < q->front->pcb->priority)
    {
        node->next = q->front;
        q->front = node;
        if (q->rear == NULL)
            q->rear = node;
        q->size++;
        return;
    }

    Node *curr = q->front;
    while (curr->next != NULL &&
           (curr->next->pcb->priority < pcb->priority ||
            (curr->next->pcb->priority == pcb->priority &&
             curr->next->pcb->arrival <= pcb->arrival)))
    {
        curr = curr->next;
    }

    node->next = curr->next;
    curr->next = node;

    if (node->next == NULL)
        q->rear = node;

    q->size++;
}

void update_waiting_times(Queue *q)
{
    Node *curr = q->front;
    while (curr != NULL)
    {
        curr->pcb->waiting_time++;
        curr = curr->next;
    }
}

int total_remaining_time(Queue *q)
{
    int total = 0;
    Node *curr = q->front;
    while (curr != NULL)
    {
        total += curr->pcb->remaining_time;
        curr = curr->next;
    }
    return total;
}

/*
plan

1- insert sorted ==================> DONE
2- for a preemptive we need a function to check if there a higher pr process came ( not now but
i have more than scenario
    . make a shm that the remaining time of the running process and check through
    . signal the scheduler every time new one arrive ( i think bad sync)
)
3- update waiting time ============> DONE



*/