#ifndef MMU_H
#define MMU_H

#include "../data structs/structs.h"
#include <sys/types.h>
#define FRAMES_NUM 32
extern  Frame frames[FRAMES_NUM];


int fault_handler();

#endif