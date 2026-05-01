#include "mmu.h"

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