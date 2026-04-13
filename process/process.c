#include "../headers.h"
#include "process.h"
#include "../data structs/structs.h"
/* Modify this file as needed*/
int remainingtime;
volatile sig_atomic_t prev_clk_tick;

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

    if (argc < 2)
    {
        perror("Error with args");
        kill(getppid(), SIGUSR1);
        exit(-1);
    }

    // TODO it needs to get the remaining time from somewhere
    // remainingtime = ??;
    // We will pass the remaining time as an argument from parent
    if (!to_int(argv[1], &remainingtime))
    {
        kill(getppid(), SIGUSR1);
        exit(-1);
    }
    int sem_id = semget(ftok("../keyfile", 66), 1, 0666);

    // prev_clk_tick = getClk();
    while (remainingtime > 0)
    {
        printf("current process is %d\n", (int)getpid());
        // int curr = getClk();
        // if (prev_clk_tick != curr)
        // {
        // prev_clk_tick = curr;
        //
        down(sem_id);
        printf("%d: remaining time: %d\n", (int)getpid(), remainingtime);
        remainingtime--;

        // }
    }

    printf("Process finished at time %d\n", getClk());

    destroyClk(false);
    kill(getppid(), SIGUSR1);
    //
    // kill(getpid(), SIGTERM);
    return 0;
}
