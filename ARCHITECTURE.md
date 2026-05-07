# CPU Scheduler — Architecture & Logic Flow

---

## 1. System-Level Block Diagram

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         process_generator                               │
│                                                                         │
│  reads input file → builds process queue                                │
│  forks ──► rr_scheduler.out  (or scheduler.out for HPF/FCFS)           │
│  forks ──► clk.out                                                      │
│                                                                         │
│  Per-tick loop:                                                         │
│    sends mtype=1 (processData) for each arriving process                │
│    sends mtype=2 (tick sync, arrival=currentTime)                       │
│    when queue empty → sends mtype=5 (termination sentinel)              │
│  waits for scheduler to exit, then kills clk                            │
└────────────┬────────────────────────────────────────────────────────────┘
             │ msgq (key 65)
             │ mtype=1  processData {id,arrival,runtime,priority,base,limit}
             │ mtype=2  tick sync   {arrival=currentTime}
             │ mtype=5  done sentinel
             ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                          rr_scheduler                                   │
│                                                                         │
│  ┌──────────────┐   fork+execl    ┌────────────────────────────────┐   │
│  │  RR_algo /   │ ──────────────► │   process child (process.out)  │   │
│  │  Scheduler   │                 │                                │   │
│  │  Logic       │ ◄── SIGUSR1 ─── │  counts down remainingtime     │   │
│  │              │   (finished)    │  reads requests_{id}.txt       │   │
│  │              │                 │  when running_time==req.tick:  │   │
│  │              │ ── SIGSTOP ───► │    sends mtype=id on req_msgq  │   │
│  │              │ ── SIGCONT ───► │  decrements shared mem RT      │   │
│  │              │                 │  waits on semaphore each tick  │   │
│  └──────┬───────┘                 └────────────────────────────────┘   │
│         │                                                               │
│         │ req_msgq (key 70)                                             │
│         │ mtype=pid  request {address, operation, tick}                 │
│         ▼                                                               │
│  ┌──────────────┐                                                       │
│  │ handleReqs / │  checkReqs() → parse_hexa_address()                  │
│  │ checkReqs    │  → check() → hit (R/M bits updated)                  │
│  │              │            → miss → fault_handler()                  │
│  └──────┬───────┘                                                       │
│         ▼                                                               │
│  ┌──────────────────────────────────────────────────────────────────┐  │
│  │                       MMU  (RAM[32])                             │  │
│  │                                                                  │  │
│  │  fault_handler: type=0 page table | type=1 demand | type=2 init  │  │
│  │  NRU victim selection: class = 2*R + M, pick lowest class        │  │
│  │  swap: evict victim, delay=20 if dirty else 10                   │  │
│  │  clear_recent: R=0 every k quanta                                │  │
│  └──────────────────────────────────────────────────────────────────┘  │
│                                                                         │
│  Shared mem shmRT: remaining_time (written by process, read by sched)   │
│  Semaphore sem_id: tick gate (scheduler ups, process downs)             │
│  Logs: scheduler.log  scheduler.perf  memory.log                       │
└─────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────┐
│        clk.out          │
│  shared mem SHKEY=300   │
│  increments every 1 sec │
│  getClk() reads it      │
└─────────────────────────┘
```

---

## 2. IPC Channels Summary

| Channel | Type | Key | Sender | Receiver | Payload |
|---------|------|-----|--------|----------|---------|
| `msgq` | Message Queue | ftok(65) | `process_generator` | `rr_scheduler` | processData mtype 1/2/5 |
| `req_msgq` | Message Queue | ftok(70) | `process` child | `rr_scheduler` | request mtype=pid |
| `shmRT` | Shared Memory | ftok(70) | `process` child | `rr_scheduler` | int remainingTime |
| `sem_id` | Semaphore | ftok(66) | `rr_scheduler` | `process` child | tick gate |
| `clk shm` | Shared Memory | SHKEY=300 | `clk.out` | all | int currentTick |
| `SIGUSR1` | Signal | — | `process` child | `rr_scheduler` | process finished |
| `SIGSTOP/CONT` | Signal | — | `rr_scheduler` | `process` child | pause / resume |

---

## 3. RR Scheduler — Per-Tick SFSM

```
                   ┌──────────────────────┐
                   │     SPIN / WAIT      │◄──────────────────────────┐
                   │  while now==last_tick│                           │
                   └──────────┬───────────┘                           │
                              │ [now != last_tick]                    │
                              │ last_tick = now                       │
                              ▼                                       │
             ┌────────────────────────────────────────────┐          │
             │          HANDLE MEMORY REQUESTS            │          │
             │                                            │          │
             │  [next_preemtion_time != -1                │          │
             │   AND now < next_preemtion_time]           │          │
             │  → handleRequests(currProcess.id)          │          │
             │    msgrcv IPC_NOWAIT → if msg:             │          │
             │      enqueue requests[], checkReqs()       │          │
             │                                            │          │
             │  else (quantum boundary):                  │          │
             │  → handleRequests() [drain last msg]       │          │
             │  → checkReqs():                            │          │
             │      peek requests[] front                 │          │
             │      [req.tick <= now]:                    │          │
             │        check(pcb, page, op)                │          │
             │          = 1 hit  → nothing                │          │
             │          = 0 miss → fault_handler(type=1)  │          │
             │                     block PCB              │          │
             │                     currProcess = NULL     │          │
             │          =-1 inv  → ignore                 │          │
             └───────────────────┬────────────────────────┘          │
                                 │                                   │
                                 ▼                                   │
                  ┌──────────────────────────────────┐              │
                  │        CHECK BLOCK END            │              │
                  │  for each PCB in blockQueue:      │              │
                  │  [pcb.unblock_at <= now]:         │              │
                  │    log "At time T disk addr D..." │              │
                  │    freeReserved(frame)            │              │
                  │    pcb.state = W                  │              │
                  │    move PCB → readyQueue          │              │
                  └──────────────┬───────────────────┘              │
                                 │                                   │
                                 ▼                                   │
                  ┌──────────────────────────────────┐              │
                  │     HANDLE FINISHED PROCESS       │              │
                  │  [processFinishedSignal == 1]:    │              │
                  │    waitpid()                      │              │
                  │    log FINISH, update perf stats  │              │
                  │    freePageTable(pcb)             │              │
                  │    currProcess = NULL             │              │
                  │    context_switch_until = now+1   │              │
                  └──────────────┬───────────────────┘              │
                                 │                                   │
           ┌─────────────────────▼──────────────────────────┐       │
           │ [context_switch_until==-1 OR now>=switch_until] │       │
           └──────┬──────────────────────────────────────────┘       │
                  │ [false] ────────────────────────────────────────►─┘
                  │ [true]
                  ▼
   ┌──────────────────────────────────────────────────────┐
   │              RECEIVE PROCESSES                       │
   │  [receivingProcesses AND now > last_received_sync]   │
   │    blocking msgrcv loop:                             │
   │      mtype=1 → createPCB → enqueue readyQueue        │
   │      mtype=2, arrival>=now:                          │
   │        last_received_sync = arrival → break          │
   │      mtype=5 → receivingProcesses=0 → break          │
   └──────────────────────┬───────────────────────────────┘
                          │
                          ▼
          ┌───────────────────────────────────────────┐
          │           RR_ALGO   [now > 0]             │
          └───┬──────────────────────────┬────────────┘
              │                          │
  ┌───────────▼────────────┐  ┌──────────▼─────────────────────────┐
  │ currProcess != NULL    │  │ currProcess == NULL                 │
  └──┬──────────────┬──────┘  │ AND readyQueue not empty            │
     │              │         │   dequeue → runProcess()            │
     │[now <        │[now >=  │   initialize_PCB (if first run):    │
     │ preempt_t]   │preempt] │     fault_handler type=0 (PT)      │
     │              │         │     fault_handler type=2 (page 0)  │
     │keep running  │         │   fork child OR SIGCONT             │
     │              │         │   next_preemtion_time = now+quantum │
     │              │         │   up(semaphore)                     │
     │              │         └─────────────────────────────────────┘
     │              ▼
     │   ┌────────────────────────────────────────────────────────┐
     │   │  PREEMPT CHECK                                         │
     │   │                                                        │
     │   │  [readyQueue empty OR front.arrival==now]:             │
     │   │    extend: next_preemtion_time = now+quantum, return   │
     │   │                                                        │
     │   │  else:                                                 │
     │   │    SIGSTOP currProcess                                 │
     │   │    log STOP, remaining = *shmRT                        │
     │   │    currProcess.state = W                               │
     │   │    enqueue readyQueue                                  │
     │   │    quantums_passed++                                   │
     │   │    [quantums_passed % k == 0] → clear_recent()         │
     │   │    wait_N_secs(1,1)                                    │
     │   │    next_preemtion_time = now+quantum                   │
     │   │    dequeue next → runProcess()                         │
     │   │    up(semaphore)                                       │
     │   └────────────────────────────────────────────────────────┘
     │
     └──────────────────────────────────────────────────────────►SPIN
