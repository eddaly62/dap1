// main.c
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
#include "dap.h"

#define EXIT_STRING "q"     // string to type to exit program

// app supplied callback prototypes
void callback(char *s);

// app supplied pattern/callback look up table 
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
};

// sample callback function supplied by the app
void callback(char *s){
    fprintf(stdout, "callback function called, pattern = %s\n", s);
    return;
}

int main(int argc, char *argv[]) {

    int n;
	long long elapsedt=0;
    char s[MAX_PATTERN_BUF_SIZE];
    struct DAP_REGEX_RESULTS rt;
    struct timeval start, end;
    struct DAP_PATTERN_QUEUE q;

    dap_pattern_queue_init(&q);

    for (;;){
        fprintf(stdout,"Enter data packet to process: ");
        if (fscanf(stdin, "%s", s) > 0) {

            if (strcmp(EXIT_STRING, s) == 0) {
                ASSERT(0,"NULL POINTER", -1000123)
                // quit
                exit(0);
            }
            else {

                // start timer
                elapsed_time(START, &start, &end);

                dap_pattern_find(s, &relut[0], ARRAY_SIZE(relut), &rt);

                elapsedt = elapsed_time(END, &start, &end);
                fprintf(stdout, "Search time is %lld usecs with %d threads\n", elapsedt, MAXNUMTHR);
    
                fprintf(stdout,"index in lut = %d,\t string = %s,\t found by thread = %ld, tid = %lu\n",
                rt.indexlut, rt.out, rt.idx, (unsigned long)rt.tid);

                // run callback
                if (rt.cb != NULL) {
                    (*(rt.cb))(rt.out);
                }

                // store result in queue
                dap_pattern_queue_insert(&q, &rt);

                // show contents of queue
                if (dap_pattern_queue_is_empty(&q)) {
                    printf("Empty Queue \n");
                }
                else
                {
                    // show queue
                    printf("Queue: \n");
                    for (n = q.front; n <= q.rear; n++) {
                        printf("%s ", q.rq[n].out);
                    }
                    printf("\n");
                }
            }

        }
    }

}
