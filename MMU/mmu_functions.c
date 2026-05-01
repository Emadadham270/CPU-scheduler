#include "mmu.h"

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