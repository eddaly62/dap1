// dap_uart.c

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>
#include <errno.h>
#include <semaphore.h>
#include "dap_config.h"
#include "dap.h"

struct DAP_UART {
    unsigned char buf_rx[DAP_UART_BUF_SIZE];    // rcv buffer (circular buffer)
    unsigned int num_unread;                    // number of unread bytes
    unsigned char *read_ptr;                    // pntr in buf_rx to start of unread data
    unsigned char buf_tx[DAP_UART_BUF_SIZE];    // tx buffer (linear buffer)
    unsigned int num_to_tx;                     // number of bytes to transmit
    int fd_uart;                                // file descriptor of uart pipe
    speed_t baud;
    struct termios tty;
    sem_t *gotdata_sem;                         // pointer to app provided semaphore
    int status;
    pthread_t tid;                              // thread id
    enum DAP_DATA_SRC id;
};

// UART data structures
static struct DAP_UART uart1;
static struct DAP_UART uart2;
static pthread_t tid_uart;
static pthread_mutex_t cpmutex = PTHREAD_MUTEX_INITIALIZER;

// clear uart struct (helper function)
static void dap_clr_uart_struct (struct DAP_UART *u) {
    memset(u, 0, sizeof(struct DAP_UART));
}

// close uart (helper function)
static void dap_port_close (struct DAP_UART *u) {
    close (u->fd_uart);
    u->fd_uart = DAP_ERROR;     // don't use 0, 0 = stdin
}

// return a pointer to the selected uart struct (helper function)
static struct DAP_UART * dap_src_select (enum DAP_DATA_SRC ds) {

    struct DAP_UART *u = NULL;

    switch (ds) {
        case DAP_DATA_SRC1:
        u = &uart1;
        break;

        case DAP_DATA_SRC2:
        u = &uart2;
        break;

        default:
        ASSERT((ds < DAP_NUM_OF_SRC), "UART: Data source out of range", NULL)
        break;
    }

    return u;
}

// set app provided semaphore
// used to signal app has data ready to read
int dap_port_set_sem (enum DAP_DATA_SRC ds, sem_t *s) {

    struct DAP_UART *u;

    u = dap_src_select(ds);
    ASSERT((u != NULL), "UART: null pointer to semaphore", DAP_ERROR)
    u->gotdata_sem = s;

    return DAP_SUCCESS;
}

// set uart attributes (helper function)
static int dap_port_init_attributes (struct DAP_UART *u) {
    int results = DAP_SUCCESS;

    ASSERT((u->fd_uart > 2), "UART: fd_uart <= 0, can not set attributes", DAP_ERROR)

    tcgetattr(u->fd_uart, &u->tty);     // Get the current attributes of the first serial port
    FAIL_IF((results != 0), DAP_ERROR)

    cfsetispeed(&u->tty, u->baud);      // Set read speed
    cfsetospeed(&u->tty, u->baud);      // Set write speed

    u->tty.c_cflag &= ~PARENB;          // Disables the Parity Enable bit(PARENB)
    u->tty.c_cflag &= ~CSTOPB;          // Clear CSTOPB, configuring 1 stop bit
    u->tty.c_cflag &= ~CSIZE;           // Using mask to clear data size setting
    u->tty.c_cflag |= CS8;              // Set 8 data bits
    u->tty.c_cflag &= ~CRTSCTS;         // Disable Hardware Flow Control
                                        // TODO - Add Xon/Xoff flow control

    // set for raw mode, no processing by terminal driver
    // Noncanonical mode, disable signals, extended input processing, and echoing
    u->tty.c_lflag &= ~(ICANON | ISIG | IEXTEN | ECHO);

    // Disable special handling of CR, NL, and BREAK.
    // No 8th-bit stripping or parity error handling.
    // Disable START/STOP output flow control.
    u->tty.c_iflag &= ~(BRKINT | ICRNL | IGNBRK | IGNCR | INLCR | INPCK | ISTRIP | IXON | PARMRK);

    // Disable all output processing
    u->tty.c_oflag &= ~OPOST;

    // TODO - evaluate
    u->tty.c_cc[VMIN] = 1;
    u->tty.c_cc[VTIME] = 0; // Character-at-a-time input with blocking

    results = tcsetattr(u->fd_uart, TCSANOW, &u->tty);
    FAIL_IF((results != 0), DAP_ERROR)

    return results;
}

