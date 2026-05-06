#ifndef REQ_H
#define REQ_H

#include "../../data structs/structs.h"

reqQueue *createReqQueue();
int    isEmptyReq(reqQueue *q);

void   enqueueReq(reqQueue *q, request*req);             

request   *dequeueReq(reqQueue *q);
request   *peekReq(reqQueue *q);

void   freeReqQueue(reqQueue *q);

#endif