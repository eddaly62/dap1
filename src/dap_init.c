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
#include "dap_config.h"
#include "dap.h"

extern void dap_uart_shutdown (void);
extern int dap_uart_init (void);

// Initialization function
int dap_init(void) {

	int result = DAP_SUCCESS;

	// Set signal masks
	// done at start up so all threads created by DAP inherit the same signal mask
	// Mask all signals except kill signal
	// TODO  - sig mask init? TBD

	result = dap_uart_init();
	ASSERT((result != DAP_ERROR), "UARTs Not intialized", DAP_ERROR)

	return result;
}


// Shut-down function
void dap_shutdown (void) {

	// shut down uarts
	dap_uart_shutdown ();

}
