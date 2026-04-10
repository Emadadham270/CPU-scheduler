#include <stdio.h>
#include <stdlib.h>
#include "Sch_PCB.h"

static void assert_or_exit(int condition, const char *message)
{
    if (!condition)
    {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(1);
    }
}

int main(void)
{
    Queue *q = createQueue();

    PCB p1 = {.id = 1, .priority = 3, .remaining_time = 5, .waiting_time = 0};
    PCB p2 = {.id = 2, .priority = 1, .remaining_time = 2, .waiting_time = 1};
    PCB p3 = {.id = 3, .priority = 2, .remaining_time = 7, .waiting_time = 0};

    enqueue_priority(q, &p1);
    enqueue_priority(q, &p2);
    enqueue_priority(q, &p3);

    assert_or_exit(q->size == 3, "queue size after enqueue_priority should be 3");
    assert_or_exit(peek(q)->id == 2, "front should be smallest priority id=2");

    update_waiting_times(q);
    assert_or_exit(p1.waiting_time == 1, "p1 waiting time should increment");
    assert_or_exit(p2.waiting_time == 2, "p2 waiting time should increment");
    assert_or_exit(p3.waiting_time == 1, "p3 waiting time should increment");

    assert_or_exit(total_remaining_time(q) == 14, "total remaining time should be 14");

    PCB *d1 = dequeue(q);
    PCB *d2 = dequeue(q);
    PCB *d3 = dequeue(q);

    assert_or_exit(d1->id == 2, "first dequeued id should be 2");
    assert_or_exit(d2->id == 3, "second dequeued id should be 3");
    assert_or_exit(d3->id == 1, "third dequeued id should be 1");
    assert_or_exit(isEmpty(q), "queue should be empty after 3 dequeues");

    freeQueue(q);

    printf("PCB queue test: PASS\n");
    return 0;
}
