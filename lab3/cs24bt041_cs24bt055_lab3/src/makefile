build-part1: scheduler.out

scheduler.out: scheduler.cpp
	g++ -g scheduler.cpp -o scheduler.out

run-part1: scheduler.out
	./scheduler.out FIFO assignment3_test_cases-20260908T151158Z-1-001/assignment3_test_cases/process1.txt 1

clean-part1:
	rm -f scheduler.out

build-part2: scheduler.out

run-part2: scheduler.out
	./scheduler.out FIFO assignment3_test_cases-20260908T151158Z-1-001/assignment3_test_cases/process1.txt 2

clean-part2:
	rm -f scheduler.out

clean:
	rm -f *.out
