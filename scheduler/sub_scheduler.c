#include "../headers.h"
#include "sub_scheduler.h"

void stall_sig(int signum); // This function needs a global variable stalled, so it can't be in sub_scheduler.h
void onProcessFinished(int signum);
int stalled = 0;
int stall_end_time = -1;
int processFinishedSignal = 0;

int main(int argc, char* argv[]) {
    signal(SIGUSR2, stall_sig);
    signal(SIGUSR1, onProcessFinished);
}

void stall_sig(int signum) {
    (void) signum;
    
    stalled = 1;
    stall_end_time = getClk() + 3;
    return;
}

void onProcessFinished(int signum)
{
  (void)signum;
  processFinishedSignal = 1;
}
