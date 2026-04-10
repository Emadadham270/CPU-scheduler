#include "../headers.h"
#include "process.h"

/* Modify this file as needed*/
int remainingtime;

int main(int argc, char *argv[])
{
    initClk();

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

    int prev = getClk();
    while (remainingtime > 0)
    {
        int curr = getClk();
        if (prev != curr)
        {
            prev = curr;
            printf("remaining time: %d\n", remainingtime);
            remainingtime--;
        }
    }
    printf("remaining time: %d\n", remainingtime);
    printf("Process finished at time %d\n", getClk());



    destroyClk(false);
    kill(getppid(), SIGUSR1);

    while (1)
    {
        pause();
    }
}
