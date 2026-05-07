#include <stdio.h>
#include <stdlib.h>
#include "requests.h"

reqQueue *createReqQueue()
{
    reqQueue *q = (reqQueue *)malloc(sizeof(reqQueue));
    if (q == NULL)
    {
        perror("malloc failed");
        exit(1);
    }
    q->front = q->rear = NULL;
    q->size = 0;
    return q;
}

int isEmptyReq(reqQueue *q)
{
    return q->front == NULL;
}

void enqueueReq(reqQueue *q, request*req)
{
    reqNode *newNode = (reqNode *)malloc(sizeof(reqNode));
    if (newNode == NULL)
    {
        perror("malloc failed");
        exit(1);
    }
    newNode->req = req;
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

request *dequeueReq(reqQueue *q)
{
    if (isEmptyReq(q))
    {
        fprintf(stderr, "Queue underflow!\n");
        exit(1);
    }
    reqNode *temp = q->front;
    request *r = temp->req;
    q->front = q->front->next;
    if (q->front == NULL)
        q->rear = NULL;
    free(temp);
    q->size--;
    return r;
}


request *peekReq(reqQueue *q)
{
    if (isEmptyReq(q))
    {
        fprintf(stderr, "Queue is empty!\n");
        exit(1);
    }
    return q->front->req;
}


void freeReqQueue(reqQueue *q)
{
    while (!isEmptyReq(q))
        dequeueReq(q);
    free(q);
}

