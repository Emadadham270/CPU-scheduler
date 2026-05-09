#include "../headers.h"
#include "process.h"
#include "../data structs/structs.h"

/* Modify this file as needed*/
int remainingtime;
int shmRT_id;
int sem_id;
volatile sig_atomic_t prev_clk_tick;
int running_time = 0;
int req_index = 0;
int id = 0;
int req_msgq;

void on_cont(int signum)
{
    (void)signum;
    prev_clk_tick = getClk();
}

void down(int sem)
{
    struct sembuf op;

    op.sem_num = 0;
    op.sem_op = -1;
    op.sem_flg = !IPC_NOWAIT;

    while (semop(sem, &op, 1) == -1)
    {
        if (errno == EINTR)
            continue; // retry if interrupted by signal
        perror("Error in down()");
        exit(-1);
    }
}

void up(int sem)
{
    struct sembuf op;

    op.sem_num = 0;
    op.sem_op = 1;
    op.sem_flg = !IPC_NOWAIT;

    if (semop(sem, &op, 1) == -1)
    {
        perror("Error in up()");
        exit(-1);
    }
}

int main(int argc, char *argv[])
{
    initClk();
    setvbuf(stdout, NULL, _IONBF, 0);
    signal(SIGCONT, on_cont);

    if (argc < 5)
    {
        perror("Error with args");
        kill(getppid(), SIGUSR1);
        exit(-1);
    }

    // We will pass the remaining time as an argument from parent
    if (!to_int(argv[1], &remainingtime))
    {
        kill(getppid(), SIGUSR1);
        exit(-1);
    }

    if (!to_int(argv[2], &shmRT_id))
    {
        kill(getppid(), SIGUSR1);
        exit(-1);
    }

    if (!to_int(argv[3], &sem_id))
    {
        kill(getppid(), SIGUSR1);
        exit(-1);
    }

    if (!to_int(argv[4], &id))
    {
        kill(getppid(), SIGUSR1);
        exit(-1);
    }

    int *shmRT_addr = (int *)shmat(shmRT_id, (void *)0, 0);
    if ((long)shmRT_addr == -1)
    {
        perror("Error in attaching the shm of RT");
        exit(-1);
    }

    // make array of requests [size 100]
    // fill it from the file requests.txt at the input
    char filename[256];
    snprintf(filename, sizeof(filename), "../input/requests_%d.txt", id);
    FILE *file = fopen(filename, "r");
    if (file == NULL)
    {
        snprintf(filename, sizeof(filename), "../input/requests/requests_%d.txt", id);
        file = fopen(filename, "r");
        if (file == NULL)
        {
            perror("Error opening file");
            exit(-1);
        }
    }
    request reqs_arr[100];
    int i = 0;

    char line[1024];
    while (fgets(line, 1024, file))
    {
        if (line[0] == '#' || line[0] == '\n')
            continue;

        // char binary_str[64];
        if (i >= 100)
        {
            fprintf(stderr, "Too many requests in %s, ignoring extra lines\n", filename);
            break;
        }

        if (sscanf(line, "%d %s %c", &reqs_arr[i].tick, reqs_arr[i].address, &reqs_arr[i].operation) != 3)
        {
            fprintf(stderr, "Malformed request line in %s, skipping: %s", filename, line);
            continue;
        }
        reqs_arr[i].mtype = 1;
        i++;
        // reqs_arr[i].address = (int)strtol(binary_str, NULL, 2);
    }
    int num_requests = i;

    int key_id = ftok("../keyFile", 70);
    req_msgq = msgget(key_id, 0666 | IPC_CREAT);

    if (req_msgq == -1)
    {
        perror("Error in request message queue");
        exit(1);
    }

    // prev_clk_tick = getClk();
    while (remainingtime > 0)
    {

        down(sem_id);

        if (remainingtime && req_index < num_requests && running_time == reqs_arr[req_index].tick)
        {
            reqs_arr[req_index].mtype = id;
            if (msgsnd(req_msgq, &reqs_arr[req_index], sizeof(request) - sizeof(long), 0) == -1)
            {
                perror("Error in msgsnd");
            }

            req_index++;
        }
        remainingtime--;
        *shmRT_addr = remainingtime;
        running_time++;
    }

    destroyClk(false);
    kill(getppid(), SIGUSR1);

    return 0;
}
