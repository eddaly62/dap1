// testparse.c
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

#define MAX_BUF 100         // test buffer size
#define EXIT_STRING "q"     // string to type to exit program

// app supplied callback prototypes
void callback(char *s);

// app supplied pattern/callback look up table
// 1st element is the regular expression to search for
// 2nd element is the callback function to call
const struct DAP_PATTERN_CB relut[] = {
    {"033A", &callback},
    {"033B", &callback},
    {"033C", &callback},
    {"033D", &callback},
    {"033E", &callback},
    {"033F", &callback},
    {"033G", &callback},
    {"033H", &callback},
    {"033I", &callback},
    {"033J", &callback},
    {"033K", &callback},
    {"033L", &callback},
    {"033M", &callback},
    {"033N", &callback},
    {"033O", &callback},
    {"033P", &callback},
    {"033Q", &callback},
    {"033R", &callback},
    {"033S", &callback},
    {"033T", &callback},
    {"033U", &callback},
    {"033V", &callback},
    {"033W", &callback},
    {"033X", &callback},
    {"033Y", &callback},
    {"033Z", &callback},
    {"\e[1m", &callback},
    {"\e[0m", &callback},
};

// sample callback function supplied by the app
void callback(char *s){
    fprintf(stdout, "callback function called, pattern = %s\n", s);
    return;
}

// Test Parsing and Queue APIs
int ParseQueueTest (void) {

    int n;
    int nq;
    int result;
	long long elapsedt=0;
    char s[MAX_BUF];
    struct DAP_REGEX_RESULTS rt;
    struct timeval start, end;
    struct DAP_PATTERN_QUEUE q;

    struct DAP_PATTERN_DATA pd;

    dap_pattern_queue_init(&q);

    for (;;){
        fprintf(stdout,"Enter data packet to process: ");
        if (fscanf(stdin, "%s", s) > 0) {

            if (strcmp(EXIT_STRING, s) == 0) {
                // quit
                return DAP_SUCCESS;
            }
            else {

                // start timer
                elapsed_time(START, &start, &end);

                dap_pattern_set(&relut[0], ARRAY_SIZE(relut), &pd);

                result = dap_pattern_find(s, sizeof(s), &pd);

                elapsedt = elapsed_time(END, &start, &end);
                fprintf(stdout, "Search time is %lld usecs\n", elapsedt);

                ASSERT((result != DAP_PATTERN_FIND_ERROR), "return error", DAP_ERROR)

                if (result == DAP_RE_MATCH) {

                    dap_pattern_get(&pd, &rt);

                    fprintf(stdout,"index in lut = %d, string = %s\n", rt.indexlut, rt.out);

                    // run callback
                    if (rt.cb != NULL) {
                        (*(rt.cb))(rt.out);
                    }
                    // show contents of queue
                    if (dap_pattern_queue_is_empty(&q) == true) {
                        printf("Queue is EMPTY \n");
                    }
                    else {
                        printf("Queue is NOT EMPTY \n");
                    }

                    // store result in queue
                    printf("Add item to queue\n");
                    dap_pattern_queue_insert(&q, &rt);

                    // show contents of queue
                    if (dap_pattern_queue_is_empty(&q) == true) {
                        printf("Queue is EMPTY \n");
                    }
                    else {
                        printf("Queue is NOT EMPTY \n");
                    }

                    if (dap_pattern_queue_is_full(&q) == true) {
                        printf("Queue is FULL\n");
                    }
                    else {
                        printf("Queue is NOT FULL\n");
                    }

                    nq = dap_pattern_queue_size(&q);
                    printf("Number of items in queue = %d\n", nq);

                    // show queue
                    printf("Queue: \n");
                    for (n = q.front; n <= q.rear; n++) {
                        printf("%s ", q.rq[n].out);
                    }
                    printf("\n");

                }
                else if(result == DAP_RE_NO_MATCH) {
                    printf("No match found\n");
                }
                else {
                    printf("Error during search\n");

                }
            }
        }
    }
}

void fini (void) {

    // Shut down DAP
    dap_shutdown();
}

int main (int argc, char *argv[]) {

    atexit (fini);

    // Initialize DAP
    dap_init ();

    // Test Parsing, Queue, and Elapsed Time APIs
    ParseQueueTest();
}
