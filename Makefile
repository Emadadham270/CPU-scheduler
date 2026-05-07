CC := gcc
CFLAGS := -Wall -Wextra -I. -Idata_structures/PCB -Idata_structures/REQ -fcommon
LDFLAGS := -Wl,--allow-multiple-definition
OUT_DIR := outFiles
OBJ_DIR := output

PROCESS_GEN_SRCS := process_generator/process_generator.c process_generator/process_generator_functions.c
SCHEDULER_SRCS := scheduler/scheduler.c scheduler/scheduler_functions.c data_structures/PCB/Sch_PCB.c 
RR_SCHEDULER_SRCS := rr_scheduler/rr_scheduler.c rr_scheduler/rr_scheduler_functions.c MMU/mmu_functions.c data_structures/PCB/Sch_PCB.c data_structures/REQ/requests.c
PROCESS_SRCS := process/RR_process.c process/process_functions.c
TEST_GEN_SRCS := test_generator/test_generator.c test_generator/test_generator_functions.c
PCB_SRCS := data_structures/PCB/Sch_PCB.c
REQ_SRCS := data_structures/REQ/requests.c

SUB_SCHED_SRCS := sub-sched/sub_scheduler.c sub-sched/sub_scheduler_functions.c data_structures/PCB/Sch_PCB.c

PROCESS_GEN_BIN := $(OUT_DIR)/process_generator.out
SCHEDULER_BIN := $(OUT_DIR)/scheduler.out
RR_SCHEDULER_BIN := $(OUT_DIR)/rr_scheduler.out
PROCESS_BIN := $(OUT_DIR)/process.out
TEST_GEN_BIN := $(OUT_DIR)/test_generator.out
CLK_BIN := $(OUT_DIR)/clk.out
PCB_OBJ := $(OBJ_DIR)/data_structures/PCB/Sch_PCB.o
REQ_OBJ := $(OBJ_DIR)/data_structures/REQ/requests.o
SUB_SCHED_BIN := $(OUT_DIR)/sub_scheduler.out

.PHONY: all build clean run run-all run-all-auto dirs process_generator scheduler rr_scheduler sub_scheduler process test_generator clk pcb folders

all: build

build: dirs process_generator scheduler rr_scheduler sub_scheduler process test_generator clk

dirs:
	mkdir -p $(OUT_DIR)
	mkdir -p $(OBJ_DIR)/data_structures/PCB
	mkdir -p $(OBJ_DIR)/data_structures/REQ


process_generator: $(PROCESS_GEN_BIN)

scheduler: $(SCHEDULER_BIN)

rr_scheduler: $(RR_SCHEDULER_BIN)

process: $(PROCESS_BIN)

test_generator: $(TEST_GEN_BIN)

clk: $(CLK_BIN)

pcb: $(PCB_OBJ)

req: $(REQ_OBJ)

folders: process_generator scheduler process test_generator

$(PROCESS_GEN_BIN): $(PROCESS_GEN_SRCS) | dirs
	$(CC) $(CFLAGS) $(PROCESS_GEN_SRCS) -o $@ $(LDFLAGS)

$(SCHEDULER_BIN): $(SCHEDULER_SRCS) | dirs
	$(CC) $(CFLAGS) $(SCHEDULER_SRCS) -o $@ $(LDFLAGS) -lm

$(RR_SCHEDULER_BIN): $(RR_SCHEDULER_SRCS) | dirs
	$(CC) $(CFLAGS) $(RR_SCHEDULER_SRCS) -o $@ $(LDFLAGS) -lm

$(PROCESS_BIN): $(PROCESS_SRCS) | dirs
	$(CC) $(CFLAGS) $(PROCESS_SRCS) -o $@ $(LDFLAGS)

$(TEST_GEN_BIN): $(TEST_GEN_SRCS) | dirs
	$(CC) $(CFLAGS) $(TEST_GEN_SRCS) -o $@ $(LDFLAGS)

$(CLK_BIN): clk.c | dirs
	$(CC) $(CFLAGS) clk.c -o $@ $(LDFLAGS)

$(PCB_OBJ): $(PCB_SRCS) | dirs
	$(CC) $(CFLAGS) -c $(PCB_SRCS) -o $@

$(REQ_OBJ): $(REQ_SRCS) | dirs
	$(CC) $(CFLAGS) -c $(REQ_SRCS) -o $@

sub_scheduler: $(SUB_SCHED_BIN)

$(SUB_SCHED_BIN): $(SUB_SCHED_SRCS) | dirs
	$(CC) $(CFLAGS) $(SUB_SCHED_SRCS) -o $@ $(LDFLAGS) -lm

clean:
	rm -f $(OUT_DIR)/*.out
	rm -rf $(OBJ_DIR)

run: process_generator
	./$(PROCESS_GEN_BIN)

run-all: process_generator scheduler rr_scheduler sub_scheduler process clk
	chmod +x $(PROCESS_GEN_BIN) $(SCHEDULER_BIN) $(RR_SCHEDULER_BIN) $(SUB_SCHED_BIN) $(PROCESS_BIN) $(CLK_BIN)
	cd process_generator; ../$(PROCESS_GEN_BIN)

run-all-auto: process_generator scheduler rr_scheduler process clk
	chmod +x $(PROCESS_GEN_BIN) $(SCHEDULER_BIN) $(RR_SCHEDULER_BIN) $(PROCESS_BIN) $(CLK_BIN)
	setsid sh -c 'cd process_generator; printf "3\n3\n3\n" | ../$(PROCESS_GEN_BIN)' || true
