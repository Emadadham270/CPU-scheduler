# Phase 2 Paging and NRU Memory Management Implementation Plan

## Goal

Extend the current Phase 1 OS Scheduler project with demand paging for the 1-CPU Round Robin scheduler only. The implementation will add an MMU module, page-table management, page-fault handling, NRU page replacement, disk-delay simulation, request-file parsing, and the required `memory.log` output.

This phase does not require adapting HPF or the 2-CPU FCFS extension.

## Project Assumptions

- Address space is 10-bit and byte-addressable.
- Physical memory size is 512 bytes.
- Page size is 16 bytes.
- RAM has 32 physical frames.
- One memory access takes 1 tick.
- One disk access takes 10 ticks.
- Page-table frames are never swapped out.
- NRU replacement applies only to resident process data/code pages.
- Each process has one resident page-table frame for its lifetime.
- The first process page is loaded when the process starts.
- Initial page-table allocation and initial first-page loading take no extra time.
- A page fault blocks the process while disk activity happens.
- A context switch still costs 1 tick when switching after a fault.
- The simulator prompts for:
  - RR quantum.
  - `K`, the number of elapsed RR quantums after which all resident data-page R bits are cleared.

## Current Code Touchpoints

| Area | Existing file | Required work |
|------|---------------|---------------|
| Process metadata | `data structs/structs.h` | Add disk `base`, virtual-page `limit`, and page-table frame fields to `processData`/`PCB`. |
| Generator parsing | `process_generator/process_generator.c` | Parse `id arrival runtime priority base limit`; ask for `K` when RR is selected; pass `K` to scheduler. |
| Scheduler loop | `scheduler/scheduler.c` | Integrate page requests, page faults, blocking, disk completion, RR quantum counting, and R-bit clearing. |
| Scheduler helpers | `scheduler/scheduler_functions.c`, `scheduler/scheduler.h` | Update PCB creation, process dispatch, termination cleanup, and function prototypes. |
| New MMU module | `scheduler/MMU.c`, `scheduler/MMU.h` | Implement frames, page tables, NRU, address translation, logging helpers, and memory cleanup. |
| Build system | `Makefile` | Add `scheduler/MMU.c` to `SCHEDULER_SRCS`. |
| Inputs | `input/processes.txt`, `input/requests_<pid>.txt` or agreed naming | Add base/limit fields and per-process request files. |
| Outputs | `memory.log` | Produce exact required memory event format. |

## Team Responsibilities

### Member 1: MMU Core and Data Structures

Owner of `scheduler/MMU.h`, `scheduler/MMU.c`, and shared memory-related constants.

Tasks:

1. Define constants:
   - `MEMORY_SIZE = 512`
   - `PAGE_SIZE = 16`
   - `FRAME_COUNT = 32`
   - `ADDRESS_BITS = 10`
2. Define frame metadata:
   - frame number
   - free/occupied state
   - frame type: free, page table, data page
   - owner process id
   - virtual page number
   - referenced bit
   - modified bit
3. Define page-table entry metadata:
   - present/valid bit
   - frame number
   - referenced bit
   - modified bit
   - disk page number or enough data to compute it from process base
4. Implement MMU initialization:
   - mark all 32 frames free
   - clear all frame metadata
   - open/create `memory.log`
5. Implement free-frame allocation:
   - scan frames in ascending order
   - return first free frame
   - log `Free Physical page "ZZ" allocated` only when the allocation is part of a page-fault demand load, not for silent initial allocation unless required by tests
6. Implement page-table allocation:
   - allocate one frame for every new process
   - if no free frame exists, evict a data page using NRU
   - mark page-table frame as non-replaceable
7. Implement process memory cleanup:
   - free page-table frame
   - free all resident data frames owned by the process
   - invalidate/clear related metadata

Deliverable from Member 1:

- MMU structures compile cleanly and can allocate/free frames in isolation.

### Member 2: Process Input, PCB, and Request Files

Owner of `data structs/structs.h`, `process_generator/process_generator.c`, request-file parsing helpers, and PCB creation updates.

Tasks:

1. Extend `processData`:
   - `base`
   - `limit`
2. Extend `PCB`:
   - disk base page
   - virtual page limit
   - page-table frame number
   - consumed CPU time
   - blocked/fault state fields if needed
   - pending page-fault information if needed
3. Update `process_generator.c` parsing:
   - ignore comments and blank lines
   - parse six fields from `processes.txt`
   - keep compatibility only if the team intentionally wants a fallback, but grading should use the six-field format
4. Update `createPCB(processData p)`:
   - copy base/limit into the PCB
   - initialize page-table frame to invalid
   - initialize consumed CPU time to zero
5. Decide request-file naming and document it in the report:
   - recommended: `input/requests_<process_id>.txt`
   - example: `input/requests_1.txt`
