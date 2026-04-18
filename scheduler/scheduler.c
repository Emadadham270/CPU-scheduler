#include "scheduler.h"
#include "../headers.h"

int load_sem_id;
int threshold_sem_id = -1;
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
perfVars perf;
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
  key_id = ftok("../keyFile", 65);
  msgq_id = msgget(key_id, 0666);
  perf = initialize_perf();

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

  int last_tick = -1;
  int s=0;
  int wait_sub=0;
  int term_sent_to_subs = 0;
  int first_arrival_sync_done = 0;

  if (type == 3)
  {
      subCpu_created = 1;
      if (create_2cpu_ipcs())
          exit(-1);

      for (int i = 1; i <= 2; i++)
      {
          pid_t pid = fork();
          if (pid == -1)
          {
              perror("fork failed");
              exit(1);
          }
          if (pid == 0)
          {
              char cpu_id_str[16];
              snprintf(cpu_id_str, sizeof(cpu_id_str), "%d", i);
              execl("../outFiles/sub_scheduler.out", "sub_scheduler.out", cpu_id_str, (char *)NULL);
              perror("execl sub_scheduler failed");
              _exit(1);
          }
          idArr[i - 1] = pid;
      }
  }

  initClk();

  int subs_terminated = 0;
  while (!isEmpty(readyQueue) || receivingProcesses || currProcess || wait_sub || (type == 3 && subs_terminated < 2))
  {
    if (type == 3)
    {
        processData ack;
        while (msgrcv(msgq_resp_id, &ack, sizeof(processData) - sizeof(long), 6, IPC_NOWAIT) != -1)
        {
            subs_terminated++;
        }
    }

    int now = getClk();
    if (now == last_tick)
        continue; // spin until next tick
    printf("===========================we are at time step %d ==========================\n", now);
    // Delay 5ms to allow running process to detect the clock change and print
    // its remaining time before we potentially preempt it.

    last_tick = now;

    if (type == 3 && subCpu_created)
    {
      union Semun gate;
      gate.val = 0;
      semctl(threshold_sem_id, 0, SETVAL, gate);
    }

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

      /* One-time synchronization for 2-CPU FCFS:
       * wait within the first active tick so arrivals at time=1 are visible
       * before routing to sub-schedulers. */
      if (type == 3 && !first_arrival_sync_done && now > 0)
      {
        while (getClk() == now)
        {
          int got_any = 0;
          while (!receiveProcesses(readyQueue,process,type))
            got_any = 1;

          if (got_any)
            break;
        }
        first_arrival_sync_done = 1;
      }

      if (now > 0 && type==1)
        {
          //receive the first process before the algo logic then receive after the logic
          if(!s)
            if (!receiveProcesses(readyQueue,process,type))
              s=1;
          RR_algo(readyQueue, &currProcess, quantum, &next_preemtion_time, log_file);

        }

      // 2. Receive new arrivals
      while (!receiveProcesses(readyQueue,process,type));

      // 3. Run scheduling algorithm (preempt → re-enqueue → dispatch)
      if (now > 0 )
        switch (type)
        {
        // case 1:
        //   RR_algo(readyQueue, &currProcess, quantum, &next_preemtion_time, log_file);
        //   break;
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
    if(subCpu_created)
    {
      int c1,c2,rt1,rt2;
      read_all_load_shm(load_shm_addr, &c1, &rt1, &c2, &rt2);
      wait_sub=rt1+rt2;
      
      if (receivingProcesses == 0 && !term_sent_to_subs)
      {
          processData term_msg = {0};
          term_msg.mtype = 5;
          send_process_msg(msgq_sub1_id, &term_msg, 5); // 5 is MTYPE_TERMINATE
          send_process_msg(msgq_sub2_id, &term_msg, 5);
          term_sent_to_subs = 1;
      }
    }
  }
  // Wait for sub-schedulers to finish before cleanup
  if (subCpu_created)
  {
    // Prevent deadlock: provide one last round of permits so any sub-scheduler 
    // that reached the next tick but was abandoned by main can evaluate its condition and exit cleanly.
    union Semun gate;
    gate.val = 2;
    semctl(threshold_sem_id, 0, SETVAL, gate);
    
    printf("\tMAIN: waiting for sub-scheduler 1 (PID: %d)...\n", idArr[0]);
    waitpid(idArr[0], NULL, 0);
    printf("\tMAIN: sub-scheduler 1 exited!\n");
    
    printf("\tMAIN: waiting for sub-scheduler 2 (PID: %d)...\n", idArr[1]);
    waitpid(idArr[1], NULL, 0);
    printf("\tMAIN: sub-scheduler 2 exited!\n");
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
  semctl(load_sem_id, 0, IPC_RMID);

}
