#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/wait.h>
#include <unistd.h>

static volatile sig_atomic_t got_usr1 = 0;

static void on_usr1(int signum)
{
    (void)signum;
    got_usr1 = 1;
}

int main(void)
{
    struct sigaction sa;
    sa.sa_handler = on_usr1;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGUSR1, &sa, NULL) == -1)
    {
        perror("sigaction");
        return 1;
    }

    pid_t clk_pid = fork();
    if (clk_pid == -1)
    {
        perror("fork clk");
        return 1;
    }

    if (clk_pid == 0)
    {
        execl("./outFiles/clk.out", "clk.out", (char *)NULL);
        perror("execl clk.out");
        _exit(1);
    }

    // Give the clock enough time to create and initialize shared memory.
    sleep(1);

    pid_t proc_pid = fork();
    if (proc_pid == -1)
    {
        perror("fork process");
        kill(clk_pid, SIGINT);
        waitpid(clk_pid, NULL, 0);
        return 1;
    }

    if (proc_pid == 0)
    {
        execl("./outFiles/process.out", "process.out", "2", (char *)NULL);
        perror("execl process.out");
        _exit(1);
    }

    int wait_seconds = 0;
    while (!got_usr1 && wait_seconds < 10)
    {
        sleep(1);
        wait_seconds++;
    }

    if (!got_usr1)
    {
        fprintf(stderr, "FAIL: process.out did not send SIGUSR1 in expected time\n");
        kill(proc_pid, SIGTERM);
        waitpid(proc_pid, NULL, 0);
        kill(clk_pid, SIGINT);
        waitpid(clk_pid, NULL, 0);
        return 1;
    }

    kill(proc_pid, SIGTERM);

    int proc_status = 0;
    while (waitpid(proc_pid, &proc_status, 0) == -1)
    {
        if (errno == EINTR)
        {
            continue;
        }

        perror("waitpid process");
        kill(clk_pid, SIGINT);
        waitpid(clk_pid, NULL, 0);
        return 1;
    }

    kill(clk_pid, SIGINT);
    waitpid(clk_pid, NULL, 0);

    if (!(WIFSIGNALED(proc_status) && WTERMSIG(proc_status) == SIGTERM) &&
        !(WIFEXITED(proc_status) && WEXITSTATUS(proc_status) == 0))
    {
        fprintf(stderr, "FAIL: process.out termination status is unexpected\n");
        return 1;
    }

    printf("process.c integration test: PASS\n");
    return 0;
}
