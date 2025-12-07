#include "types.h"
#include "stat.h"
#include "user.h"

#define NUM_PROCS 10
#define WORK_ITERATIONS 1000000000

void workload() {
    int i;
    volatile long x = 1;
    for (i = 0; i < WORK_ITERATIONS; i++) {
        x = (x * 3 + 1) / 2; 
        if (x % 500000 == 0) {
            
        }
    }
    exit();
}

int main(void) {
    int i;
    int pids[NUM_PROCS];

    printf(1, "\n=== HMP Scheduler Test Started ===\n");
    printf(1, "Creating %d heavy processes...\n", NUM_PROCS);

    start_measure();

    for (i = 0; i < NUM_PROCS; i++) {
        pids[i] = fork();
        if (pids[i] == 0) {
            workload();
        } else if (pids[i] < 0) {
            printf(2, "Error: fork failed.\n");
            break;
        }
    }

    for (i = 0; i < NUM_PROCS; i++) {
        if (pids[i] > 0) {
            wait();
        }
    }

    printf(1, "\nAll children finished. Final Process Info:\n");
    print_info(); 

    end_measure();
    printf(1, "\n=== HMP Scheduler Test Finished ===\n");

    exit();
}