6. Implement a request parser:
   - fields: `time addressInBinary r/w`
   - time is relative to consumed CPU time
   - binary address is converted to an integer virtual address
   - access type is read or write
7. Store each process request list in the PCB or in an MMU-owned per-process table.
8. Validate virtual addresses:
   - virtual page = address / 16
   - offset = address % 16
   - virtual page must be `< limit`
   - invalid requests should not crash the program; decide whether to ignore with console debug output or handle as an error.

Deliverable from Member 2:

- Process generator sends base/limit correctly, scheduler PCBs contain memory metadata, and request files can be loaded and queried by consumed CPU time.

### Member 3: Scheduler and Page-Fault Integration

Owner of `scheduler/scheduler.c`, `scheduler/scheduler_functions.c`, and RR behavior.

Tasks:

1. Restrict Phase 2 execution path to RR:
   - keep HPF/2-CPU code compiling
   - only integrate paging into `type == 1`
2. Update startup arguments:
   - scheduler receives RR quantum and `K`
   - store `K` globally or pass it into the RR function
3. On process arrival/start:
   - create PCB
   - allocate page-table frame through MMU
   - initialize all page-table entries invalid
   - load virtual page 0 into memory with no extra time
   - update page-table entry for page 0
4. Track consumed CPU time:
   - increment when the process actually runs for a tick
   - do not increment during context-switch overhead or blocked disk time
5. Before or during each CPU tick, check whether a memory request is due at the current consumed CPU time.
6. For every due request:
   - translate virtual address through MMU
   - if resident: set R bit, and set M bit for writes
   - if not resident: trigger page fault
7. Page-fault behavior:
   - log `PageFault upon VA <binary> from process <id>`
   - remove process from CPU
   - mark process blocked
   - calculate unblock time:
     - 10 ticks if no dirty victim write-back is needed
     - 20 ticks if dirty victim write-back is needed
   - if another process is dispatched, apply 1-tick context-switch overhead
8. Maintain a blocked list:
   - each blocked process has a disk completion time
   - at completion, load demanded page, update page table, mark ready
   - enqueue back into the ready queue
9. Handle process finish:
   - free all process memory frames through MMU
   - keep existing performance accounting if still needed internally
10. Implement `K` quantum R-bit clearing:
   - count completed RR quantums globally
   - after every `K` elapsed quantums, clear R bits for all resident data pages
   - also clear matching page-table-entry R bits

Deliverable from Member 3:

- RR correctly blocks on page faults, continues running other ready processes, respects context-switch overhead, and clears R bits after every `K` quantums.

### Member 4: NRU, Logging, Testing, and Report

Owner of NRU verification, exact log formatting, test cases, and final report sections.

Tasks:

1. Implement or review NRU victim selection:
   - classify data frames only
   - class 0: `R=0, M=0`
   - class 1: `R=0, M=1`
   - class 2: `R=1, M=0`
   - class 3: `R=1, M=1`
   - choose the first frame in ascending physical-frame order from the lowest non-empty class
2. Implement victim eviction:
   - if victim is modified, log `Swapping out page <ZZ> to disk`
   - invalidate the victim process page-table entry
   - clear old owner metadata from the frame
3. Implement exact `memory.log` output:
   - `PageFault upon VA <VA> from process <PID>`
   - `Free Physical page <FRAME> allocated`
   - `Swapping out page <FRAME> to disk`
   - `At time <TIME> disk address <DISK_PAGE> for process <PID> is loaded into memory page <FRAME>.`
4. Confirm whether quotes from the handout examples are literal or explanatory:
   - recommended implementation should match the non-comment examples without quotes.
5. Build deterministic tests:
   - free-frame page fault
   - page fault with clean NRU victim
   - page fault with dirty NRU victim
   - page-table frames excluded from replacement
   - R-bit clearing after `K` RR quantums
   - blocked process returns to ready queue after 10 or 20 ticks
   - process finish frees frames for later processes
6. Update `Makefile` test targets if helpful.
7. Write final report sections:
   - block diagram
   - process states
   - memory-management design
   - page-table structure
   - NRU victim-selection logic
   - assumptions
   - known limitations

Deliverable from Member 4:

- `memory.log` matches the expected format and the final report explains the design clearly.

## Proposed Module Design

### `MMU.h`

Recommended public API:

```c
void mmu_init(FILE *memory_log);
void mmu_shutdown(void);

int mmu_create_process_memory(PCB *pcb);
void mmu_free_process_memory(PCB *pcb);

int mmu_access(PCB *pcb, const char *va_binary, int virtual_address, char mode,
               int now, int *fault_disk_time, int *loaded_frame);

int mmu_complete_page_fault(PCB *pcb, int virtual_address, char mode, int now);
void mmu_clear_all_reference_bits(void);
```

