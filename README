# CPU Scheduler with Demand Paging

An operating systems simulation project implementing a multi-algorithm CPU scheduler with integrated demand paging memory management, built entirely in C using POSIX inter-process communication primitives on Linux.

---

## Table of Contents

1. [Project Overview](#project-overview)
2. [System Architecture](#system-architecture)
3. [Scheduling Algorithms](#scheduling-algorithms)
4. [Memory Management](#memory-management)
5. [Inter-Process Communication](#inter-process-communication)
6. [Input Format](#input-format)
7. [Output and Logging](#output-and-logging)
8. [Build Instructions](#build-instructions)
9. [Running the Simulator](#running-the-simulator)
10. [Cleaning the Build](#cleaning-the-build)
11. [Design Decisions and Rationale](#design-decisions-and-rationale)
12. [Project Structure](#project-structure)
13. [Development Team](#development-team)

---

## Project Overview

This project simulates a complete operating system scheduling subsystem, covering process lifecycle management, CPU scheduling under multiple policies, demand-paged virtual memory, and real-time performance reporting. The simulation is composed of several independent processes that communicate exclusively through POSIX IPC mechanisms, mirroring the structure of a real kernel scheduler without relying on any threading library.

The simulator runs in discrete clock ticks. A dedicated clock process advances a shared counter each second of wall time; all other components read this counter to synchronize their behavior deterministically.

---

## System Architecture

The system is composed of six distinct process roles:

```
                         +-------------------+
                         |  Process Generator |
                         |  (orchestrator)    |
                         +--------+----------+
                                  |
               +------------------+------------------+
               |                                     |
     +---------+----------+              +-----------+--------+
     |  Clock  (clk.out)  |              |  Scheduler          |
     |  shared-memory tick|              |  rr_scheduler.out   |
     +--------------------+              |  scheduler.out      |
                                         +-----------+--------+
                                                     |        |
                              +----------------------+        | 
                              |                      |        |
                    +---------+-----+    +-----------+-----+  |
                    | Sub-Scheduler |    | Sub-Scheduler   |  |
                    |    CPU 1      |    |    CPU 2        |  |
                    +---------------+    +-----------------+  |
                              |                    |          |
                              +-----------------------------------------------------+
                                                                                    |

                                                                           +---------+-----+
                                                                           | Process Child  |
                                                                           | (process.out)  |
                                                                           +---------------+
```
### Component Responsibilities

| Component | Binary | Role |
|---|---|---|
| Clock | `clk.out` | Maintains a global tick counter in shared memory. Advances once per second. |
| Process Generator | `process_generator.out` | Reads the input file, prompts the user for scheduling parameters, forks the scheduler and clock, then dispatches processes to the scheduler via message queue at their declared arrival times. |
| Scheduler (HPF / FCFS) | `scheduler.out` | Implements Highest Priority First and dual-CPU FCFS scheduling. Manages process dispatch, preemption signals, and performance logging. |
| RR Scheduler | `rr_scheduler.out` | Implements Round Robin scheduling with integrated MMU calls for demand paging. Manages the block queue for faulted processes. |
| Sub-Scheduler | `sub_scheduler.out` | One instance per virtual CPU in dual-CPU FCFS mode. Each runs FCFS independently and supports work-stealing. |
| Process Child | `process.out` | Simulates a running process. Decrements remaining time each tick via shared memory, sends memory access requests, and signals the scheduler upon completion. |

---

## Scheduling Algorithms

### 1. Round Robin (RR)

Selected by entering `1` at the algorithm prompt.

Round Robin is a preemptive, time-sharing algorithm. Each process is granted a fixed time quantum. Upon quantum expiry the running process receives `SIGSTOP` and is re-enqueued at the tail of the ready queue; the next process is dequeued and dispatched.

**Parameters required:**
- **Quantum (q):** Number of clock ticks each process may run before preemption.
- **R-bit clear interval (k):** Every k completed quantums, the Referenced bit of all page table entries is cleared to support the NRU page replacement policy.

**Key behavior:**
- A one-tick context-switch penalty is enforced between consecutive process switches to simulate realistic overhead.
- The scheduler performs a per-tick poll of the block queue and unblocks any process whose page-fault delay has elapsed before running the scheduling decision.
- Memory access requests submitted by the running process mid-quantum are intercepted and forwarded to the MMU before the quantum boundary is evaluated.

**Rationale for RR:** Round Robin provides fair CPU sharing among all ready processes without risk of starvation. It is the natural companion to demand paging because its periodic quantum boundaries create predictable opportunities to service page faults without indefinitely blocking the scheduler loop.

---

### 2. Highest Priority First (HPF)

Selected by entering `2` at the algorithm prompt.

HPF is a non-preemptive priority scheduler. The ready queue is maintained as a priority-sorted linked list ordered by the process priority field (lower numeric value equals higher priority). Each time the CPU becomes free, the process at the head of the sorted queue is dispatched and runs to completion without interruption.

**Rationale for HPF:** A non-preemptive priority discipline minimizes context-switch overhead in workloads where processes have well-differentiated priority levels. It is appropriate when higher-priority work must not be delayed by lower-priority contention.

---

### 3. First Come First Served with Dual CPU (FCFS)

Selected by entering `3` at the algorithm prompt.

This mode instantiates two independent sub-scheduler processes, each managing a dedicated virtual CPU. The main scheduler acts as a load balancer, routing newly arrived processes to the sub-scheduler whose current total remaining work (measured as the sum of remaining times of all queued and running processes) is smallest at each tick.

**Parameters required:**
- **N:** The total remaining work threshold below which a sub-scheduler is considered underloaded.
- **M:** The work threshold above which a sub-scheduler is considered overloaded and eligible to have a process stolen.

**Work stealing:** When a sub-scheduler finishes all its processes and detects that the other sub-scheduler is overloaded (total remaining work exceeds M), it sends `SIGUSR2` to the overloaded peer. The peer responds by dequeuing the process at the rear of its ready queue and transmitting its descriptor back through the response message queue. This rebalances load without idle-spinning.

**Stall mechanism:** When a sub-scheduler receives a stolen process it enters a three-tick stall period before dispatching, simulating the cache warm-up cost of migrating a process across CPUs.

**Rationale for dual-CPU FCFS:** FCFS avoids preemption overhead and ensures each process runs in arrival order on its assigned CPU. The dual-CPU extension doubles throughput for CPU-bound workloads with comparable burst lengths.

---

## Memory Management

### Demand Paging with NRU Replacement

The RR scheduler integrates a software Memory Management Unit (`MMU/mmu_functions.c`) that simulates demand paging over a physical memory of 32 frames (512 bytes total, 16 bytes per page).

Each process is assigned a contiguous virtual address space defined by a base address and a limit from the input file. Virtual addresses submitted as memory access requests are translated by the MMU into a page number and offset.

#### Page Table

Each process holds a page table allocated in a dedicated physical frame, permanently resident for the duration of the process lifetime. The page table entry (PTE) structure tracks:

| Field | Type | Description |
|---|---|---|
| `valid` | short | Whether the page is currently in physical memory. |
| `frame_address` | int | Physical frame index holding this page. |
| `R` | short | Referenced bit, set on every access, cleared every k quantums. |
| `M` | short | Modified bit, set on write accesses, governs swap-out cost. |

#### Page Fault Handling

When a process accesses a virtual address whose PTE is not valid, the MMU invokes `fault_handler`. The sequence is:

1. The faulting process's PCB state is set to `B` (blocked).
2. A free physical frame is sought. If one exists, the page is loaded immediately.
3. If no free frame exists, the NRU algorithm selects a victim frame:
   - Frames are classified into four classes based on their `(R, M)` pair: class 0 = (0,0), class 1 = (0,1), class 2 = (1,0), class 3 = (1,1).
   - The lowest-class, non-reserved, non-page-table frame is evicted.
   - If the victim's `M` bit is set, a write-back to disk is simulated (20-tick delay); otherwise, the page is simply invalidated (10-tick delay).
4. The PCB records `unblock_at` to the current tick plus the computed delay. The scheduler polls this value each tick and moves the process back to the ready queue when the delay has elapsed.

#### R-bit Clearance

Every k completed quantums, `clear_recent()` scans all 32 frames and resets the `R` bit, giving recently unused pages the opportunity to fall into lower NRU classes and be evicted before more recently accessed pages.

---

## Inter-Process Communication

The project uses four distinct POSIX IPC mechanisms. All IPC resources are keyed via `ftok("keyFile", <id>)` to guarantee consistent identification across independently forked processes.

### 1. Shared Memory — Clock Tick Counter

**Key:** `ftok("keyFile", 300)` (SHKEY)  
**Size:** 4 bytes (one `int`)  
**Owner:** `clk.out`  
**Readers:** All other processes via `initClk()` / `getClk()`

The clock process creates this segment and increments its value once per second. Every other process attaches to it read-only. This is the single source of time truth for the entire simulation. No process ever blocks on a lock for this read; the value is sampled in a tight spin-wait loop until it advances.

---

### 2. Shared Memory — Remaining Time

**Key:** `ftok("keyfile", 70)`, size 4 bytes  
**Owner / Writer:** The running process child (`process.out`)  
**Reader:** The scheduler

Each tick the running child decrements its remaining time counter and writes it to this segment. When the quantum expires or the process finishes, the scheduler reads this value to record accurate remaining-time figures in the log without requiring a message to be sent.

---

### 3. Shared Memory — Load Information (FCFS dual-CPU only)

**Key:** `ftok("keyfile", 80)`, size `4 * sizeof(int)`  
**Layout:** `[count_cpu1, totalRT_cpu1, count_cpu2, totalRT_cpu2]`  
**Writers:** Both sub-schedulers, protected by `load_sem_id`  
**Reader:** Main scheduler

Updated every tick by each sub-scheduler. The main scheduler reads this array to decide which CPU receives the next arriving process and which CPU is eligible for work stealing.

---

### 4. Message Queue — Process Arrival

**Key:** `ftok("keyFile", 65)`  
**Creator:** Process generator  
**Sender:** Process generator  
**Receiver:** Scheduler

Process descriptors are transmitted as `processData` structs. Three message types are used:

| `mtype` | Meaning |
|---|---|
| `1` | A new process has arrived; the payload contains `id`, `arrival`, `runtime`, `priority`, `base`, `limit`. |
| `2` | A tick-synchronization sentinel; the scheduler drains all `mtype <= 2` messages up to and including the sentinel whose `arrival` field matches or exceeds the current tick, then breaks. This ensures process arrivals and the scheduler's ready-queue admission are aligned to the same clock tick. |
| `5` | All processes have been dispatched; the generator has finished reading the input file. |

**Tick synchronization rationale:** Without a sentinel mechanism, the scheduler could spin-receive on the queue and consume tick-N arrivals during tick N-1, causing processes to appear to the scheduler one tick early. The mtype-2 sentinel acts as a barrier: the scheduler blocks on `msgrcv` with a priority filter (`-5`, meaning receive any message with mtype <= 5) and only exits the receive loop when it sees the sentinel for the current tick or the termination signal.

---

### 5. Message Queue — Memory Access Requests

**Key:** `ftok("keyFile", 70)`  
**Creator / Receiver:** RR scheduler  
**Sender:** Running process child

Each process is given a per-process request file (`input/requests/requests_<id>.txt`) listing `(tick, virtual_address, operation)` tuples. When the running process's internal tick counter matches an entry's tick field, it sends a `request` message with `mtype = pid`. The scheduler polls this queue each clock tick with `IPC_NOWAIT` and forwards valid requests to the MMU.

---

### 6. Message Queues — Sub-Scheduler Routing (FCFS dual-CPU only)

**Sub-scheduler input queues:** `msgq_sub1_id`, `msgq_sub2_id`  
**Response / ack queue:** `msgq_resp_id`

The main scheduler routes arriving processes to sub-schedulers by sending `processData` messages into the appropriate sub-scheduler input queue. Sub-schedulers send acknowledgment messages (mtype 6) on `msgq_resp_id` upon termination. Stolen process descriptors are also returned on this queue (mtype 11 on success, mtype 12 if nothing was available to steal).

---

### 7. Semaphore — Process Dispatch Gate

**Key:** `ftok("keyfile", 66)`, count 1  
**Creator:** Scheduler  
**Up (signal):** Scheduler, after each dispatch decision  
**Down (wait):** Running process child, at the top of its tick loop

This binary semaphore is the execution gate. The scheduler resets it to 0 immediately before the dispatch call, then performs a single `up` (V operation) to grant the running process exactly one tick of execution. The process child calls `down` (P operation) at the top of each iteration and only proceeds to simulate one tick of work after obtaining the semaphore. This eliminates busy-waiting in the child and ensures the scheduler retains full control over process execution tempo.

---

### 8. Semaphore — Threshold Gate (FCFS dual-CPU only)

**Key:** `ftok("keyfile", 67)`, count 1  
**Creator / Setter:** Main scheduler, set to 0 at the start of each tick  
**Up:** Main scheduler, set to 2 (one permit per sub-scheduler) after routing  
**Down:** Each sub-scheduler, once per tick before acting

This semaphore coordinates tick-level synchronization between the main scheduler and the two sub-schedulers. At the top of each tick the main scheduler resets the semaphore to 0, performs load-balancing routing, then raises it to 2. Each sub-scheduler blocks on a `down` until it receives its permit, ensuring no sub-scheduler advances past a tick boundary before the main scheduler has finished routing that tick's arrivals.

---

### 9. Semaphore — Load Shared Memory Mutex (FCFS dual-CPU only)

**Key:** `ftok("keyfile", 68)`, count 1  
**Purpose:** Mutual exclusion for reads and writes to the load shared memory segment

Both sub-schedulers update the load array concurrently. This semaphore prevents torn reads by the main scheduler when both sub-schedulers update their respective slots simultaneously.

---

### Signal Usage

| Signal | Sender | Receiver | Meaning |
|---|---|---|---|
| `SIGSTOP` | Scheduler | Process child | Pause execution at quantum expiry |
| `SIGCONT` | Scheduler | Process child | Resume execution after re-dispatch |
| `SIGUSR1` | Process child | Scheduler | Process has completed (remaining time reached zero) |
| `SIGUSR2` | Main scheduler | Sub-scheduler | Initiate work-steal: dequeue rear of ready queue and return it |
| `SIGINT` | Process generator | Process group | Terminate all processes on interrupt or simulation end |
| `SIGINT` | Last process to finish | Entire group | `destroyClk(true)` propagates termination to all members |

---

## Input Format

### Process File

Located in `input/`. Each non-comment line describes one process:

```
# id   arrival   runtime   priority   base   limit
1      0         5         3          0      10
2      2         3         1          50     10
```

| Field | Type | Description |
|---|---|---|
| `id` | int | Unique process identifier |
| `arrival` | int | Clock tick at which the process arrives |
| `runtime` | int | Total CPU burst length in ticks |
| `priority` | int | Scheduling priority (lower value = higher priority, used by HPF) |
| `base` | int | Virtual address space base (in bytes) |
| `limit` | int | Virtual address space size (in bytes) |

Lines beginning with `#` are treated as comments and ignored.

### Memory Request Files

For RR simulations, each process with id `n` may have a corresponding file at `input/requests/requests_n.txt`. Each line specifies one memory access:

```
# tick   virtual_address   operation
1        0x00              r
3        0x10              w
```

`operation` is `r` for read or `w` for write. The process child reads this file at startup and sends each request to the scheduler at the appropriate tick.

---

## Output and Logging

All output files are written to the `logs/` directory, which is created automatically if absent.

### scheduler.log / scheduler_N.log

Records every scheduling event in tab-separated format:

```
At  time  <T>  process  <id>  <event>  arr  <A>  total  <R0>  remain  <RT>  wait  <W>  [TA  <TA>  WTA  <WTA>]
```

| Field | Description |
|---|---|
| `T` | Clock tick of the event |
| `id` | Process identifier |
| `event` | One of `started`, `resumed`, `stopped`, `finished` |
| `A` | Process arrival time |
| `R0` | Original runtime |
| `RT` | Remaining time at event time |
| `W` | Accumulated waiting time |
| `TA` | Turnaround time (finish - arrival), appended on `finished` events |
| `WTA` | Weighted turnaround time (TA / runtime), appended on `finished` events |

### scheduler.perf / scheduler_N.perf

Summary performance metrics written after all processes complete:

```
CPU utilization = <X>%
Avg WTA = <Y>
Avg Waiting = <Z>
Std WTA = <S>
```

The standard deviation of WTA is computed using Welford's online algorithm to avoid a second pass over the data.

### memory.log

Records all MMU events produced by the RR scheduler:

```
Free Physical page <F> allocated
At time <T> disk address <D> for process <P> is loaded into memory page <F>.
PageFault upon VA <addr> from process <P>
Swapping out page <V> to disk
```

---

## Build Instructions

Prerequisites: GCC, GNU Make, and a Linux environment (native or WSL).

```bash
# Compile all binaries
make

# Compile a specific target only
make rr_scheduler
make scheduler
make process_generator
make clk
```

All compiled binaries are placed in `outFiles/`.

---

## Running the Simulator

```bash
make run-all
```

This compiles the process generator if necessary and launches it. The generator is the single entry point; it subsequently forks and executes all other components. The generator prompts for:

1. **Input file name** — relative to the `input/` directory (e.g., `processes.txt`).
2. **Scheduling algorithm** — `1` for RR, `2` for HPF, `3` for FCFS (dual-CPU).
3. **Additional parameters** — quantum and k for RR; N and M for dual-CPU FCFS.

The simulation runs until all processes have completed, then terminates cleanly and writes the performance report.



This pipes predefined parameters to the generator using `printf` and launches the session in a detached shell.

---

## Cleaning the Build

```bash
make clean
```

This removes all compiled binaries from `outFiles/` and deletes the intermediate object file directory. Source files and input/output data are not affected. Log files in `logs/` are preserved between runs and are overwritten only when a new simulation executes.

To also remove IPC resources that may have been left behind by an interrupted run, execute:

```bash
ipcrm --all
```

This clears all System V message queues, shared memory segments, and semaphores belonging to the current user. Use with care in shared environments.

---

## Design Decisions and Rationale

### Discrete Tick Simulation via Shared Memory Clock

Rather than relying on `sleep` or timer signals within each process, all components read a single integer from shared memory that the clock process increments once per second. This guarantees a globally consistent view of time without drift or race conditions introduced by independent timers. Each component implements a spin-wait (`while (getClk() == last_tick) ;`) to detect the tick boundary, consuming negligible CPU given the one-second tick granularity.

### Tick-Synchronization Sentinel Messages

A naive approach to arrival dispatch would be to have the generator continuously `msgsnd` process arrivals and have the scheduler consume them at any time. This creates a race where processes arriving at tick T are consumed by the scheduler during tick T-1. The sentinel message (mtype 2) solution synchronizes the two without requiring shared state or additional semaphores: the scheduler blocks in `msgrcv` until it observes the sentinel for the current tick, guaranteeing it has drained all arrivals for that tick before running the scheduling algorithm.

### Semaphore as Execution Gate

Using a semaphore to gate each tick of process execution eliminates the need for the scheduler to send a signal to the child each tick. The child blocks cheaply in kernel space on `semop`, and the scheduler grants exactly one execution unit per tick by performing a single `up`. This creates a strict one-tick-at-a-time execution model that is verifiable and deterministic.

### Welford's Online Algorithm for Standard Deviation

Performance statistics are accumulated incrementally using Welford's algorithm, which computes a numerically stable running variance in a single pass without storing all WTA values. This keeps memory usage O(1) regardless of the number of processes.

### NRU with Periodic R-bit Reset

The NRU policy approximates LRU behavior at low overhead. The periodic reset of R bits every k quantums introduces a temporal dimension: pages that were recently used but have not been accessed since the last reset are downgraded to lower NRU classes, making them available for eviction before pages that were accessed more recently. This prevents heavily-used pages from monopolizing physical memory indefinitely.

### Page Table Resident Frames

Evicting a page table would require the MMU to handle a fault on the fault handler's own data structures, introducing unbounded recursion. Reserving the page table frame permanently eliminates this case at the cost of two frames per process, which is justified by the simplification in fault handling logic.

### Work Stealing with Stall Penalty

The work-steal signal (`SIGUSR2`) is handled asynchronously in the sub-scheduler. To prevent the newly stolen process from executing immediately on the receiving CPU with stale cache-affinity assumptions, a three-tick stall is imposed before the sub-scheduler dispatches the stolen process. This models the cache warm-up latency associated with process migration in real SMP systems.

---

## Project Structure

```
CPU-scheduler/
├── clk.c                          Clock process
├── headers.h                      Shared clock API (initClk, getClk, destroyClk)
├── Makefile
├── keyFile                        ftok anchor file for IPC key derivation
│
├── process_generator/
│   ├── process_generator.c        Orchestrator: reads input, forks scheduler and clock, dispatches arrivals
│   ├── process_generator.h
│   └── process_generator_functions.c
│
├── scheduler/
│   ├── scheduler.c                HPF and dual-CPU FCFS scheduler main loop
│   ├── scheduler.h
│   └── scheduler_functions.c      HPF_algo, FCFS_algo, logging, perf tracking
│
├── rr_scheduler/
│   ├── rr_scheduler.c             RR scheduler main loop with MMU integration
│   ├── rr_scheduler.h
│   └── rr_scheduler_functions.c   RR_algo, block queue management, request handling
│
├── sub-sched/
│   ├── sub_scheduler.c            Per-CPU FCFS sub-scheduler with work stealing
│   ├── sub_scheduler.h
│   └── sub_scheduler_functions.c
│
├── process/
│   ├── RR_process.c               Process child: tick simulation, request sending, shm RT update
│   ├── process.h
│   ├── process_functions.c
│   └── process.c
│
├── MMU/
│   ├── mmu.h                      MMU API declarations
│   └── mmu_functions.c            Page table management, fault handler, NRU replacement
│
├── data_structures/
│   ├── PCB/
│   │   ├── Sch_PCB.h              PCB queue interface
│   │   └── Sch_PCB.c              Priority and FIFO queue operations
│   └── REQ/
│       ├── requests.h             Memory request queue interface
│       └── requests.c             Request queue operations
│
├── data structs/
│   └── structs.h                  Core type definitions (PCB, Frame, PTE, processData, request, ...)
│
├── input/
│   ├── processes.txt              Default process input file
│   ├── requests/                  Per-process memory access request files
│   └── tc*.txt                    Additional test case input files
│
├── logs/
│   ├── scheduler.log              Scheduling event log
│   ├── scheduler.perf             Performance summary
│   └── memory.log                 MMU event log
│                    
└── outFiles/                Compiled binaries (generated by make)
```

---

## Development Team

| Name | Role |
|---|---|
| Emad Adham | Scheduling algorithms, IPC design, RR scheduler implementation |
| George Bahig | Dual-CPU FCFS, sub-scheduler, work stealing, load balancing |
| Mariam Mohammed | Memory management unit, NRU page replacement, page fault handling |
| Mariam Sameh | Process lifecycle, logging infrastructure, performance reporting |

---

## License

This project is released under the MIT License. See `LICENSE` for details.