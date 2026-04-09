#include "../headers.h"
#include "scheduler.h"
#include "../data_structures/PCB/Sch_PCB.h"
int receivingProcesses=1;
int context_switch=0;
Queue* readyQueue;
struct PBC* currProcess=NULL; 
int quantum,N,M,current_time;

int main(int argc, char * argv[])
{
    key_t key_id;
    int rec_val, msgq_id;
    key_id = ftok("../keyfile", 65);
    msgq_id = msgget(key_id, 0666 );
    if (msgq_id == -1)
    {
        perror("Error in receive message queue");
        exit(1);
    }
    struct processData process;
    readyQueue=createQueue();
    char *end;
    int type = strtol(argv[1], &end, 10);
    if (type == 1)
    {
        char *end;
        quantum = strtol(argv[2], &end, 10);
    }
    else if (type == 3)
    {
        char *e1,*e2;
        N = strtol(argv[1], &e1, 10);
        M = strtol(argv[2], &e2, 10);
    }
        
    initClk();
    while(!isEmpty(readyQueue) || receivingProcesses || currProcess)
    {
        current_time = getClk();
        while (!isEmpty(readyQueue)&& peek(readyQueue)->p.arrival==getClk())
        {  
           process=receive(msgq_id);
           if(process.mtype==5)
           {
             receivingProcesses=0;
             break;
           }
           else
           {
           struct PCB* pcb = (PCB*) malloc(sizeof(PCB));
           *pcb=createPCB(process); 
           enqueue(readyQueue,pcb);
           }
        }
       
        switch (type)
        {
        case 1:
            RR_algo(readyQueue,currProcess,quantum);
            break;
        case 2:
            HPF_algo(readyQueue,currProcess);
            break;
        case 3:
            FCFS_algo(readyQueue,currProcess,N,M);
            break;
        default:
            break;
        }
        
        //handle context switch  -------not done yet---------
        if(context_switch)
            handle_context_switch();
        //---------------not sure if it will be done in the schedule-------------------
        //---------------it is already written in the process
        // if (currProcess != NULL) 
        //     currProcess->remaining_time--;
    }
    
    //TODO implement the scheduler :)
    //upon termination release the clock resources.
    
    destroyClk(true);
}
