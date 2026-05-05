#include "mmu.h"

// #include <stdio.h>
#include <stdlib.h>
// #include <sys/types.h>

// #include "../data structs/structs.h"
#include "../rr_scheduler/rr_scheduler.h"

Frame RAM[MEM_SIZE];
FILE *memory_log = NULL;

static int page_fault_delay(int frame_index)
{
    if (frame_index >= 0 && RAM[frame_index].M != 0)
        return 20;
    return 10;
}

void set_memory_log(FILE *log)
{
    memory_log = log;
}

// parse the virtual address and return the page number and offset
// used at rr_sched 322

VirtualAddress parse_virtual_address(int address)
{
    int digits[32] = {0};
    int count = 0;
    int addr = 0;

    if (address == 0)
    {
        VirtualAddress zero_va;
        zero_va.page = 0;
        zero_va.offset = 0;
        return zero_va;
    }

    while (address != 0 && count < 32)
    {
        digits[count++] = address % 10;
        address /= 10;
    }

    for (int i = count - 1; i >= 0; --i)
        addr = (addr << 1) | digits[i];

    // split into page and offset
    VirtualAddress va;
    va.page = addr >> 4;    // upper 6 bits
    va.offset = addr & 0xF; // lower 4 bits
    return va;
}
// check wether the address is valid and if it is in RAM or not
short check(PCB *pcb, int vpt_address, char req_type)
{
    int pt_address = pcb->frame_index;
    PTE *PT = RAM[pt_address].pte;

    if (vpt_address > pcb->limit)
        return -1; // Invalid

    if (PT[vpt_address].valid)
    {
        PT[vpt_address].R = 1; // Set R bit on access
        if (req_type == 'w')
            PT[vpt_address].M = 1; // Set M bit on write
        int frame_index = PT[vpt_address].frame_address;
        RAM[frame_index].R = 1; // Set R bit on access
        if (req_type == 'w')
            RAM[frame_index].M = 1; // Set M bit on write
        return 1;                   // True
    }
    else
    {
        return 0; // False
    }
}


//directly put the page in the frame without checking anything
void put_page_in_frame(int pid, int page_number, int frame_index,char req_type)
{
    fprintf(memory_log, "Free Physical page %d allocated\n", frame_index);
    printf("At time %d page %d of pid %d was placed at frame %d\n",getClk(),page_number,pid,frame_index);
    PCB *owner = get_process(pid);
    if (owner == NULL || owner->frame_index < 0)
        return;

    RAM[frame_index].occupied = 1;
    RAM[frame_index].process_id = pid;
    RAM[frame_index].pte = NULL;
    RAM[frame_index].R = 1;
    RAM[frame_index].vpage = page_number;
    if(req_type=='w')
    {
        RAM[frame_index].M = 1;
        RAM[owner->frame_index].pte[page_number].M = 1;
    }
    else{
        RAM[frame_index].M = 0;
        RAM[owner->frame_index].pte[page_number].M = 0;
    }
    RAM[owner->frame_index].pte[page_number].valid = 1;
    RAM[owner->frame_index].pte[page_number].R = 1;
    RAM[owner->frame_index].pte[page_number].frame_address = frame_index;
}

// get owner
// return the PCB of the process with the given id, or NULL if not found
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

    return NULL; // not found
}
// define the page table of the process at the given frame index
void define_page_table(int pid, int frame_index)
{
    fprintf(memory_log, "Free Physical page %d allocated\n", frame_index);
    printf("At time %d page table of %d was placed at frame %d\n",getClk(),pid,frame_index);

    PCB *owner = get_process(pid);

    RAM[frame_index].occupied = 1;
    RAM[frame_index].process_id = pid;
    RAM[frame_index].R = 1;
    RAM[frame_index].M = 0;
    RAM[frame_index].vpage = -1;
    RAM[frame_index].pte = (PTE *)malloc(sizeof(PTE) * PAGE_TABLE_SIZE);
    if (RAM[frame_index].pte == NULL)
    {
        perror("malloc failed");
        exit(1);
    }
    for (int i = 0; i < PAGE_TABLE_SIZE; i++)
    {
        RAM[frame_index].pte[i].valid = 0;
        RAM[frame_index].pte[i].R = 0;
        RAM[frame_index].pte[i].M = 0;
        RAM[frame_index].pte[i].frame_address = -1;
    }

    if (owner != NULL)
        owner->frame_index = frame_index;
}

// swap the page in the frame with the new page and update the page table of the owner process

