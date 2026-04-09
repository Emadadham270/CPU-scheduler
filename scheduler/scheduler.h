#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <sys/types.h>  

struct processData receive(int msgq_id);
struct  PCB  *convert(struct processData p);


#endif // SCHEDULER_H