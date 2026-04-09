#include "../headers.h"
#include "scheduler.h"
#include "../data_structures/PCB/Sch_PCB.h"
int receivingProcesses=1;
Queue* readyQueue;
struct PBC* currProcess=NULL; 
int quantum,N,M;
int main(int argc, char * argv[])
{
    key_t key_id;
    int rec_val, msgq_id;
    key_id = ftok("../keyfile", 65);
    msgq_id = msgget(key_id, 0666 );
    if (msgq_id == -1)
    {
        perror("Error in receive message queue");
        exit(-1);
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
        process=receive(msgq_id);
        if(process.mtype==5)
            receivingProcesses=0;
        else
        {
           struct PCB *pcb=convert(process); 
           enqueue(readyQueue,pcb);
        }


        // if (currProcess != NULL) 
        //     currProcess->remaining_time--;
    }
    
    //TODO implement the scheduler :)
    //upon termination release the clock resources.
    
    destroyClk(true);
}
