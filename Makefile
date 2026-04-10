CC := gcc
CFLAGS := -Wall -Wextra -I. -Idata_structures/PCB
OUT_DIR := outFiles
OBJ_DIR := output

PROCESS_GEN_SRCS := process_generator/process_generator.c process_generator/process_generator_functions.c
SCHEDULER_SRCS := scheduler/scheduler.c scheduler/scheduler_functions.c data_structures/PCB/Sch_PCB.c
PROCESS_SRCS := process/process.c process/process_functions.c
TEST_GEN_SRCS := test_generator/test_generator.c test_generator/test_generator_functions.c
PCB_SRCS := data_structures/PCB/Sch_PCB.c

PROCESS_GEN_BIN := $(OUT_DIR)/process_generator.out
SCHEDULER_BIN := $(OUT_DIR)/scheduler.out
PROCESS_BIN := $(OUT_DIR)/process.out
TEST_GEN_BIN := $(OUT_DIR)/test_generator.out
CLK_BIN := $(OUT_DIR)/clk.out
PCB_OBJ := $(OBJ_DIR)/data_structures/PCB/Sch_PCB.o

.PHONY: all build clean run run-all run-all-auto dirs process_generator scheduler process test_generator clk pcb folders

all: build

build: dirs process_generator scheduler process test_generator clk

dirs:
	mkdir -p $(OUT_DIR)
	mkdir -p $(OBJ_DIR)/data_structures/PCB

process_generator: $(PROCESS_GEN_BIN)

scheduler: $(SCHEDULER_BIN)

process: $(PROCESS_BIN)

test_generator: $(TEST_GEN_BIN)

clk: $(CLK_BIN)

pcb: $(PCB_OBJ)

folders: process_generator scheduler process test_generator

$(PROCESS_GEN_BIN): $(PROCESS_GEN_SRCS) | dirs
	$(CC) $(CFLAGS) $(PROCESS_GEN_SRCS) -o $@

$(SCHEDULER_BIN): $(SCHEDULER_SRCS) | dirs
	$(CC) $(CFLAGS) $(SCHEDULER_SRCS) -o $@

$(PROCESS_BIN): $(PROCESS_SRCS) | dirs
	$(CC) $(CFLAGS) $(PROCESS_SRCS) -o $@

$(TEST_GEN_BIN): $(TEST_GEN_SRCS) | dirs
	$(CC) $(CFLAGS) $(TEST_GEN_SRCS) -o $@

$(CLK_BIN): clk.c | dirs
	$(CC) $(CFLAGS) clk.c -o $@

$(PCB_OBJ): $(PCB_SRCS) | dirs
	$(CC) $(CFLAGS) -c $(PCB_SRCS) -o $@

clean:
	rm -f $(OUT_DIR)/*.out
	rm -rf $(OBJ_DIR)

run: process_generator
	./$(PROCESS_GEN_BIN)

run-all: process_generator scheduler process clk
	cd process_generator; ../$(PROCESS_GEN_BIN)

run-all-auto: process_generator scheduler process clk
	setsid sh -c 'cd process_generator; printf "3\n3\n3\n" | ../$(PROCESS_GEN_BIN)' || true
