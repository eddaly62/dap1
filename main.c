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
#include <semaphore.h>
#include <fcntl.h>
#include "dap.h"

#define MAX_BUF 1024        // test buffer size
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

// Test Parsing and Queue APIs
void ParseQueueTest (void) {

    int n;
	long long elapsedt=0;
    char s[MAX_BUF];
    struct DAP_REGEX_RESULTS rt;
    struct timeval start, end;
    struct DAP_PATTERN_QUEUE q;

    dap_pattern_queue_init(&q);

    for (;;){
        fprintf(stdout,"Enter data packet to process: ");
        if (fscanf(stdin, "%s", s) > 0) {

            if (strcmp(EXIT_STRING, s) == 0) {
                // quit
                exit(0);
            }
            else {

                // start timer
                elapsed_time(START, &start, &end);

                dap_pattern_find(s, &relut[0], ARRAY_SIZE(relut), &rt);

                elapsedt = elapsed_time(END, &start, &end);
                fprintf(stdout, "Search time is %lld usecs\n", elapsedt);

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

// Test DATA (UART) APIs

void UartTest (void) {

    #define MESSAGE "A123456789\t123456789\t123456789"
    #define MESSAGE_SIZE    30
    #define MAX_LOOPS       10
    #define S1NAME          "u1sem"

    unsigned char rx[MAX_BUF];
    unsigned char tx[MAX_BUF];
    int send_len;
    int rcv_len;
    sem_t *s1p;
    int n = 0;


    sem_unlink(S1NAME);
    s1p = sem_open(S1NAME, (O_CREAT|O_EXCL), O_RDWR, 0);

    dap_port_set_sem(DAP_DATA_SRC1, s1p);

    memcpy(&tx, MESSAGE, strlen(MESSAGE));

    while (n < MAX_LOOPS) {

        // transmit data in  buf_tx buffer
        send_len = dap_port_transmit (DAP_DATA_SRC1, tx, 30);
        printf("before sem_wait in main\n"); //TODO
        // wait until data is received
        sem_wait(s1p);
        printf("after sem_wait in main\n"); //TODO

        // receive data, returns number of bytes or error code (negative value), data copied to buff
        rcv_len = dap_port_receive (DAP_DATA_SRC1, rx);
        rx[rcv_len] = 0;

        fprintf(stdout, "TRANSMIT(%d): %s\n", send_len, tx);
        fprintf(stdout, "RECEIVE(%d) : %s\n", rcv_len,  rx);

        //sleep(10); //TODO - join?
        n++;
    }

    sem_close(s1p);
}

void fini (void) {

    // Shut down DAP
    dap_shutdown();
}


int main (int argc, char *argv[]) {

    atexit (fini);

    // Initialize DAP
    dap_init ();

#if 0
    // Test Parsing, Queue, and Elapsed Time APIs
    ParseQueueTest ();
#endif

    // UART Test
    UartTest ();


}
