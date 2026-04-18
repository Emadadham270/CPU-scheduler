#include "../headers.h"
#include "process.h"
#include "../data structs/structs.h"
/* Modify this file as needed*/
int remainingtime;
int shmRT_id;
int sem_id;
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
    
    if (argc < 4)
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
  
    int *shmRT_addr = (int *)shmat(shmRT_id, (void *)0, 0);
    if ((long)shmRT_addr == -1)
    {
        perror("Error in attaching the shm of RT");
        exit(-1);
    }



    // prev_clk_tick = getClk();
    while (remainingtime> 0)
    {
        down(sem_id);
        remainingtime--;
        *shmRT_addr=remainingtime;
    }


    destroyClk(false);
    kill(getppid(), SIGUSR1);
    
    return 0;
}