void swap(int id, int frameIndex, int page, int type,char req_type)
{
    
    // set the delay
    int block_end_time = getClk();
    if (page < 64 && type != 2)
        block_end_time = getClk() + page_fault_delay(frameIndex);

    // update last owner page table
    PCB *old_owner = get_process(RAM[frameIndex].process_id);
    if (old_owner != NULL && old_owner->frame_index >= 0 && RAM[frameIndex].vpage >= 0)
    {
        RAM[old_owner->frame_index].pte[RAM[frameIndex].vpage].valid = 0;
        RAM[old_owner->frame_index].pte[RAM[frameIndex].vpage].frame_address = -1;
        RAM[old_owner->frame_index].pte[RAM[frameIndex].vpage].R = 0;
        RAM[old_owner->frame_index].pte[RAM[frameIndex].vpage].M = 0;
    }
    printf("\nswaping framIndex %d from process %d to process %d\n",frameIndex,old_owner->id,id);
    // check if you load a page table or a normal data

    if (type == 0)
    {
        define_page_table(id, frameIndex);
        //fprintf(memory_log, "Swapping out page %d to disk\n", frameIndex);
        return;
    }

    if (memory_log && RAM[frameIndex].M != 0)
    {
        fprintf(memory_log, "Swapping out page %d to disk\n", frameIndex);
    }

    PCB *owner = get_process(id);
    if (owner == NULL)
        return;

    if (type == 2 && owner!=NULL)
    {
        fprintf(memory_log,"At time %d disk address %d for process %d is loaded into memory page %d.\n",
                getClk(), owner->base + page, owner->id, frameIndex);
    }
    if (type == 1 && page < 64)
    {
        owner->state = 'B';
        owner->unblock_at = block_end_time;
    }
    else if (page >= 64)
    {
        page -= 64;
    }
    printf("At time %d page %d of pid %d was placed at frame %d\n",getClk(),page,id,frameIndex);
    RAM[owner->frame_index].pte[page].valid = 1;
    RAM[owner->frame_index].pte[page].frame_address = frameIndex;
    RAM[owner->frame_index].pte[page].R = 1;
    if(req_type=='w')
    {
        RAM[owner->frame_index].pte[page].M = 1;
        RAM[frameIndex].M = 1;
    }
    else{
        RAM[owner->frame_index].pte[page].M = 0;
        RAM[frameIndex].M = 0;
    }
    RAM[frameIndex].process_id = id;
    RAM[frameIndex].R = 1;
    RAM[frameIndex].occupied = 1;
    RAM[frameIndex].vpage = page;
    if(type!=2)
        RAM[frameIndex].reserved = 1;

    if (type == 1)
    {
        owner->pending_page = page;
        owner->pending_frame = frameIndex;
    }
}
// handle faults of reserving data page
void fault_handler(int pid, int page_num, int type, int raw_address,char req_type)
{
    //printf("[fault_handler] Handling page fault for process %d at time %d\n", pid, getClk());
    int cls = 4;
    int victim_index = -1;
    PCB *owner = get_process(pid);
    printf("At time %d page %d of pid %d want to be placed\n",getClk(),page_num,pid);

    if (type == 1 && owner != NULL && page_num < 64)
    {
        owner->state = 'B';
        owner->unblock_at = getClk() + 10;
        owner->pending_page = -1;
        owner->pending_frame = -1;
    }
    if (type == 1 && memory_log)
        fprintf(memory_log, "PageFault upon VA %d from process %d\n", raw_address, pid);

    for (int i = 0; i < MEM_SIZE; i++)
    {
        //printf("i is %d and memSize is %d\n",i,MEM_SIZE);
        if (!RAM[i].occupied)
        {
            if (type == 0)
                define_page_table(pid, i);
            else
            {
                if(type != 2) RAM[i].reserved = 1;
                if (owner != NULL && page_num < 64)
                    owner->unblock_at = getClk() + page_fault_delay(i);
                put_page_in_frame(pid,page_num,i,req_type);
            }

          //  if (type == 1 && memory_log)
               // fprintf(memory_log, "Free Physical page %d allocated\n", i);

            if (type == 1 && owner != NULL)
            {
                owner->pending_page = page_num;
                owner->pending_frame = i;
            }

            if (type == 2 && owner!=NULL)
            {
                fprintf(memory_log,"At time %d disk address %d for process %d is loaded into memory page %d.\n",
                        getClk(), owner->base + page_num, owner->id, i);
            }
            return;
        }
        //printf("frame %d reserve=%d at tick %d \n",i,RAM[i].reserved,getClk());
       // printf("\nAt time %d The victim frameIndex is %d \n",getClk(),victim_index);

        if (RAM[i].pte ||RAM[i].reserved) // pte != null ,that means it is a page table
        {
            continue;
        }

        int curr_cls = 2 * RAM[i].R + RAM[i].M;
        if (curr_cls < cls)
        {
            cls = curr_cls;
            victim_index = i;
            //printf("\nAt time %d The victim frameIndex is %d \n",getClk(),victim_index);
        }
    }

    if (victim_index != -1)
        swap(pid,victim_index,page_num,type,req_type);
}

void clear_recent()
{
    for (int i = 0; i < MEM_SIZE; i++)
    {
        if (RAM[i].occupied)
        {
            RAM[i].R = 0;
            if (RAM[i].pte) // if it's a page table, clear the R bits of its entries as well
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

    return addr / 16 <= pcb->limit ? addr / 16 : -1;
}


void freeReserved(int frameIndex)
{
    RAM[frameIndex].reserved=0;
}


void freePageTable(PCB *finishedProcess)
{
    int frame =finishedProcess->frame_index;
    RAM[frame].M=0;
    RAM[frame].occupied=0;
    RAM[frame].process_id=-1;
    RAM[frame].pte=NULL;
    RAM[frame].R=0;
    RAM[frame].reserved=0;
    RAM[frame].vpage=-1;
}