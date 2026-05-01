#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "mmu.h"
#include "../data structs/structs.h"
#include "../headers.h"
#include "../rr_scheduler/rr_scheduler.h"

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

void allocatePageTable()
{

}


void swap(int id, int frameIndex, Frame arr[],int type,int page)
{
    //set the delay
     if (arr[frameIndex].M == 0)
    {
        block_end_time = getClk() + 10;
    }
    else
    {
        block_end_time = getClk() + 20;
    }

    //update last owner page table
    PCB *old_owner = get_process(arr[frameIndex].process_id);
    arr[old_owner->frame_index].pte[arr[frameIndex].vpage].valid = 0;
    arr[old_owner->frame_index].pte[arr[frameIndex].vpage].frame_address = -1;
    arr[old_owner->frame_index].pte[arr[frameIndex].vpage].R = 0;
    arr[old_owner->frame_index].pte[arr[frameIndex].vpage].M = 0;

    //check if uou load a page table or a normal data
    if(type==1)
        allocatePageTable();
    else
    {
        PCB *owner = get_process(id);
        //owner->state = BLOCKED;===================>to be added
        //owner->unblock_at = block_end_time;
        arr[owner->frame_index].pte[page].valid = 1;
        arr[owner->frame_index].pte[page].frame_address = frameIndex;
        arr[owner->frame_index].pte[page].R = 0;
        arr[owner->frame_index].pte[page].M = 0;
        arr[frameIndex].process_id = id;
        arr[frameIndex].R = 0;
        arr[frameIndex].M = 0;
        arr[frameIndex].occupied = 1;
        arr[frameIndex].vpage = page; 
    }

   
   
}

VirtualAddress parse_virtual_address(char *bin_str)
{
    // convert binary string to integer
    int addr = 0;
    for (int i = 0; bin_str[i] != '\0'; i++)
        addr = addr * 2 + (bin_str[i] - '0');

    // split into page and offset
    VirtualAddress va;
    va.page   = addr >> 4;   // upper 6 bits
    va.offset = addr & 0xF;  // lower 4 bits
    return va;
}