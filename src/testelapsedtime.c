// testelapsedtime.c
// This file is used to test the dap APIs


#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <limits.h>
#include <sys/time.h>
#include <regex.h>
#include <stdint.h>
#include <stdbool.h>
#include <semaphore.h>
#include <fcntl.h>
#include "dap.h"

// test of elapsed time function
void elapsedTimeTest (void) {

    long long i;
	long long elapsedt=0;
    struct timeval start, end;

    for (i = 0; i < 1000000; i+=1000){
        // start timer
        elapsed_time(START, &start, &end);
        // delay
        usleep(1000+i);
        // stop timer
        elapsedt = elapsed_time(END, &start, &end);
        fprintf(stdout, "Delays of %lld usec, measured %lld usecs\n", (1000+i), elapsedt);
    }
}

void fini (void) {

    // Shut down DAP
    dap_shutdown();
}

int main (int argc, char *argv[]) {

    atexit(fini);

    // Initialize DAP
    dap_init();

    // elapesd time test
    elapsedTimeTest();

    return 0;

}
