#include <stdio.h> //if you don't use scanf/printf change this include
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <limits.h>

typedef short bool;
#define true 1
#define false 0

#define SHKEY 300

///==============================
// don't mess with this variable//
int *shmaddr; //
//===============================

int getClk()
{
    return *shmaddr;
}

/*
 * All process call this function at the beginning to establish communication between them and the clock module.
 * Again, remember that the clock is only emulation!
 */
void initClk()
{
    int shmid = shmget(SHKEY, 4, 0444);
    while ((int)shmid == -1)
    {
        // Make sure that the clock exists
        printf("Wait! The clock not initialized yet!\n");
        sleep(1);
        shmid = shmget(SHKEY, 4, 0444);
    }
    shmaddr = (int *)shmat(shmid, (void *)0, 0);
}

/*
 * All process call this function at the end to release the communication
 * resources between them and the clock module.
 * Again, Remember that the clock is only emulation!
 * Input: terminateAll: a flag to indicate whether that this is the end of simulation.
 *                      It terminates the whole system and releases resources.
 */

void destroyClk(bool terminateAll)
{
    shmdt(shmaddr);
    if (terminateAll)
    {
        killpg(getpgrp(), SIGINT);
    }
}

/*
 * Convert a string to an integer, with error checking
 * returns true for a valid input and false for an invalid one
 * Code from "https://cmu-sei.github.io/secure-coding-standards/sei-cert-c-coding-standard/rules/error-handling-err/err34-c/"
 */

bool to_int(const char *buff, int *return_val)
{
    char *end;

    errno = 0;

    const long sl = strtol(buff, &end, 10);

    if (end == buff)
    {
        (void)fprintf(stderr, "%s: not a decimal number\n", buff);
    }
    else if ('\0' != *end)
    {
        (void)fprintf(stderr, "%s: extra characters at end of input: %s\n", buff, end);
    }
    else if ((LONG_MIN == sl || LONG_MAX == sl) && ERANGE == errno)
    {
        (void)fprintf(stderr, "%s out of range of type long\n", buff);
    }
    else if (sl > INT_MAX)
    {
        (void)fprintf(stderr, "%ld greater than INT_MAX\n", sl);
    }
    else if (sl < INT_MIN)
    {
        (void)fprintf(stderr, "%ld less than INT_MIN\n", sl);
    }
    else
    {
        *return_val = (int)sl;
        return true;
    }
    *return_val = 0;
    return false;
}