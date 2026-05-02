#ifndef STRUCTS_H
#define STRUCTS_H

#include <sys/types.h>

typedef struct processData
{
  long mtype;
  int id;
  int arrival;
  int runtime;
  int priority;
  int base;
  int limit;
} processData;

typedef enum logState
{
  START,
  RESUME,
  STOP,
  FINISH,
  STOLEN,
} logState;

typedef struct PerfVars
{
  float avg_WTA;
  float avg_Waiting;
  float std_WTA;
  float M2_WTA;           // Needed to perform rolling Standard deviation
  float welford_mean_WTA; // Needed to perform rolling Standard deviation
  int total_runtime;
  int first_arrival;
  int finish_time;
  int num_procs;
} perfVars;

typedef struct PCB
{
  int id;
  int arrival;
  int runtime;
  int priority;

  int remaining_time;
  int waiting_time;
  int start_time;
  int finish_time;
  int last_stopped;

  pid_t pid;
  char state;
  enum logState lState; // This is used for logging purposes

  struct PCB *next;

  int frame_index; // This is used to keep track of the frame index in the page table
  int base;
  int limit;
  int unblock_at; // This is used to keep track of the time when the process will be unblocked in case it gets blocked due to a page fault

} PCB;



typedef struct PCBNode
{
  PCB *pcb;
  struct PCBNode *next;
} PCBNode;

typedef struct PCBQueue
{
  PCBNode *front;
  PCBNode *rear;
  int size;
} PCBQueue;

typedef struct PGNode
{
  processData data;
  struct PGNode *next;
} PGNode;

typedef struct PGQueue
{
  PGNode *front;
  PGNode *rear;
  int size;
} PGQueue;

union Semun
{
  int val;
  struct semid_ds *buf;
  unsigned short *array;
  struct seminfo *__buf;
};

typedef struct PTE
{
  short R;
  short M;
  int frame_address;
  int valid;
} PTE;

typedef struct Frame
{
  short R;
  short M;
  int process_id;
  PTE *pte;
  short occupied;
  int data;
  int vpage;
} Frame;

typedef struct request
{
  int tick;
  int address;
  char operation; // 'R' for read, 'W' for write
} request;

typedef struct VirtualAddress
{
    int page;
    int offset;
} VirtualAddress;

#endif