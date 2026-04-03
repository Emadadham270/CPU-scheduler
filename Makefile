build:
	mkdir -p output
	gcc process_generator/*.c -o ./outFiles/process_generator.out -I.
	gcc clk.c -o clk.out -I.
	gcc scheduler/*.c -o ./outFiles/scheduler.out -I.
	gcc process/*.c -o ./outFiles/process.out -I.
	gcc test_generator/*.c -o ./outFiles/test_generator.out -I.

clean:
	rm -f outFiles/*.out
	rm -rf output

all: clean build

run:
	./outFiles/process_generator.out
