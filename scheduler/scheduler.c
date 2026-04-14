#include "scheduler.h"
#include "../headers.h"

int load_sem_id;
int msgq_id;
int sem_id;
int receivingProcesses = 1;
Queue *readyQueue;
struct PCB *currProcess = NULL;
int quantum, N, M;
int processFinishedSignal = 0;
int next_preemtion_time = -1;
int *shmRT_addr;
int *load_shm_addr;
int msgq_sub1_id, msgq_sub2_id, msgq_resp_id;
int shmRT_id,load_shm_id;
int subCpu_created=0;
int idArr[2];
int N_time=0;
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
  perfVars perf = initialize_perf();

  shmRT_id = shmget(ftok("../keyfile", 70), 4, IPC_CREAT | 0666);
  if ((long)shmRT_id == -1)
  {
    perror("Error in creating remaining time shm");
    exit(-1);
  }
  shmRT_addr = (int *)shmat(shmRT_id, (void *)0, 0);
  if ((long)shmRT_addr == -1)
  {
    perror("Error in attaching the shm of RT");
    exit(-1);
  }


  sem_id = semget(ftok("../keyfile", 66), 1, 0666 | IPC_CREAT);
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

      currProcess->remaining_time = *shmRT_addr;
      // log data to scheduler.log
      currProcess->lState = FINISH;
      log_data(log_file, currProcess);

      // Add WTA and Waiting to perf struct
      float WTA = (float)(currProcess->finish_time - currProcess->arrival) / (float)currProcess->runtime;
      perf.avg_WTA += WTA;

      // perform rolling standard deviation
      // https://en.wikipedia.org/wiki/Algorithms_for_calculating_variance#Welford's_online_algorithm
      perf.num_procs++;
      float delta = WTA - perf.welford_mean_WTA;
      perf.welford_mean_WTA += delta / perf.num_procs;
      float delta2 = WTA - perf.welford_mean_WTA;
      perf.M2_WTA += delta * delta2;

      perf.avg_Waiting += currProcess->waiting_time;
      perf.total_runtime += currProcess->runtime;
      perf.finish_time = currProcess->finish_time;

      free(currProcess);
      currProcess = NULL;
      next_preemtion_time = -1;
    }
    else if (!processFinishedSignal)
    {
      // 2. Receive new arrivals
      while (msgrcv(msgq_id, &process, sizeof(processData) - sizeof(long), 0,
                    IPC_NOWAIT) != -1)
      {
        if (process.mtype == 5)
        {
          if (subCpu_created)
          {
            send_process_msg(msgq_sub1_id, &process, 5);
            send_process_msg(msgq_sub2_id, &process, 5);
          }
          receivingProcesses = 0;
          break;
        }

        struct PCB *pcb = (struct PCB *)malloc(sizeof(struct PCB));
        *pcb = createPCB(process);


        // we need the first arrival to calculate CPU utilization (= Finish - first_arrival / total_runtime)
        if(perf.first_arrival == -1) {
          perf.first_arrival = pcb->arrival;
        }
        // printf("recieved process %d\n", pcb->id);
        if (type == 2) // HPF
          enqueue_priority(readyQueue, pcb);
        else
          enqueue(readyQueue, pcb);
              }

      // 3. Run scheduling algorithm (preempt → re-enqueue → dispatch)
      if (now > 0)
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

      if (currProcess != NULL)
      {
        union Semun s;
        s.val = 0;
        semctl(sem_id, 0, SETVAL, s);
        up(sem_id);
      }
    }
  }
  // Wait for sub-schedulers to finish before cleanup
  if (subCpu_created)
  {
    waitpid(idArr[0], NULL, 0);
    waitpid(idArr[1], NULL, 0);
  }
  // upon termination release the clock resources.
  msgctl(msgq_id, IPC_RMID, NULL);
  semctl(sem_id, 0, IPC_RMID);
  write_perf(perf, perf_file);
  shmdt(shmRT_addr);
  shmctl(shmRT_id, IPC_RMID, NULL);
  if (subCpu_created)
    destroy_2cpu_ipcs();
  fclose(log_file);
  fclose(perf_file);
  destroyClk(false);
}
