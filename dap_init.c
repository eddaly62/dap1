// dap_init.c
// Initialization and Shut-down functions

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
#include <termios.h>
#include "dap.h"

// UART data structures
extern struct DAP_UART uart1;
extern struct DAP_UART uart2;
extern struct DAP_UART_EPOLL uep;
extern pthread_t tid_uart;
extern void *dap_uart_epoll_thr(void *arg);


// Initialization function
int dap_init(void) {

	int result;

	// Set signal masks
	// done at start up so all threads created by DAP inherit the same signal mask
	// Mask all signals except kill signal
	// TODO  - sig mask init? TBD

	// initializes UART port 1
	result = dap_port_init (&uart1, DAP_UART_1, DAP_UART_1_BAUD);
	if (result != DAP_SUCCESS) {
		ASSERT(ASSERT_FAIL, "UART 1 Initialization: Can not initialize", "-1")
	}

	// initializes UART port 2
	result = dap_port_init (&uart1, DAP_UART_2, DAP_UART_2_BAUD);
	if (result != DAP_SUCCESS) {
		ASSERT(ASSERT_FAIL, "UART 2 Initialization: Can not initialize", "-1")
	}

	// create thread for reading UART inputs
	result = pthread_create(&tid_uart, NULL, dap_uart_epoll_thr, (void *)i);
    if (result != 0) {
		ASSERT(ASSERT_FAIL, "UART epoll thread creation failed", "-1")
    }

	result = dap_uart_epoll_init (&uep, &uart1, &uart2);
	if (result != DAP_SUCCESS) {
		ASSERT(ASSERT_FAIL, "UART epoll initialization failed", "-1")
	}

	return result;
}


// Shut-down function
void dap_shutdown (void) {

	// close uart 1
	dap_port_close (&uart1);

	// close uart 2
	dap_port_close (&uart2);

	// TODO add close for thread and epoll
}
