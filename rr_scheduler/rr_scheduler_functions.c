#include "rr_scheduler.h"
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <stdbool.h>
#include <errno.h>

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

struct PCB createPCB(processData p)
{
    struct PCB pcb;

    pcb.id = p.id;
    pcb.arrival = p.arrival;
    pcb.runtime = p.runtime;
    pcb.priority = p.priority;
    pcb.start_time = -1;
    pcb.finish_time = -1;
    pcb.last_stopped = -1;
    pcb.remaining_time = p.runtime;
    pcb.waiting_time = 0;
    pcb.state = 'W';
    pcb.pid = -1;
    pcb.next = NULL;
    pcb.limit = p.limit;
    pcb.base = p.base;
    pcb.frame_index = -1;
    pcb.unblock_at = -1;
    pcb.pending_page = -1;
    pcb.pending_frame = -1;

    return pcb;
}

struct PerfVars initialize_perf()
{
    struct PerfVars perf;

    perf.avg_Waiting = 0.0;
    perf.avg_WTA = 0.0;
    perf.first_arrival = -1;
    perf.num_procs = 0;
    perf.std_WTA = 0.0;
    perf.total_runtime = 0;
    perf.finish_time = -1;
    perf.M2_WTA = 0.0;
    perf.welford_mean_WTA = 0.0;

    return perf;
}

void initialize_PCB(PCB *pcb)
{
    if (pcb->frame_index == -1)
    {
        fault_handler(pcb->id, 0, 0, 0, 'r');
        fault_handler(pcb->id, 0, 2, 0, 'r');
    }
}

void runProcess(struct PCB *pcb, FILE *log_file)
{
    *shmRT_addr = pcb->remaining_time;
    if (pcb->start_time == -1)
    {
        pcb->start_time = getClk();
        pcb->waiting_time = pcb->start_time - pcb->arrival;
    }

    if (pcb->pid == -1)
    {
        initialize_PCB(pcb);
        pcb->lState = START;
        log_data(log_file, pcb);
        // call function to make the init logic ( put the pt table at a free frame and put pt[0] at a free frame

        pid_t pid = fork();
        if (pid == -1)
        {
            perror("fork failed");
            exit(1);
        }

        if (pid == 0)
        {
            char runtime_str[16];
            char shm_str[16];
            char sem_str[16];
            char id_str[16];

            snprintf(runtime_str, sizeof(runtime_str), "%d", pcb->remaining_time);
            snprintf(shm_str, sizeof(shm_str), "%d", shmRT_id);
            snprintf(sem_str, sizeof(sem_str), "%d", sem_id);
            snprintf(id_str, sizeof(id_str), "%d", pcb->id);
            execl("../outFiles/process.out", "process.out", runtime_str, shm_str, sem_str,
                  id_str, (char *)NULL);
            perror("execl failed");
            _exit(1);
        }

        pcb->pid = pid;
    }
    else
    {
        if (pcb->last_stopped >= 0)
            pcb->waiting_time += (getClk() - pcb->last_stopped);
        pcb->lState = RESUME;
        log_data(log_file, pcb);
        kill(pcb->pid, SIGCONT);
    }
    pcb->state = 'R';
}

void cleanup(int signum)
{
    (void)signum;
    if (currProcess != NULL && currProcess->pid > 0)
        kill(currProcess->pid, SIGKILL);

    if (readyQueue != NULL)
    {
        for (PCBNode *node = readyQueue->front; node != NULL; node = node->next)
            if (node->pcb != NULL && node->pcb->pid > 0)
                kill(node->pcb->pid, SIGKILL);
    }

    if (blockQueue != NULL)
    {
        for (PCBNode *node = blockQueue->front; node != NULL; node = node->next)
            if (node->pcb != NULL && node->pcb->pid > 0)
                kill(node->pcb->pid, SIGKILL);
    }

    if (currentPCBs != NULL)
    {
        for (PCBNode *node = currentPCBs->front; node != NULL; node = node->next)
            if (node->pcb != NULL && node->pcb->pid > 0)
                kill(node->pcb->pid, SIGKILL);
    }

    msgctl(req_msgq, IPC_RMID, NULL);
    msgctl(msgq_id, IPC_RMID, NULL);
    semctl(sem_id, 0, IPC_RMID);
    shmdt(shmRT_addr);
    shmctl(shmRT_id, IPC_RMID, NULL);
    destroyClk(false);
    exit(0);
}

