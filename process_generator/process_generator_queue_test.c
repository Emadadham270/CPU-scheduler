#include <stdio.h>
#include <stdlib.h>
#include "process_generator.h"

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
    PGQueue *q = pg_createQueue();

    processData p1 = {.mtype = 1, .id = 10, .arrival = 0, .runtime = 5, .priority = 2};
    processData p2 = {.mtype = 1, .id = 11, .arrival = 1, .runtime = 3, .priority = 1};
    processData p3 = {.mtype = 1, .id = 12, .arrival = 2, .runtime = 7, .priority = 3};

    pg_enqueue(q, p1);
    pg_enqueue(q, p2);
    pg_enqueue(q, p3);

    assert_or_exit(!pg_isEmpty(q), "queue should not be empty after enqueue");
    assert_or_exit(pg_peek(q).id == 10, "peek id should be 10 (FIFO)");

    processData d1 = pg_dequeue(q);
    processData d2 = pg_dequeue(q);
    processData d3 = pg_dequeue(q);

    assert_or_exit(d1.id == 10, "first dequeued id should be 10");
    assert_or_exit(d2.id == 11, "second dequeued id should be 11");
    assert_or_exit(d3.id == 12, "third dequeued id should be 12");
    assert_or_exit(pg_isEmpty(q), "queue should be empty after 3 dequeues");

    pg_freeQueue(q);

    printf("Process generator queue test: PASS\n");
    return 0;
}
