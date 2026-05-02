#ifndef MMU_H
#define MMU_H

#include <sys/types.h>
#include "../data structs/structs.h"
#include "../data_structures/PCB/Sch_PCB.h"
#include <stdio.h>

#define MEM_SIZE 3
#define PAGE_TABLE_SIZE 64

extern Frame RAM[MEM_SIZE];
extern FILE *memory_log;

void set_memory_log(FILE *log);
void clear_recent();
int validate(PCB *process, int address);
short check(PCB *pcb, int vpt_address, char req_type);
void define_page_table(int pid, int frame_index);
int check_free_frame();
void put_page_in_frame(int pid, int page_number, int frame_index);
void fault_handler(int pid, int page_num, int type, int raw_address);
void swap(int id, int frameIndex, int page, int type);
VirtualAddress parse_virtual_address(int address);
short check_page_in_RAM(int vpt_address);
PCB *get_process(int id);
#endif