The exact signatures can change during implementation, but keep the responsibility split:

- scheduler decides when a request occurs and when a blocked process wakes up
- MMU decides whether the page is resident, which frame to use, and which victim to evict
- MMU writes memory-related log lines

### Frame Table

```c
typedef enum FrameType {
    FRAME_FREE,
    FRAME_PAGE_TABLE,
    FRAME_DATA
} FrameType;

typedef struct Frame {
    int frame_number;
    FrameType type;
    int owner_pid;
    int virtual_page;
    int referenced;
    int modified;
} Frame;
```

### Page-Table Entry

```c
typedef struct PageTableEntry {
    int present;
    int frame_number;
    int referenced;
    int modified;
} PageTableEntry;
```

Each PCB needs either:

- an in-memory `PageTableEntry *page_table` pointer allocated by the simulator, plus a physical page-table frame number, or
- an index into an MMU-managed process table.

The project simulates the page-table frame, so the entries do not need to literally occupy the 16 bytes in C memory unless the team chooses to model that strictly.

## Page-Fault Flow

1. Running process reaches a request time based on consumed CPU time.
2. Scheduler asks MMU to access the virtual address.
3. MMU computes:
   - virtual page = `virtual_address / PAGE_SIZE`
   - offset = `virtual_address % PAGE_SIZE`
   - disk address = `pcb->base + virtual_page`
4. If page is present:
   - set R bit
   - set M bit for writes
   - access completes in 1 tick as part of normal CPU execution
5. If page is missing:
   - log page fault
   - find free frame or NRU victim
   - if victim dirty, schedule 20 ticks total disk delay
   - otherwise schedule 10 ticks total disk delay
   - block process
6. When disk completes:
   - update frame metadata
   - update page-table entry
   - log loaded page line using the disk completion time
   - move process back to ready queue

## Implementation Order

1. Add memory fields to `processData` and `PCB`.
2. Update generator parsing and scheduler argument passing for `K`.
3. Add `MMU.h`/`MMU.c` with frame-table initialization and free-frame allocation.
4. Update `Makefile` so scheduler links with `MMU.c`.
5. Allocate page-table frame and first page when each PCB is created/started.
6. Add request-file parsing and consumed-CPU-time tracking.
7. Add resident-page access handling.
8. Add page-fault blocking and disk-completion handling.
9. Add NRU replacement.
10. Add dirty-victim write-back timing and log line.
11. Add R-bit clearing after every `K` RR quantums.
12. Add memory cleanup on process finish.
13. Run focused tests and fix output formatting.
14. Update the final report.

## Testing Checklist

- Build succeeds with `make clean && make build`.
- RR still starts and accepts the quantum.
- RR asks for `K`.
- `processes.txt` with six fields is parsed correctly.
- Requests at consumed CPU time are triggered exactly once.
- Binary addresses are translated correctly.
- Page 0 is initially loaded without extra disk time.
- Free frame allocation happens before replacement.
- Page-table frames are never selected by NRU.
- NRU chooses the lowest class and lowest frame number in that class.
- Dirty victim adds a second disk access.
- Blocked process does not consume CPU time.
- Other ready processes can run during disk activity.
- Context-switch overhead is applied after a fault when another process runs.
- R bits clear after every `K` completed RR quantums.
- Finished processes free all owned frames.
- `memory.log` contains no scheduler log lines.
- `memory.log` matches exact required line format.

## Risks and Decisions to Confirm Early

- Request-file naming is not fully specified in the handout. Use `requests_<pid>.txt` unless the teaching staff specifies another convention.
- The examples show both quoted placeholder comments and unquoted actual log lines. Use the unquoted actual log format.
- If several memory requests are due at the same consumed CPU time, process them in file order.
- If a page fault happens before the CPU tick completes, do not increment consumed CPU time for that failed access unless the team confirms a different interpretation.
- Keep HPF and 2-CPU code compiling, but do not spend Phase 2 effort adapting them to paging.

## Suggested Timeline

| Day | Work |
|-----|------|
| Day 1 | Finalize structs, process input format, request-file naming, and MMU skeleton. |
| Day 2 | Implement frame allocation, page-table allocation, first-page loading, and build integration. |
| Day 3 | Implement request parsing, address translation, resident reads/writes, and consumed CPU time. |
| Day 4 | Implement page faults, blocked queue, disk completion, and basic `memory.log`. |
| Day 5 | Implement NRU, dirty write-back timing, and R-bit clearing by `K` quantums. |
| Day 6 | Integration testing, edge cases, memory cleanup, and format fixes. |
| Day 7 | Final report, diagrams, grading checklist pass, and repository cleanup. |

