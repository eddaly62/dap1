#ifndef _DAP_CONFIG_H
#define _DAP_CONFIG_H	1

#ifdef __cplusplus
extern "C" {
#endif

// dap_config.h


// pattern find
// =============

#define MAX_PATTERN_BUF_SIZE    100
#define MAXNUMTHR   2


// pattern queue
// ==============

#define MAX_PATTERN_Q_SIZE 6


// data source management (uart)
// =============================

#define DAP_UART_BUF_SIZE   60
#define DAP_UART_1_BAUD     (B9600)
#define DAP_UART_1          "/dev/ttymxc1"  // Toradex UART2
#define DAP_UART_1_TPOLL    200000          // usec
#define DAP_UART_2_BAUD     (B9600)
#define DAP_UART_2          "/dev/ttymxc3"  // Toradex UART3
#define DAP_UART_2_TPOLL    300000          // usec
#define DAP_READ_SIZE       50

enum DAP_UART_ENABLE {
    DAP_DATA_SRC1_ENABLE,   // enable uart 1
    DAP_DATA_SRC1_DISABLE,  // disable uart 1
    DAP_DATA_SRC2_ENABLE,   // enable uart 2
    DAP_DATA_SRC2_DISABLE,  // disable uart 2
};

// UART open attributes
// O_RDWR Read/write access to the serial port
// O_NOCTTY No terminal will control the process
// O_NDELAY Use non-blocking I/O
#define DAP_UART_ACCESS_FLAGS (O_RDWR | O_NOCTTY | O_NDELAY)

#ifdef __cplusplus
}
#endif

#endif