```

---

## 4. Process Child — Per-Tick SFSM

```
  ┌──────────────────────┐
  │  WAIT on semaphore   │◄─────────────────────────────┐
  │  down(sem_id)        │   [remainingtime > 0]        │
  └──────────┬───────────┘                              │
             │ [sem signalled by scheduler]              │
             ▼                                          │
  ┌──────────────────────────────────────┐             │
  │  [running_time == req[i].tick]       │             │
  │    msgsnd(req_msgq, req, mtype=pid)  │             │
  │    req_index++                       │             │
  │                                      │             │
  │  remainingtime--                     │             │
  │  *shmRT_addr = remainingtime         │             │
  │  running_time++                      │             │
  └──────────┬──────────────┬────────────┘             │
             │[remain > 0]  │[remain == 0]              │
             └──────────────┼───────────────────────────┘
                            ▼
               ┌────────────────────────┐
               │  SIGUSR1 → parent      │
               │  destroyClk() → exit() │
               └────────────────────────┘
```

---

## 5. MMU fault_handler — Decision Tree

```
fault_handler(pid, page, type, raw_addr, req_type)
    │
    ├─ type == 0 ──────────────────────────────────────────────────────────┐
    │                                                                      │
    ├─ type == 1  ► log "PageFault upon VA 0x__ from process P"            │
    │               pcb.state = B                                          │
    │               pcb.unblock_at = now + 10  (tentative)                │
    │               pcb.pending_page = pending_frame = -1                  │
    │                                                                      │
    └─ type == 2  (initial load, no block)                                 │
                                                                           │
    scan RAM[0..31]:                                                       │
      ┌──────────────────────────────────────────────────────────────┐    │
      │ [frame not occupied]                                         │    │
      │   [type==0] ──────────────────────────────────────────────►─┘    │
      │               define_page_table(pid, i)                          │
      │               log "Free Physical page i allocated"               │
      │               pcb.frame_index = i                                │
      │               return                                             │
      │                                                                   │
      │   [type==1]                                                       │
      │     reserved = 1                                                  │
      │     put_page_in_frame(pid, page, i, req_type)                    │
      │     pcb.unblock_at = now + page_fault_delay(i)                   │
      │       [RAM[i].M==1] → delay=20   [M==0] → delay=10              │
      │     pcb.pending_page = page, pcb.pending_frame = i               │
      │     return                                                        │
      │                                                                   │
      │   [type==2]                                                       │
      │     put_page_in_frame(pid, page, i, req_type)                    │
      │     log "At time T disk address D for process P loaded page i"   │
      │     return                                                        │
      └───────────────────────────────────────────────────────────────────┘
      [frame.pte != NULL] → skip (page table, protected)
      [frame.reserved]    → skip (reserved for pending fault)
      else: NRU_class = 2*RAM[i].R + RAM[i].M
            track minimum class → victim_index

    [victim_index != -1] → swap(pid, victim, page, type, req_type)
         │
         ├─ block_end_time = now + page_fault_delay(victim)
         │     [victim.M==1] → 20 ticks
         │     [victim.M==0] → 10 ticks
         │
         ├─ invalidate old owner PTE:
         │     old_owner.PT[victim.vpage].valid = 0
         │     old_owner.PT[victim.vpage].frame = -1
         │     R = M = 0
         │
         ├─ [type==1, victim.M==1] → log "Swapping out page X to disk"
         │
         ├─ [type==2] → log "At time T disk address D for process P..."
         │
         ├─ update RAM[victim]:
         │     process_id=pid, vpage=page, R=1
         │     M = (req_type=='w') ? 1 : 0
         │     reserved = 1 (if type!=2)
         │
         ├─ update new owner PTE:
         │     valid=1, frame_address=victim, R=1, M accordingly
         │
         └─ [type==1]:
               pcb.state = B
               pcb.unblock_at = block_end_time
               pcb.pending_page = page
               pcb.pending_frame = victim
```
