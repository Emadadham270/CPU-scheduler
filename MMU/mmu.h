#ifndef MMU_H
#define MMU_H

#include <sys/types.h>
#include "../data structs/structs.h"
#include "../data_structures/PCB/Sch_PCB.h"
#include <stdio.h>

#define MEM_SIZE 32
#define PAGE_TABLE_SIZE 64
extern int block_end_time;

Frame RAM[MEM_SIZE];

void clear_recent();
short validate(PCB *process, int address);
short check(PCB *pcb, int vpt_address);
void define_page_table(int pid, int frame_index);
int check_free_frame();
void put_page_in_frame(int pid, int page_number, int frame_index);
void fault_handler(int pid,int page_num,char req_type);
void swap(int id, int frameIndex,int type,int page);
VirtualAddress parse_virtual_address(char *bin_str);
short check_page_in_RAM(int vpt_address);
#endif