void RR_algo(Queue *readyQueue, struct PCB **currProcess, int q,
             int *next_preemtion_time, FILE *log_file)
{
    if (*currProcess != NULL)
    {
        /* Check if the quantum has expired */
        if ((isEmpty(readyQueue) || readyQueue->front->pcb->arrival == getClk()) && getClk() >= *next_preemtion_time)
        {
            *next_preemtion_time = getClk() + q;
            quantums_passed++; // Increment quantum counter when a quantum expires
            return;
        }

        if (getClk() >= *next_preemtion_time)
        {
            /* Preempt: stop the current process and put it back in the queue */
            (*currProcess)->lState = STOP;
            (*currProcess)->remaining_time = *shmRT_addr;
            kill((*currProcess)->pid, SIGSTOP);

            log_data(log_file, *currProcess);
            (*currProcess)->last_stopped = getClk();

            (*currProcess)->remaining_time = *shmRT_addr;
            (*currProcess)->state = 'W';

            quantums_passed++; // Increment quantum counter when a quantum expires
            enqueue(readyQueue, (*currProcess));
            context_switch_until = getClk() + 1;
            if(!checked)checkBlockEnd();
            wait_N_secs(1, 1);
            *next_preemtion_time = getClk() + q;

            /* Pick the next process from the queue */
            *currProcess = dequeue(readyQueue);
            runProcess(*currProcess, log_file);
            return;
        }
    }
    else if (!isEmpty(readyQueue))
    {
        *currProcess = dequeue(readyQueue);
        runProcess(*currProcess, log_file);
        *next_preemtion_time = getClk() + q;
        return;
    }
}

void wait_N_secs(int pen, int N)
{
    int curr = getClk() + pen;
    if (N > 0)
    {
        /* The current tick was already counted before entering this wait.
         * Only account for the additional skipped ticks. */
        int skipped_ticks = (pen > 0) ? (pen - 1) : 0;
        (void)skipped_ticks;
    }
    while (curr > getClk())
    {
    }
}

void create_log_files(FILE **log_file, FILE **perf_file)
{
    mkdir("../logs/", 0755);

    *log_file = fopen("../logs/scheduler.log", "w");
    *perf_file = fopen("../logs/scheduler.perf", "w");

    setvbuf(*log_file, NULL, _IONBF, 0);

    return;
}

void write_comment_line(FILE *log_file)
{
    char *comment = "#At\ttime\tx\tprocess\ty\tstate\tarr\tw\ttotal\tz\tremain\ty\twait\tk\n";
    fprintf(log_file, "%s", comment);
}

void log_data(FILE *log_file, PCB *pcb)
{
    char stateStr[100];
    bool finishFlag = 0;
    int log_time = getClk();

    switch (pcb->lState)
    {
    case START:
        strcpy(stateStr, "started");
        break;
    case FINISH:
        strcpy(stateStr, "finished");
        finishFlag = 1;
        if (pcb->finish_time != -1)
            log_time = pcb->finish_time;
        break;
    case STOP:
        strcpy(stateStr, "stopped");
        break;
    case RESUME:
        strcpy(stateStr, "resumed");
        break;
    default:
        strcpy(stateStr, "state");
        break;
    }

    fprintf(log_file, "At\ttime\t%d\tprocess\t%d\t%s\tarr\t%d\ttotal\t%d\tremain\t%d\twait\t%d", log_time, pcb->id, stateStr, pcb->arrival, pcb->runtime, pcb->remaining_time, pcb->waiting_time);
    if (finishFlag)
    {
        int TA = pcb->finish_time - pcb->arrival;
        float WTA = ((float)TA) / pcb->runtime;
        fprintf(log_file, "\tTA\t%d\tWTA\t%.2f", TA, WTA);
    }
    fprintf(log_file, "\n");
}

void write_perf(struct PerfVars perf, FILE *perf_file)
{
    if (perf.num_procs == 0 || perf.finish_time <= perf.first_arrival)
    {
        fprintf(perf_file, "CPU utilization = 0.00%%\n");
        fprintf(perf_file, "Avg WTA = 0.00\n");
        fprintf(perf_file, "Avg Waiting = 0.00\n");
        fprintf(perf_file, "Std WTA = 0.00\n");
        return;
    }

    float cpu_util = (float)perf.total_runtime * 100.0 / (float)(perf.finish_time - perf.first_arrival);
    perf.avg_WTA /= perf.num_procs;
    perf.avg_Waiting /= perf.num_procs;

    float std_WTA = sqrtf(perf.M2_WTA / perf.num_procs);
    fprintf(perf_file, "CPU utilization = %.2f%%\n", cpu_util);
    fprintf(perf_file, "Avg WTA = %.2f\n", perf.avg_WTA);
    fprintf(perf_file, "Avg Waiting = %.2f\n", perf.avg_Waiting);
    fprintf(perf_file, "Std WTA = %.2f\n", std_WTA);
}