// initializes UART port
static int dap_port_init (enum DAP_DATA_SRC uid, struct DAP_UART *u, char *upath, speed_t baud, sem_t *sem) {

    int results = DAP_SUCCESS;

    dap_clr_uart_struct(u);

    ASSERT((upath[0] != 0), "UART path not intilaized", DAP_ERROR)
    ASSERT((baud != 0), "UART baud rate not intilaized", DAP_ERROR)
    ASSERT((u != NULL), "UART: DAP_UART struct pointer in not intilaized", DAP_ERROR)

    // save baud and semaphore
    u->id = uid;
    u->baud = baud;
    u->gotdata_sem = sem;

    // initialize read pointer to point to start of buffer
    u->read_ptr = u->buf_rx;

    // open serial port
    u->fd_uart = open(upath, DAP_UART_ACCESS_FLAGS);
    FAIL_IF((u->fd_uart == -1 ), DAP_ERROR);

    // set com attributes
    results = dap_port_init_attributes(u);
    ASSERT((results != DAP_ERROR), "Could not intialize UART attributes", DAP_ERROR)

    tcflush(u->fd_uart, TCIFLUSH);

    return results;
}


// determine pointer to first open address in circular buffer (helper function)
static unsigned char* dap_next_addr (struct DAP_UART *u) {

    unsigned char *maxptr;
    unsigned char *ptr;

    pthread_mutex_lock(&cpmutex);

    maxptr = u->buf_rx + DAP_UART_BUF_SIZE;
    ptr = u->read_ptr + u->num_unread;

    if (ptr >= maxptr){
        // circle back, adjust ptr
        ptr = (unsigned char *)(ptr - maxptr);
    }
    ASSERT(((ptr>=u->buf_rx) && (ptr < maxptr)), "UART: read_ptr is invalid", NULL)

    pthread_mutex_unlock(&cpmutex);

    return ptr;
}

// copy data to circular buffer (helper function)
static int dap_rx_cp (unsigned int num, const unsigned char *src, struct DAP_UART *u) {

    unsigned int i;
    unsigned int index;
    unsigned char *ptr;
    unsigned char *dst;
    unsigned char *srccp;

    ASSERT((num != 0), "UART Warning: n is equal to 0, nothing to do", DAP_ERROR)
    ASSERT((src != NULL), "UART: src pointer is NULL", DAP_ERROR)
    ASSERT((u != NULL), "UART: u pointer is NULL", DAP_ERROR)

    srccp = (unsigned char *)src;
    ptr = dap_next_addr(u);
    ASSERT((ptr >= u->buf_rx), "UART: ptr into buf_rx incorrect, too low", DAP_ERROR)
    ASSERT((ptr < (u->buf_rx + DAP_UART_BUF_SIZE)), "UART: ptr into buf_rx incorrect", DAP_ERROR)

    pthread_mutex_lock(&cpmutex);

    // save new read pointer location, if all data has been read
    if (u->num_unread == 0) {
        u->read_ptr = ptr;
    }

    index = ptr - u->buf_rx;

    // copy data
    for (i=0; i < num; i++) {
        dst = ptr + index;
        *dst = *srccp;
        srccp++;
        index++;
        index = index % DAP_UART_BUF_SIZE;
    }
    u->num_unread += num;

    pthread_mutex_unlock(&cpmutex);

    // signal to app, got data ready to read
    sem_post(u->gotdata_sem);

    ASSERT((u->num_unread < DAP_UART_BUF_SIZE), "UART: num_unread to large", DAP_ERROR)

    return DAP_SUCCESS;
}


// transmit data in  buf_tx buffer
int dap_port_transmit (enum DAP_DATA_SRC ds, unsigned char *buff, int len) {

    int result;
    struct DAP_UART *u;

    ASSERT((buff != NULL), "UART: Transmit, buff pointer is NULL", DAP_DATA_TX_ERROR)

    if (len == 0) {
        // no data to transmit
        return len;
    }

    u = dap_src_select(ds);
    ASSERT((u != NULL), "UART: Transmit, data source out of range", DAP_DATA_TX_ERROR)

    u->num_to_tx = len;
    memcpy(u->buf_tx, buff, len);

    ASSERT((u->fd_uart >= 0), "UART: Transmit, port not open", DAP_DATA_TX_ERROR)
    result = write(u->fd_uart, u->buf_tx, u->num_to_tx);
    FAIL_IF((result == -1), DAP_DATA_TX_ERROR)

    ASSERT((result == u->num_to_tx), "UART: Transmit, incomplete data write", DAP_DATA_TX_ERROR)

    u->num_to_tx = 0;

    // returns number of bytes transmitted or error
    return result;

}

