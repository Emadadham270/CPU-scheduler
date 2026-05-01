#ifndef MMU_H
#define MMU_H

#include <sys/types.h>
#include "../data structs/structs.h"
#include "../data_structures/PCB/Sch_PCB.h"
#include <stdio.h>

#define MEM_SIZE 32
#define PAGE_TABLE_SIZE 64


#include "../data structs/structs.h"
#include <sys/types.h>
#define FRAMES_NUM 32
extern  Frame frames[FRAMES_NUM];


Frame RAM[MEM_SIZE];

void clear_recent();
short validate(PCB *process, int address);
short check(PCB *pcb, int vpt_address);
void define_page_table(int pid, int frame_index);
int check_free_frame();
void put_page_in_frame(int pid, int page_number, int frame_index);

#endif
int fault_handler();

extern int block_end_time;
void swap(int id, int frameIndex, Frame arr[],int type,int page);
VirtualAddress parse_virtual_address(char *bin_str);
#endif
