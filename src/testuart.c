// testuart.c
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

void UartTest (void) {

    #define MESSAGE "A123456789\e123456789\e123456789"
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

        // wait until data is received
        sem_wait(s1p);

        // receive data, returns number of bytes or error code (negative value), data copied to buff
        rcv_len = dap_port_receive (DAP_DATA_SRC1, rx);
        rx[rcv_len] = 0;

        fprintf(stdout, "TRANSMIT(%d): %s\n", send_len, tx);
        fprintf(stdout, "RECEIVE(%d) : %s\n", rcv_len,  rx);

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

    // UART Test
    UartTest ();

}
