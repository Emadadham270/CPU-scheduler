#ifndef PROCESS_QUEUE_H
#define PROCESS_QUEUE_H

#include "../data structs/structs.h"


PGQueue *pg_createQueue();
int pg_isEmpty(PGQueue *q);
void pg_enqueue(PGQueue *q, processData p);
processData pg_dequeue(PGQueue *q);
processData pg_peek(PGQueue *q);
void pg_freeQueue(PGQueue *q);

#endif