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
            remainingtime--;
            printf("remaining time: %d\n", remainingtime);
        }
    }

    destroyClk(false);
    kill(getppid(), SIGUSR1);
    return 0;
}
