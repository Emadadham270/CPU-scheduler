#include "../headers.h"
#define MAX_N 1024
#include "process_generator.h"
void clearResources(int);

int msgq_id;

int main(int argc, char *argv[])
{
    signal(SIGINT, clearResources);
    key_t key_id;

    key_id = ftok("../keyfile", 65);

    msgq_id = msgget(key_id, 0666 | IPC_CREAT);
    if (msgq_id == -1)
    {
        perror("Error in create message queue");
        exit(-1);
    }
    // TODO Initialization
    // 1. Read the input files.
    FILE *inputFile = fopen("input/processes.txt", "r");
    if (inputFile == NULL)
    {
        // Support running from process_generator directory as well.
        inputFile = fopen("../input/processes.txt", "r");
    }
    if (inputFile == NULL)
    {
        perror("Error opening input/processes.txt");
        return 1;
    }

    char line[MAX_N];
    Queue *q = createQueue();

    while (fgets(line, MAX_N, inputFile))
    {
        if (line[0] == '#')
            continue;
        processData p;
        p.mtype = 1;
        sscanf(line, "%d %d %d %d", &p.id, &p.arrival, &p.runtime, &p.priority);
        enqueue(q, p);
    }
    fclose(inputFile);
    // 2. Ask the user for the chosen scheduling algorithm and its parameters, if there are any.
    printf("Choose the Scheduling Algorithm \n1. RR\n2. HPF\n3. FCFS (2 CPUs)\n");
    int type;
    char tStr[10], qStr[20], nStr[20], mStr[20];
    scanf("%d", &type);
    sprintf(tStr, "%d", type);

    int quantum, N, M;

    if (type == 1)
    {
        printf("Enter the time quantum of RR\n");
        scanf("%d", &quantum);
        sprintf(qStr, "%d", quantum);
    }
    else if (type == 3)
    {
        printf("Enter N\n");
        scanf("%d", &N);
        sprintf(nStr, "%d", N);
        printf("Enter M\n");
        scanf("%d", &M);
        sprintf(mStr, "%d", M);
    }
    // 3. Initiate and create the scheduler and clock processes.

    pid_t pid = fork();

    if (pid == 0)
    {
        if (type == 1)
        {
            execl("outFiles/scheduler.out", "scheduler.out", tStr, qStr, NULL);
        }
        else if (type == 2)
        {
            execl("outFiles/scheduler.out", "scheduler.out", tStr, NULL);
        }
        else if (type == 3)
        {
            execl("outFiles/scheduler.out", "scheduler.out", tStr, nStr, mStr, NULL);
        }

        perror("scheduler execl failed");
        exit(1);
    }

    pid = fork();

    if (pid == 0)
    {

        execl("outFiles/clk.out", "clk.out", NULL);
        perror("clk execl failed");
        exit(1);
    }

    // 4. Use this function after creating the clock process to initialize clock
    initClk();
    // To get time use this
    // int x = getClk();
    // printf("current time is %d\n", x);
    // TODO Generation Main Loop
    // 5. Create a data structure for processes and provide it with its parameters.
    // 6. Send the information to the scheduler at the appropriate time.

    int lastTime = -1;
    while (!isEmpty(q))
    {
        int currentTime = getClk();

        if (currentTime != lastTime)
        {
            // printf("Current time: %d\n", currentTime);
            while (!isEmpty(q) && peek(q).arrival <= currentTime)
            {
                processData p = dequeue(q);
                if (msgsnd(msgq_id, &p, sizeof(processData) - sizeof(long), 0) == -1)
                {
                    perror("Error in msgsnd");
                }
            }
        }
        lastTime = currentTime;
    }

    freeQueue(q);
    // wait(NULL);
    // wait(NULL);

    // 7. Clear clock resources
    destroyClk(true);
}

void clearResources(int signum)
{
    // TODO Clears all resources in case of interruption
    msgctl(msgq_id, IPC_RMID, (struct msqid_ds *)0);
    exit(0);
}
