#include "../headers.h"
#include "scheduler.h"

int msgq_id;
int receivingProcesses=1;
int context_switch=0;
Queue* readyQueue;
struct PCB* currProcess=NULL; 
int quantum,N,M,current_time;
int processFinishedSignal = 0;

void onProcessFinished(int signum)
{
    (void)signum;
    processFinishedSignal = 1;
}

int main(int argc, char * argv[])
{
    (void)argc;
    key_t key_id;
    signal(SIGINT, cleanup); 
    signal(SIGUSR1, onProcessFinished);
    key_id = ftok("../keyfile", 65);
    msgq_id = msgget(key_id, 0666 );
    if (msgq_id == -1)
    {
        perror("Error in receive message queue");
        exit(1);
    }
    processData process;
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
        // review this 
        N = strtol(argv[1], &e1, 10);
        M = strtol(argv[2], &e2, 10);
    }
        
    initClk();
    current_time = getClk();
    while(!isEmpty(readyQueue) || receivingProcesses || currProcess)
    {
        while (msgrcv(msgq_id, &process, sizeof(processData) - sizeof(long), 0, IPC_NOWAIT) != -1) 
        {
            if(process.mtype == 5) 
            {
                receivingProcesses = 0;
                break;
            }
            
            // Create PCB and add to your Ready Queue
            struct PCB* pcb = (struct PCB*) malloc(sizeof(struct PCB));
            *pcb = createPCB(process); 
            enqueue(readyQueue, pcb);
            
        }

        if (currProcess != NULL && processFinishedSignal)
        {
            int status;
            processFinishedSignal = 0;

            kill(currProcess->pid, SIGTERM);

            while (waitpid(currProcess->pid, &status, 0) == -1)
            {
                if (errno != EINTR)
                {
                    perror("waitpid failed");
                    break;
                }
            }

            currProcess->finish_time = getClk();
            currProcess->remaining_time = 0;
            currProcess->state = 'F';
            free(currProcess);
            currProcess = NULL;
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
            FCFS_algo(readyQueue,&currProcess,N,M);
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

        // handle the correct timing    -------not done yet---------
        wait_one_sec();
    }
    
    //TODO implement the scheduler 
    //upon termination release the clock resources.
    
    destroyClk(true);
}
