main:
	gcc -o preflow preflow.c pthread_barrier.c -g -O3 -pthread -DMAIN
	time sh check-solution.sh ./preflow
	@echo PASS all tests
