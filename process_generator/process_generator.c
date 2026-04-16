#include "../headers.h"
#define MAX_N 1024
#include "process_generator.h"
void clearResources(int);

int msgq_id;

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    signal(SIGINT, clearResources);
    key_t key_id;

    key_id = ftok("../keyFile", 65);

    int old_msgq_id = msgget(key_id, 0666);
    if (old_msgq_id != -1)
        msgctl(old_msgq_id, IPC_RMID, (struct msqid_ds *)0);

    msgq_id = msgget(key_id, 0666 | IPC_CREAT | IPC_EXCL);
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
    PGQueue *q = pg_createQueue();

    while (fgets(line, MAX_N, inputFile))
    {
        if (line[0] == '#' || line[0] == '\n')
            continue;
        processData p;
        p.mtype = 1;
        sscanf(line, "%d %d %d %d", &p.id, &p.arrival, &p.runtime, &p.priority);
        pg_enqueue(q, p);
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

    pid_t scheduler_pid = fork();

    if (scheduler_pid == 0)
    {
        if (type == 1)
        {
            execl("../outFiles/scheduler.out", "scheduler.out", tStr, qStr, NULL);
        }
        else if (type == 2)
        {
            execl("../outFiles/scheduler.out", "scheduler.out", tStr, NULL);
        }
        else if (type == 3)
        {
            execl("../outFiles/scheduler.out", "scheduler.out", tStr, nStr, mStr, NULL);
        }

        perror("scheduler execl failed");
        exit(1);
    }
    else if (scheduler_pid < 0)
    {
        perror("fork scheduler failed");
        exit(1);
    }

    pid_t clk_pid = fork();

    if (clk_pid == 0)
    {

        execl("../outFiles/clk.out", "clk.out", NULL);
        perror("clk execl failed");
        exit(1);
    }
    else if (clk_pid < 0)
    {
        perror("fork clk failed");
        exit(1);
    }

    // 4. Use this function after creating the clock process to initialize clock
    initClk();

    // printf("%d",getClk());
    //  To get time use this
    //  int x = getClk();
    //  printf("current time is %d\n", x);
    //  TODO Generation Main Loop
    //  5. Create a data structure for processes and provide it with its parameters.
    //  6. Send the information to the scheduler at the appropriate time.

    int lastTime = -1;
    while (!pg_isEmpty(q))
    {
        int currentTime = getClk();

        if (currentTime != lastTime)
        {
            // printf("Current time: %d\n", currentTime);
            while (!pg_isEmpty(q) && pg_peek(q).arrival <= currentTime)
            {
                processData p = pg_dequeue(q);
                if (msgsnd(msgq_id, &p, sizeof(processData) - sizeof(long), 0) == -1)
                {
                    perror("Error in msgsnd");
                }
            }
        }
        lastTime = currentTime;
    }

    // terminating process
    processData p;
    p.mtype = 5;
    if (msgsnd(msgq_id, &p, sizeof(processData) - sizeof(long), 0) == -1)
    {
        perror("Error in msgsnd");
    }
    pg_freeQueue(q);

    // Wait for scheduler to finish all processes, then stop clock.
    waitpid(scheduler_pid, NULL, 0);
    kill(clk_pid, SIGINT);
    waitpid(clk_pid, NULL, 0);

    // 7. Clear clock communication resources in generator only.
    destroyClk(false);
    msgctl(msgq_id, IPC_RMID, (struct msqid_ds *)0);
    return 0;
}

void clearResources(int signum)
{
    (void)signum;
    // TODO Clears all resources in case of interruption
    msgctl(msgq_id, IPC_RMID, (struct msqid_ds *)0);
    exit(0);
}
