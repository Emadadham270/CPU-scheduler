#ifndef STRUCTS_H
#define STRUCTS_H

#include <sys/types.h>

typedef struct processData {
  long mtype;
  int id;
  int arrival;
  int runtime;
  int priority;
} processData;

typedef struct PCB {
  int id;
  int arrival;
  int runtime;
  int priority;

  int remaining_time;
  int waiting_time;
  int start_time;
  int finish_time;

  pid_t pid;
  char state;

  struct PCB *next;
} PCB;

typedef struct PCBNode {
  PCB *pcb;
  struct PCBNode *next;
} PCBNode;

typedef struct PCBQueue {
  PCBNode *front;
  PCBNode *rear;
  int size;
} PCBQueue;

typedef struct PGNode {
  processData data;
  struct PGNode *next;
} PGNode;

typedef struct PGQueue {
  PGNode *front;
  PGNode *rear;
  int size;
} PGQueue;

#endif
