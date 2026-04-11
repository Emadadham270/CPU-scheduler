#include "../headers.h"
#include "process.h"

/* Modify this file as needed*/
int remainingtime;
volatile sig_atomic_t prev_clk_tick;

void on_cont(int signum) {
    (void)signum;
    prev_clk_tick = getClk();
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

    prev_clk_tick = getClk();
    while (remainingtime > 0)
    {
        int curr = getClk();
        if (prev_clk_tick != curr)
        {
            prev_clk_tick = curr;
            remainingtime--;
            printf("%d: remaining time: %d\n",(int) getpid(), remainingtime);
        }
    }
    
    printf("Process finished at time %d\n", getClk());



    destroyClk(false);
    kill(getppid(), SIGUSR1);
    //
    kill(getpid(), SIGTERM);

}
