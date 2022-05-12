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

struct DAP_UART uart1;
struct DAP_UART uart2;

// Initialization function
int dap_init(void) {

	int result;

	// initializes UART port 1
	result = dap_port_init (&uart1, DAP_UART_1, DAP_UART_1_BAUD);
	if (result != DAP_SUCCESS) {
		ASSERT(ASSERT_FAIL, "UART 1 Initialization: Can not initialize", strerror(errno))
	}

	// initializes UART port 2
	result = dap_port_init (&uart1, DAP_UART_2, DAP_UART_2_BAUD);
	if (result != DAP_SUCCESS) {
		ASSERT(ASSERT_FAIL, "UART 2 Initialization: Can not initialize", strerror(errno))
	}

	return result;
}


// Shut-down function
int dap_shutdown (void) {

	int result = DAP_SUCCESS;

	// close uart 1
	dap_port_close (&uart1);

	// close uart 2
	dap_port_close (&uart2);

	return result;

}