// get data from circular buffer (helper function)
// returns number of bytes transfered to buff or error
static int dap_rx_get (const unsigned char *buff, struct DAP_UART *u) {

    unsigned int i;
    int numread;
    unsigned int index;
    unsigned char *dst;
    unsigned char *ptr;

    ASSERT((buff != NULL), "UART: dst pointer is NULL", DAP_ERROR)
    ASSERT((u != NULL), "UART: u pointer is NULL", DAP_ERROR)

    pthread_mutex_lock(&cpmutex);

    index = u->read_ptr - u->buf_rx;
    ASSERT((u->read_ptr >= u->buf_rx) && (index < DAP_UART_BUF_SIZE), "UART: index incorrect", DAP_ERROR)

    // copy data
    for (i=0; i < u->num_unread; i++) {
        dst = (unsigned char *)buff + i;
        ptr = u->read_ptr + index;
        *dst = *ptr;
        index++;
        index = index % DAP_UART_BUF_SIZE;
    }

    numread = u->num_unread;
    u->num_unread = 0;
    pthread_mutex_unlock(&cpmutex);

    return numread;
}

// receive data, returns number of bytes or error code (negative value), data copied to buff
int dap_port_receive (enum DAP_DATA_SRC ds, unsigned char *buff) {

    int result;
    int len;
    struct DAP_UART *u;

    ASSERT((buff != NULL), "UART: Receive, buff pointer is NULL", DAP_DATA_RX_ERROR)

    u = dap_src_select(ds);
    ASSERT((u != NULL), "UART: Receive, data source out of range", DAP_DATA_RX_ERROR)

    len = u->num_unread;
    if (len == 0) {
        // no data received
        return len;
    }

    result = dap_rx_get(buff, u);
    ASSERT((result != DAP_ERROR), "UART: Receive, could not get rx data", DAP_DATA_RX_ERROR)

    // returns number of bytes returned or -1 if failed
    return result;
}

// uart rx thread(s)
// one thread per uart port
static void *dap_uart_thr (void *arg) {

    int s;
    int result;
    unsigned char buf[DAP_UART_BUF_SIZE];
    struct DAP_UART *up;

    up = (struct DAP_UART *)arg;
    up->tid = pthread_self();

    while (up->status) {

        s = read(up->fd_uart, buf, DAP_READ_SIZE);
        FAIL_IF(((s == -1) && (errno != EAGAIN)), NULL)

        // store data
        if (s > 0) {
            result = dap_rx_cp(s, buf, up);
            ASSERT((result != DAP_ERROR), "UART: rx data not saved", NULL)
        }
        // interleave when rx threads run
        if (up->id == DAP_DATA_SRC1) {
            usleep(DAP_UART_1_TPOLL);
        }
        else {
            usleep(DAP_UART_2_TPOLL);
        }
    }
    return NULL;
}

// initialzie uarts
int dap_uart_init (void) {

    int result;

	// initializes UART port 1
	result = dap_port_init (DAP_DATA_SRC1, &uart1, DAP_UART_1, DAP_UART_1_BAUD, NULL);
    ASSERT((result != DAP_ERROR), "UART 1: Can not initialize", DAP_DATA_INIT_ERROR)

	// initializes UART port 2
	result = dap_port_init (DAP_DATA_SRC2, &uart2, DAP_UART_2, DAP_UART_2_BAUD, NULL);
    ASSERT((result != DAP_ERROR), "UART 2: Can not initialize", DAP_DATA_INIT_ERROR)

    // set threads to loop
    uart1.status = true;
    uart2.status = true;

	// create threads for reading UART inputs
	result = pthread_create(&tid_uart, NULL, dap_uart_thr, &uart1);
    FAIL_IF((result != 0), DAP_DATA_INIT_ERROR)

	result = pthread_create(&tid_uart, NULL, dap_uart_thr, &uart2);
    FAIL_IF((result != 0), DAP_DATA_INIT_ERROR)

    return DAP_SUCCESS;
}

// shut down uarts
void dap_uart_shutdown (void) {

    // shut down threads (before closing ports)
    uart1.status = false;
    uart2.status = false;

    pthread_join(uart1.tid, NULL);
    pthread_join(uart2.tid, NULL);

    // close uart 1
	dap_port_close (&uart1);

	// close uart 2
	dap_port_close (&uart2);

}