void handleRequests(int *lag)
{
    (void)lag;
    if (currProcess == NULL)
        return;

    request *req = (request *)malloc(sizeof(request));
    if (req == NULL)
    {
        perror("malloc failed");
        exit(1);
    }

    if (msgrcv(req_msgq, req, sizeof(request) - sizeof(long), currProcess->id, IPC_NOWAIT) == -1)
    {
        free(req);
        req = NULL;
    }

    if (req)
    {
        req->tick = getClk();
        enqueueReq(requests, req);
    }
    checkReqs();
}

void checkReqs()
{
    if (!isEmptyReq(requests))
    {
        request *pendingReq = peekReq(requests);
        PCB *requestOwner = get_process((int)pendingReq->mtype);

        if (requestOwner == NULL)
            return;

        VirtualAddress VA = parse_hexa_address(pendingReq->address);
        int result = check(requestOwner, VA.page, pendingReq->operation);
        if (pendingReq->tick <= getClk()) //
        {

            request *currReq = dequeueReq(requests);

            if (result == 1)
            {
            }
            else if (result == 0)
            {
                if (requestOwner == currProcess)
                {
                    currProcess->lState = STOP;
                    quantums_passed++;
                    currProcess->remaining_time = *shmRT_addr;
                    log_data(log_file, currProcess);
                    currProcess->last_stopped = getClk();
                    currProcess->state = 'B';
                    enqueue(blockQueue, currProcess);
                }
                else
                {
                    PCB *blockedProcess = dequeue_by_id(readyQueue, requestOwner->id);
                    if (blockedProcess == NULL)
                    {
                        free(currReq);
                        return;
                    }
                    blockedProcess->state = 'B';
                    enqueue(blockQueue, blockedProcess);
                    requestOwner = blockedProcess;
                }
                int id = requestOwner->id;
                fault_handler(id, VA.page, 1, currReq->address, currReq->operation);
                if (requestOwner == currProcess && currProcess->pid > 0)
                {
                    kill(currProcess->pid, SIGSTOP);
                    context_switch_until = getClk() + 1;
                }
                if (requestOwner == currProcess)
                {
                    context_switch_until = getClk() + 1;
                    currProcess = NULL;
                    next_preemtion_time = -1;
                }
            }
            else
            {
                // Handle invalid address
                free(currReq);
                return;
            }
            free(currReq);
        }
    }
}

void checkBlockEnd()
{
    PCBNode *node = blockQueue->front;
    while (node)
    {
        PCBNode *next = node->next;
        if (node->pcb->unblock_at <= getClk())
        {
            PCB *pcb = dequeue_by_id(blockQueue, node->pcb->id);
            if (pcb != NULL)
            {
                if (pcb->pending_page != -1 && pcb->pending_frame != -1 && memory_log != NULL)
                {
                    fprintf(memory_log, "At time %d disk address %d for process %d is loaded into memory page %d.\n",
                            getClk(), pcb->base + pcb->pending_page, pcb->id, pcb->pending_frame);
                    freeReserved(pcb->pending_frame);
                    pcb->pending_page = -1;
                    pcb->pending_frame = -1;
                }
                pcb->state = 'W';
                enqueue(readyQueue, pcb);
            }
        }
        node = next;
    }
}

void handleFinishedProcesses()
{
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
        context_switch_until = currProcess->finish_time + 1;
        quantums_passed++;

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
        dequeue_by_id(currentPCBs, currProcess->id);
        freePageTable(currProcess);
        free(currProcess);
        currProcess = NULL;
        next_preemtion_time = -1;
    }
}

void receiveProcesses()
{
    int now = getClk();
    while (1)
    {
        processData msg;
        if (msgrcv(msgq_id, &msg, sizeof(processData) - sizeof(long), -5, 0) != -1)
        {
            if (msg.mtype == 1)
            {
                struct PCB *pcb = (struct PCB *)malloc(sizeof(struct PCB));
                *pcb = createPCB(msg);
                pcb->frame_index = -1;
                if (perf.first_arrival == -1)
                    perf.first_arrival = pcb->arrival;
                enqueue(readyQueue, pcb);
                enqueue(currentPCBs, pcb);
            }
            else if (msg.mtype == 2)
            {
                if (msg.arrival >= now)
                {
                    // Record the tick covered by this sync so the main loop
                    // won't call us again until the clock advances past it.
                    last_received_sync = msg.arrival;
                    break;
                }
            }
            else if (msg.mtype == 5)
            {
                receivingProcesses = 0;
                break;
            }
        }
        else
        {
            if (errno != EINTR)
            {
                perror("msgrcv error");
                break;
            }
        }
    }
}
