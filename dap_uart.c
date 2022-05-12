// dap_uart.c

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>
#include <errno.h>
#include "dap.h"


// set uart attributes (helper function)
static int dap_port_init_attributes (struct DAP_UART *u) {

    tcgetattr(u->fd_uart, &u->tty);     // Get the current attributes of the first serial port

    cfsetispeed(&u->tty, u->baud);      // Set read speed
    cfsetospeed(&u->tty, u->baud);      // Set write speed

    u->tty.c_cflag &= ~PARENB;          // Disables the Parity Enable bit(PARENB)
    u->tty.c_cflag &= ~CSTOPB;          // Clear CSTOPB, configuring 1 stop bit
    u->tty.c_cflag &= ~CSIZE;           // Using mask to clear data size setting
    u->tty.c_cflag |= CS8;              // Set 8 data bits
    u->tty.c_cflag &= ~CRTSCTS;         // Disable Hardware Flow Control
                                        // TODO - Add Xon/Xoff flow control

    if ((tcsetattr(u->fd_uart, TCSANOW, &u->tty)) != 0) {   // Save configuration
        ASSERT(ASSERT_FAIL, "UART Initialization: Can not set UART attributes", strerror(errno))
        return -1;
    }
    else {
        return DAP_SUCCESS;
    }
}

// initializes UART port
int dap_port_init (struct DAP_UART *u, char *upath, speed_t baud) {

    int results = DAP_SUCCESS;

    ASSERT((upath[0] != 0), "UART path not intilaized", "0")
    ASSERT((baud != 0), "UART baud rate not intilaized", "0")
    if ((upath[0] == 0) | (baud == 0)){
        return DAP_DATA_INIT_ERROR;
    }
    u->baud = baud;

    // open serial port
    u->fd_uart = open(upath, DAP_UART_ACCESS_FLAGS);
    if (u->fd_uart == -1 ) {
        ASSERT(ASSERT_FAIL, "Failed to open port", strerror(errno))
        return DAP_DATA_INIT_ERROR;
    }

    // set com attributes
    if (dap_port_init_attributes(u) != DAP_SUCCESS) {
        ASSERT(ASSERT_FAIL, "Could not intialize UART attributes", strerror(errno))
        return DAP_DATA_INIT_ERROR;
    }

    tcflush(u->fd_uart, TCIFLUSH);
    return results;
}

// close uart
void dap_port_close (struct DAP_UART *u) {
    close (u->fd_uart);
    u->fd_uart = 0;
}

// clear uart recieve buffer
void dap_port_clr_rx_buffer (struct DAP_UART *u) {
    memset(u->buf_rx, 0, sizeof(u->buf_rx));
}

// clear uart transmit buffer
void dap_port_clr_tx_buffer (struct DAP_UART *u) {
    memset(u->buf_tx, 0, sizeof(u->buf_tx));
}

// transmit data in  buf_tx buffer
int dap_port_transmit (struct DAP_UART *u) {

    int result;

    if (u->fd_uart == 0) {
        ASSERT(ASSERT_FAIL, "Transmit: UART port not open", strerror(errno))
        return -1;
    }

    result = write(u->fd_uart, u->buf_tx, sizeof(u->buf_tx)); //TODO - size?
    if (result == -1) {
        ASSERT(ASSERT_FAIL, "Transmit: Could not transmit UART data", strerror(errno))
    }

    // returns number of bytes transmitted or -1 if failed
    return result;

}

// recieve data in  buf_rx buffer
int dap_port_recieve (struct DAP_UART *u) {

    int result;

    if (u->fd_uart == 0) {
        ASSERT(ASSERT_FAIL, "Recieve: UART port not open", strerror(errno))
        return -1;
    }

    result = read(u->fd_uart, u->buf_rx, sizeof(u->buf_rx));    //TODO - size?
    if (result == -1) {
        ASSERT(ASSERT_FAIL, "Recieve: Could not recieve UART data", strerror(errno))
    }

    // returns number of bytes recieved, or 0 for EOF, or -1 if failed
    return result;

}
