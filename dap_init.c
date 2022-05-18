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


// Initialization function
int dap_init(void) {

	int result = DAP_SUCCESS;

	// Set signal masks
	// done at start up so all threads created by DAP inherit the same signal mask
	// Mask all signals except kill signal
	// TODO  - sig mask init? TBD

	result = dap_uart_init();
	if (result != DAP_SUCCESS) {
		ASSERT(ASSERT_FAIL, "UARTs Not intialized", "-1")
	}

	return result;
}


// Shut-down function
void dap_shutdown (void) {

	// TODO add close for thread and epoll
}
