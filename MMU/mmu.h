#ifndef MMU_H
#define MMU_H

#include "../data structs/structs.h"
#include <sys/types.h>
#define FRAMES_NUM 32
extern  Frame frames[FRAMES_NUM];


int fault_handler();

extern int block_end_time;
void swap(int id, int frameIndex, Frame arr[],int type,int page);
VirtualAddress parse_virtual_address(char *bin_str);
#endif