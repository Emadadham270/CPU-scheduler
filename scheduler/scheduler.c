#include "scheduler.h"
#include "../headers.h"

int msgq_id;
int receivingProcesses = 1;
// int context_switch = 0;
Queue *readyQueue;
struct PCB *currProcess = NULL;
int quantum, N, M;
int processFinishedSignal = 0;
int next_preemtion_time = -1;
int dispatched_this_tick = 0;

void onProcessFinished(int signum)
{
  (void)signum;
  processFinishedSignal = 1;
}

int main(int argc, char *argv[])
{
  (void)argc;
  key_t key_id;
  signal(SIGINT, cleanup);
  signal(SIGUSR1, onProcessFinished);
  key_id = ftok("../keyfile", 65);
  msgq_id = msgget(key_id, 0666);

  int sem_id = semget(ftok("../keyfile", 66), 1, 0666 | IPC_CREAT);
  if (sem_id == -1)
  {
    perror("Error in create sem");
    exit(-1);
  }
  union Semun semun;
  semun.val = 0;
  if (semctl(sem_id, 0, SETVAL, semun) == -1)
  {
    perror("Error in semctl");
    exit(-1);
  }
  if (msgq_id == -1)
  {
    perror("Error in receive message queue");
    exit(1);
  }
  processData process;
  readyQueue = createQueue();
  char *end;
  int type = strtol(argv[1], &end, 10);
  if (type == 1)
  {
    char *e;
    quantum = strtol(argv[2], &e, 10);
  }
  else if (type == 3)
  {
    char *e1, *e2;
    // review this
    N = strtol(argv[2], &e1, 10);
    M = strtol(argv[3], &e2, 10);
  }

  FILE *log_file, *perf_file;
  create_log_files(&log_file, &perf_file);
  write_comment_line(log_file);

  initClk();
  int ready_sem = semget(ftok("../keyfile", 67), 1, 0666 | IPC_CREAT);
  up(ready_sem);
  int last_tick = -1;

  while (!isEmpty(readyQueue) || receivingProcesses || currProcess)
  {
    int now = getClk();
    if (now == last_tick)
      continue; // spin until next tick
    printf("===========================we are at time step %d ==========================\n", now);
    // Delay 5ms to allow running process to detect the clock change and print
    // its remaining time before we potentially preempt it.
    // usleep(5000);

    last_tick = now;

    // 1. Handle finished process (before algo, so we don't preempt a dead process)
    if (currProcess != NULL && processFinishedSignal)
    {
      int status;
      processFinishedSignal = 0;

      while (waitpid(currProcess->pid, &status, 0) == -1)
      {
        if (errno != EINTR)
        {
          perror("waitpid failed");
          break;
        }
      }

      currProcess->finish_time = getClk();
      currProcess->remaining_time = 0;
      currProcess->state = 'F';
      currProcess->lState = FINISH;
      log_data(log_file, currProcess);
      free(currProcess);
      currProcess = NULL;
      next_preemtion_time = -1;
    }

    if (currProcess)
    {
      printf("current process is %d\n", currProcess->pid);
    }

    // 2. Receive new arrivals
    while (msgrcv(msgq_id, &process, sizeof(processData) - sizeof(long), 0,
                  IPC_NOWAIT) != -1)
    {
      if (process.mtype == 5)
      {
        receivingProcesses = 0;
        break;
      }

      struct PCB *pcb = (struct PCB *)malloc(sizeof(struct PCB));
      *pcb = createPCB(process);
      // printf("recieved process %d\n", pcb->id);
      if (type == 2) // HPF
        enqueue_priority(readyQueue, pcb);
      else
        enqueue(readyQueue, pcb);
    }

    // 3. Run scheduling algorithm (preempt → re-enqueue → dispatch)
    switch (type)
    {
    case 1:
      RR_algo(readyQueue, &currProcess, quantum, &next_preemtion_time, log_file);
      break;
    case 2:
      HPF_algo(readyQueue, &currProcess, log_file);
      break;
    case 3:
      FCFS_algo(readyQueue, &currProcess, N, M, log_file);
      break;
    default:
      break;
    }

    /* Tick gate: signal the running process to execute exactly one unit of work.
     * We reset the semaphore to 0 first to discard any stale credits that may
     * have accumulated (e.g. from a previous process), then post once so the
     * process unblocks, decrements its remaining time, and blocks again.
     * We skip this if we just dispatched this tick (dispatched_this_tick=1)
     * because the process hasn't reached down() yet — firing up() too early
     * would let it consume the credit instantly on the same tick it was forked,
     * causing it to run twice in one tick. */

    if (currProcess != NULL && !dispatched_this_tick)
    {
      union Semun s;
      s.val = 0;
      semctl(sem_id, 0, SETVAL, s);
      up(sem_id);
    }
    dispatched_this_tick = 0; // reset every tick
  }
  // TODO implement the scheduler
  // upon termination release the clock resources.
  msgctl(msgq_id, IPC_RMID, NULL);
  // destroyClk(true);
}
