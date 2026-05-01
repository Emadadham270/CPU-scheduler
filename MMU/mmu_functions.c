#include "mmu.h"

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

//----------------to be changed -----------------------
int fault_handler(int pid)
{
    int cls=4;
    int victim_index=-1;
    for(int i=0;i<FRAMES_NUM;i++)
    {
        if(!frames[i].occupied) 
        {
            victim_index=i;
            break;
        }
        if(frames[i].pte) // pte != null ,that means it is a page table 
            continue;
        int curr_cls=2*frames[i].R + frames[i].M;
        if(curr_cls<cls)
        {
            cls=curr_cls;
            victim_index=i;
        }
    }
    if(cls<4)
        Swap(victim_index,pid);
    else 
        allocate(victim_index,pid);
        //we allocate the new page in this frame 

}
void clear_recent()
{
    for (int i = 0; i < MEM_SIZE; i++)
    {
        if (RAM[i].occupied)
        {
            RAM[i].R = 0;
        }
    }
}

int validate(PCB *pcb, int address)
{
    address /= 4; // (010000)_b -> (01)_b -> (1)_dec
    // return (address < pcb.limit) ? (address) : -1;
}

short check(PCB *pcb, int vpt_address)
{
    int pt_address = pcb->frame_index;
    PTE *PT = RAM[pt_address].pte;
    // int pid = RAM[pt_address].process_id; // Can we get the PCB from the PID somehow ?
    vpt_address = validate(pcb, vpt_address);
    if (vpt_address != -1)
    {
        if (PT[vpt_address].valid)
        {
            return 1; // True
        }
        else
        {
            return 0; // False
        }
    }
}

/*
validate
check
make R =0 == DONE ==


structs i think done
update pcb done for now
edit el RR run to make the init logic ( put the pt fe el frame we put pt[0] fe new frame )






*/

//  int index = check_free_frame();
//     pcb->frame_index = index;
//     dafine_page_table(pcb->id, index);

//     put_page_in_frame(pcb->id, 0, index);

int check_free_frame()
{
    int minClass = 4; 
    int candidate_index = -1;
    for (int i = 0; i < MEM_SIZE; i++)
    {
        minClass =(RAM[i].R*2 + RAM[i].M)<minClass ? (RAM[i].R*2 + RAM[i].M) : minClass;
        candidate_index = (RAM[i].R*2 + RAM[i].M)==minClass ? i : candidate_index;
        if (!RAM[i].occupied)
        {
            return i;
        }
    }

    //CALL NRU here and the candidate_index will be the one to replace
}

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

void put_page_in_frame(int pid, int page_number, int frame_index)
{
    RAM[frame_index].occupied = 1;
    RAM[frame_index].process_id = pid;
    RAM[frame_index].pte[page_number].valid = 1;
    RAM[frame_index].pte[page_number].R = 0;
    RAM[frame_index].pte[page_number].M = 0;
    RAM[frame_index].pte[page_number].frame_address = frame_index; // Assuming the frame address is the same as the index for simplicity
}