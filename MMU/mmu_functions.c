#include "mmu.h"

// #include <stdio.h>
#include <stdlib.h>
// #include <sys/types.h>

// #include "../data structs/structs.h"
#include "../rr_scheduler/rr_scheduler.h"

Frame RAM[MEM_SIZE];
//parse the virtual address and return the page number and offset
//used at rr_sched 322

VirtualAddress parse_virtual_address(int address)
{
    // convert binary digits (stored as int) to integer value
    // 1000000000
    int addr = 0;
    while (address != 0)
    {
        addr = addr * 2 + (address % 10);
        address /= 10;
    }

    // split into page and offset
    VirtualAddress va;
    va.page   = addr >> 4;   // upper 6 bits
    va.offset = addr & 0xF;  // lower 4 bits
    return va;
}
//check wether the address is valid and if it is in RAM or not
short check(PCB *pcb, int vpt_address,char req_type)
{
    int pt_address = pcb->frame_index;
    PTE *PT = RAM[pt_address].pte;
    
    if(vpt_address > pcb->limit)
        return -1; // Invalid
    
    
    if (PT[vpt_address].valid)
    {
        PT[vpt_address].R = 1; // Set R bit on access
        if(req_type=='w')
            PT[vpt_address].M = 1; // Set M bit on write
        int frame_index = PT[vpt_address].frame_address;
        RAM[frame_index].R = 1; // Set R bit on access
        if(req_type=='w')
            RAM[frame_index].M = 1; // Set M bit on write
        return 1; // True
    }
    else
    {
        return 0; // False
    }
}
//directly put the page in the frame without checking anything
void put_page_in_frame(int pid, int page_number, int frame_index)
{
    RAM[frame_index].occupied = 1;
    RAM[frame_index].process_id = pid;
    RAM[frame_index].pte[page_number].valid = 1;
    RAM[frame_index].pte[page_number].R = 1;
    RAM[frame_index].pte[page_number].M = 0;
    RAM[frame_index].pte[page_number].frame_address = frame_index; // Assuming the frame address is the same as the index for simplicity
}

//get owner
//return the PCB of the process with the given id, or NULL if not found
PCB *get_process(int id)
{
    if (currProcess != NULL && currProcess->id == id)
        return currProcess;

    // search all PCBs in one queue
    Node *curr = currentPCBs->front;
    while (curr != NULL)
    {
        if (curr->pcb->id == id)
            return curr->pcb;
        curr = curr->next;
    }

    return NULL;  // not found
}
//define the page table of the process at the given frame index
void define_page_table(int pid, int frame_index)
{
    RAM[frame_index].occupied = 1;
    RAM[frame_index].process_id = pid;
    RAM[frame_index].pte = (PTE *)malloc(sizeof(PTE) * PAGE_TABLE_SIZE);
    for (int i = 0; i < PAGE_TABLE_SIZE; i++)
    {
        RAM[frame_index].pte[i].valid = 0;
        RAM[frame_index].pte[i].R = 0;
        RAM[frame_index].pte[i].M = 0;
        RAM[frame_index].pte[i].frame_address = -1;
    }
}

//swap the page in the frame with the new page and update the page table of the owner process

void swap(int id, int frameIndex,int page,int type)
{
    //set the delay
    int block_end_time;
    if(page<64){
     if (RAM[frameIndex].M == 0)
    {
        block_end_time = getClk() + 10;
    }
    else
    {
        block_end_time = getClk() + 20;
    }}

    //update last owner page table
    PCB *old_owner = get_process(RAM[frameIndex].process_id);
    RAM[old_owner->frame_index].pte[RAM[frameIndex].vpage].valid = 0;
    RAM[old_owner->frame_index].pte[RAM[frameIndex].vpage].frame_address = -1;
    RAM[old_owner->frame_index].pte[RAM[frameIndex].vpage].R = 0;
    RAM[old_owner->frame_index].pte[RAM[frameIndex].vpage].M = 0;

    //check if you load a page table or a normal data
   
    if(type==0){
        define_page_table(id, frameIndex);
        return;
    }

    
    PCB *owner = get_process(id);
    if(page<64){
        owner->state = 'B';
        owner->unblock_at = block_end_time;
    }else
     page-=64;
    RAM[owner->frame_index].pte[page].valid = 1;
    RAM[owner->frame_index].pte[page].frame_address = frameIndex;
    RAM[owner->frame_index].pte[page].R = 1;
    RAM[owner->frame_index].pte[page].M = 0;
    RAM[frameIndex].process_id = id;
    RAM[frameIndex].R = 1;
    RAM[frameIndex].M = 0;
    RAM[frameIndex].occupied = 1;
    RAM[frameIndex].vpage = page; 
    
}
//handle faults of reserving data page 
void fault_handler(int pid,int page_num,int type)
{
    int cls=4;
    int victim_index=-1;
    for(int i=0;i<MEM_SIZE;i++)
    {
        if(!RAM[i].occupied) 
        {
            victim_index=i;
            break;
        }
        if(RAM[i].pte) // pte != null ,that means it is a page table 
            continue;
        int curr_cls=2*RAM[i].R + RAM[i].M;
        if(curr_cls<cls)
        {
            cls=curr_cls;
            victim_index=i;
        }
    }
    if(cls<4)
        swap(pid,victim_index,page_num,type);
    else 
        //we allocate the new page in this frame 
        put_page_in_frame(pid,page_num,victim_index);

}

void clear_recent()
{
    for (int i = 0; i < MEM_SIZE; i++)
    {
        if (RAM[i].occupied)
        {
            RAM[i].R = 0;
            if(RAM[i].pte) // if it's a page table, clear the R bits of its entries as well
            {
                for (int j = 0; j < PAGE_TABLE_SIZE; j++)
                {
                    RAM[i].pte[j].R = 0;
                }
            }
        }
    }
}

int validate(PCB *pcb, int address)
{
    // address is binary , we need to vonvert to int;

    int addr = 0;
    for (int i = 0; address != 0; i++)
    {
        addr = addr * 2 + (address % 10);
        address /= 10;
    }

    return addr/16<=pcb->limit ? addr/16 : -1;
}










