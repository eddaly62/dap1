// dap_uart.c
// TODO - spell recieve correctly (receive)

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>
#include <errno.h>
#include <sys/epoll.h>
#include <semaphore.h>
#include "dap.h"

// these definitions are platform dependant
#define DAP_UART_BUF_SIZE   1024
#define DAP_UART_1_BAUD     B9600
#define DAP_UART_1          ("/dev/ttymxc1") // Toradex UART2
#define DAP_UART_2_BAUD     B9600
#define DAP_UART_2          ("/dev/ttymxc3") // Toradex UART3

// Maximum number of events to be returned from a single epoll_wait() call
#define EP_MAX_EVENTS 5

// UART open attributes
// O_RDWR Read/write access to the serial port
// O_NOCTTY No terminal will control the process
// O_NDELAY Use non-blocking I/O
#define DAP_UART_ACCESS_FLAGS (O_RDWR | O_NOCTTY | O_NDELAY)

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
};

// uart io multiplexing
struct DAP_UART_EPOLL {
    int epfd;
    int numOpenFds;
    struct epoll_event ev;
    struct epoll_event evlist[EP_MAX_EVENTS];
};

// UART data structures
static struct DAP_UART uart1;
static struct DAP_UART uart2;
static struct DAP_UART_EPOLL uep;
static pthread_t tid_uart;

// clear uart recieve buffer
static void dap_port_clr_rx_buffer (struct DAP_UART *u) {
    memset(u->buf_rx, 0, sizeof(u->buf_rx));
    u->read_ptr = u->buf_rx;
    u->num_unread = 0;
}

// clear uart transmit buffer (helper function)
static void dap_port_clr_tx_buffer (struct DAP_UART *u) {
    memset(u->buf_tx, 0, sizeof(u->buf_tx));
    u->num_to_tx = 0;
}

// close uart (helper function)
static void dap_port_close (struct DAP_UART *u) {
    close (u->fd_uart);
    u->fd_uart = 0;
    dap_port_clr_rx_buffer (u); // TODO remove?
    dap_port_clr_tx_buffer (u); // TODO remove?
}

// TODO - add a routine that does a complete clear of a DAP_UART structure

// set uart attributes (helper function)
static int dap_port_init_attributes (struct DAP_UART *u) {

    if (u->fd_uart <= 0) {
        ASSERT(ASSERT_FAIL, "UART: fd_uart <= 0, can not set attributes", "-1")
        return -1;
    }

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
        return DAP_ERROR;
    }
    return DAP_SUCCESS;
}

// TODO - sem init rethink
// initializes UART port
int dap_port_init (struct DAP_UART *u, char *upath, speed_t baud, sem_t *sem) {

    int results = DAP_SUCCESS;

    dap_port_clr_rx_buffer(u);
    dap_port_clr_tx_buffer(u);

    ASSERT((upath[0] != 0), "UART path not intilaized", "0")
    ASSERT((baud != 0), "UART baud rate not intilaized", "0")
    ASSERT((u != NULL), "UART: DAP_UART struct pointer in not intilaized", "0")
    if ((upath[0] == 0) || (baud == 0) || (u == NULL)){
        return DAP_ERROR;
    }

    // save baud and semaphore
    u->baud = baud;
    if (sem != NULL) {
        u->gotdata_sem = sem;
    }

    // open serial port
    u->fd_uart = open(upath, DAP_UART_ACCESS_FLAGS);
    if (u->fd_uart == -1 ) {
        ASSERT(ASSERT_FAIL, "Failed to open port", strerror(errno))
        return DAP_ERROR;
    }

    // set com attributes
    if (dap_port_init_attributes(u) != DAP_SUCCESS) {
        ASSERT(ASSERT_FAIL, "Could not intialize UART attributes", strerror(errno))
        return DAP_ERROR;
    }

    tcflush(u->fd_uart, TCIFLUSH);

    return results;
}


// given a fd (descriptor) determine which DAP_UART struct to use (helper function)
static int dap_which_uart(int fd, struct DAP_UART *u1, struct DAP_UART *u2) {

    if ((u1->fd_uart <= 0) && (u2->fd_uart <= 0)){
        // both UART ports are not initialized
        ASSERT(ASSERT_FAIL, "UART: Both uarts have not been opened sucessfully", "fd_uart<=0")
        return DAP_ERROR;
    }
    if (fd == u1->fd_uart) {
        return DAP_DATA_SRC1;
    }
    else if (fd == u2->fd_uart) {
        return DAP_DATA_SRC2;
    }
    else {
        // note: a failed open will have a -1 stored in the descriptor
        // and a uart port that is unused will have a 0 stored in the file descriptor
        ASSERT(ASSERT_FAIL, "UART: Could not determine which uart has created event", "-1")
        return DAP_ERROR;
    }
}

// determine pointer to first open address in circular buffer (helper function)
static unsigned char* dap_next_addr(struct DAP_UART *u) {

    unsigned char *maxptr;
    unsigned char *ptr;

    maxptr = u->buf_rx + DAP_UART_BUF_SIZE;
    ptr = u->read_ptr + u->num_unread;

    if (ptr >= maxptr){
        // circle back, adjust ptr
        ptr = (unsigned char *)(ptr - maxptr);
    }
    ASSERT(((ptr < maxptr) && (ptr >= u->buf_rx)), "UART: first open pointer out of range", "-1")

    return ptr;
}

// copy data to circular buffer (helper function)
static void dap_rx_cp (unsigned int num, unsigned char *src, struct DAP_UART *u) {

    unsigned int i;
    unsigned int index;
    unsigned char *ptr;
    unsigned char *dst;

    ASSERT((num != 0), "UART Warning: n is equal to 0, nothing to do", "-1")
    if (num == 0) {
        // nothing to do
        return;
    }
    ASSERT((src != NULL), "UART: src pointer is NULL", "-1")
    if (src == NULL) {
        return;
    }
    ASSERT((u != NULL), "UART: u pointer is NULL", "-1")
    if (u == NULL) {
        return;
    }

    ptr = dap_next_addr(u);

    index = ptr - u->buf_rx;
    ASSERT((index < DAP_UART_BUF_SIZE), "UART: index to large, seg fault possible", "-1")

    // copy data
    for (i=0; i < num; i++) {
        dst = ptr + index;
        *dst = *src;
        src++;
        index++;
        index = index % DAP_UART_BUF_SIZE;
    }

    u->num_unread += num;
    ASSERT((u->num_unread < DAP_UART_BUF_SIZE), "UART: num_unread to large, seg fault possible", "-1")
    u->read_ptr = ptr + index;
    ASSERT((index < DAP_UART_BUF_SIZE), "UART: read_ptr to large, seg fault possible", "-1")
}

// copy recieve data to uart structs (helper function)
static int dap_uart_rx_copy (int num, int fd, unsigned char *buf) {

    int src;

    src = dap_which_uart(fd, &uart1, &uart2);
    ASSERT((src != DAP_ERROR), "UART: Can not copy rx data, possible invalid fd", "-1")

    switch (src) {
        case DAP_DATA_SRC1:
        dap_rx_cp (num, buf, &uart1);
        if (uart1.gotdata_sem != NULL){
            if (sem_post(uart1.gotdata_sem) == -1) {
                ASSERT(ASSERT_FAIL, "UART: Could not post semaphore for uart1", strerror(errno))
            }
        }
        ASSERT((uart1.gotdata_sem != NULL), "UART, WARNING: semaphore for uart1 is set to NULL", "app not signaled")
        break;

        case  DAP_DATA_SRC2:
        dap_rx_cp (num, buf, &uart2);
        if (uart2.gotdata_sem != NULL){
            if (sem_post(uart2.gotdata_sem) == -1) {
                ASSERT(ASSERT_FAIL, "UART: Could not post semaphore for uart2", strerror(errno))
            }
        }
        ASSERT((uart2.gotdata_sem != NULL), "UART: WARNING semaphore for uart2 is set to NULL", "app not signaled")
        break;

        default:
        // log error
        ASSERT(ASSERT_FAIL, "UART: Could not copy rx data", "rx data not copied")
        break;
    }

    // returns the dap data source that rx was copied to or error
    return src;
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
        ASSERT((ds < DAP_NUM_OF_SRC), "UART: Data source out of range", "no selection made, rx/tx fail")
        break;
    }

    return u;
}

// transmit data in  buf_tx buffer
int dap_port_transmit (enum DAP_DATA_SRC ds, unsigned char *buff, int len) {

    int result;
    struct DAP_UART *u;

    ASSERT((buff != NULL), "UART: Transmit, buff pointer is NULL", "DAP_DATA_TX_ERROR")
    if (buff == NULL) {
        return DAP_DATA_TX_ERROR;
    }

    if (len == 0) {
        // no data to transmit
        return len;
    }

    u = dap_src_select (ds);
    if (u == NULL) {
        ASSERT((u != NULL), "UART: Transmit, data source out of range", "nothing transmitted")
        return DAP_DATA_TX_ERROR;
    }

    u->num_to_tx = len;
    memcpy(u->buf_tx, buff, len);

    if (u->fd_uart <= 0) {
        ASSERT(ASSERT_FAIL, "UART: Transmit, port not open", strerror(errno))
        return DAP_DATA_TX_ERROR;
    }

    result = write(u->fd_uart, u->buf_tx, u->num_to_tx);
    if (result == -1) {
        ASSERT((result != -1), "UART: Transmit, write data failed", strerror(errno))
        return DAP_DATA_TX_ERROR;
    }
    ASSERT((result == u->num_to_tx), "UART: Transmit, incomplete data write", strerror(errno))

    u->num_to_tx = 0;

    // returns number of bytes transmitted or error
    return result;

}

// get data from circular buffer (helper function)
// returns number of bytes transfered to buff or error
static int dap_rx_get (unsigned char *buff, struct DAP_UART *u) {

    unsigned int i;
    int numread;
    unsigned int index;
    unsigned char *dst;

    ASSERT((buff != NULL), "UART: dst pointer is NULL", "-1")
    if (buff == NULL) {
        return DAP_ERROR;
    }
    ASSERT((u != NULL), "UART: u pointer is NULL", "-1")
    if (u == NULL) {
        return DAP_ERROR;
    }

    index = u->read_ptr - u->buf_rx;
    ASSERT((index < DAP_UART_BUF_SIZE), "UART: index to large, seg fault possible", "-1")
    if (index >= DAP_UART_BUF_SIZE){
        return DAP_ERROR;
    }

    // copy data
    for (i=0; i < u->num_unread; i++) {
        dst = buff + i;
        *dst = *(u->buf_rx + index);
        index++;
        index = index % DAP_UART_BUF_SIZE;
    }

    numread = u->num_unread;
    u->num_unread = 0;
    return numread;
}

// receive data, returns number of bytes or error code (negative value), data copied to buff
int dap_port_recieve (enum DAP_DATA_SRC ds, unsigned char *buff) {

    int result;
    int len;
    struct DAP_UART *u;

    ASSERT((buff != NULL), "UART: Receive, buff pointer is NULL", "DAP_DATA_RX_ERROR")
    if (buff == NULL) {
        return DAP_DATA_RX_ERROR;
    }

    u = dap_src_select (ds);
    if (u == NULL) {
        ASSERT((u != NULL), "UART: Receive, data source out of range", "DAP_DATA_RX_ERROR")
        return DAP_DATA_RX_ERROR;
    }

    len = u->num_unread;
    if (len == 0) {
        // no data received
        return len;
    }

    result = dap_rx_get (buff, u);
    if (result == DAP_ERROR) {
        ASSERT((result != DAP_ERROR), "UART: Receive, could not get rx data", "DAP_DATA_RX_ERROR")
        return DAP_DATA_RX_ERROR;
    }

    // returns number of bytes returned or -1 if failed
    return result;
}

// Initializing for event polling of rx uart ports
static int dap_uart_epoll_init (struct DAP_UART_EPOLL *uep, struct DAP_UART *u1, struct DAP_UART *u2) {

    // notes:
    // - epoll requires Linux kernel 2.6 or better
    // - initialzie all ports with dap_port_init prior to calling

    int result = DAP_SUCCESS;

    // create an epoll descriptor
    uep->epfd = epoll_create(DAP_NUM_OF_SRC);
    ASSERT((uep->epfd != -1), "UART EPOLL: Could not create an epoll descriptor - epoll_create", strerror(errno))
    if (uep->epfd == -1){
        return DAP_ERROR;
    }

    if (u1->fd_uart >= 0) {
        // add the u1 file descriptor to the list of i/o to watch
        // set interest list: unblocked read possible (EPOLLIN) and input data has been recieved (EPOLLET, edge triggered)
        uep->ev.data.fd = u1->fd_uart;
        uep->ev.events = EPOLLIN | EPOLLET;
        if (epoll_ctl(uep->epfd, EPOLL_CTL_ADD, u1->fd_uart, &uep->ev) == -1) {
            ASSERT(ASSERT_FAIL, "UART EPOLL: Could not add uart1 - epoll_ctl", strerror(errno))
            return DAP_ERROR;
        }
    }

    if (u2->fd_uart >= 0) {
        // add the u2 file descriptor to the list of i/o to watch
        // set interest list: unblocked read possible (EPOLLIN) and input data has been recieved (EPOLLET, edge triggered)
        uep->ev.data.fd = u2->fd_uart;
        uep->ev.events = EPOLLIN | EPOLLET;
        if (epoll_ctl(uep->epfd, EPOLL_CTL_ADD, u2->fd_uart, &uep->ev) == -1) {
            ASSERT(ASSERT_FAIL, "UART EPOLL: Could not add uart2 - epoll_ctl", strerror(errno))
            return DAP_ERROR;
        }
    }

    return result;
}

// uart rx thread
static void *dap_uart_epoll_thr(void *arg) {

    int ready;
    int s;
    int j;
    int src;
    unsigned char buf[DAP_UART_BUF_SIZE];

    uep.numOpenFds = DAP_NUM_OF_SRC;

    while (uep.numOpenFds > 0) {

        /* Fetch up to MAX_EVENTS items from the ready list */
        ready = epoll_wait(uep.epfd, uep.evlist, EP_MAX_EVENTS, -1);
        if (ready == -1) {
            if (errno == EINTR) {   // TODO - add more sigs ?
                // Restart if interrupted by signal
                continue;
            }
            else {
                ASSERT((ready != -1), "UART: epoll_wait error", strerror(errno))
                return NULL;
            }
        }

        /* process returned list of events */
        for (j = 0; j < ready; j++) {

            if (uep.evlist[j].events & EPOLLIN) {
                s = read(uep.evlist[j].data.fd, buf, DAP_UART_BUF_SIZE);
                if (s == -1){
                    // read error, log, no data to read
                    ASSERT((s != -1), "UART: read error", strerror(errno))
                }
                else {
                    // store data
                    src = dap_uart_rx_copy (s, uep.evlist[j].data.fd, buf);
                    ASSERT((src != DAP_ERROR), "UART: rx data not saved", "-1")
                }
            }
            else if (uep.evlist[j].events & EPOLLHUP) {
                // Hang up event, Lost connection, log
                ASSERT(ASSERT_FAIL, "UART: Hang up, Lost UART connection", strerror(errno))
                // close(uep); // TODO - investigate
            }
            else if (uep.evlist[j].events & EPOLLERR) {
                // log epoll error
                ASSERT(ASSERT_FAIL, "UART: epoll error", strerror(errno))
            }
        }
    }
    return NULL;
}

// initialzie uarts
int dap_uart_init (void) {

    int result = DAP_SUCCESS;

	// initializes UART port 1
	result = dap_port_init (&uart1, DAP_UART_1, DAP_UART_1_BAUD, NULL); // TODO - fix sema
	if (result != DAP_SUCCESS) {
		ASSERT(ASSERT_FAIL, "UART 1 Initialization: Can not initialize", "-1")
        return DAP_DATA_INIT_ERROR;
	}

	// initializes UART port 2
	result = dap_port_init (&uart1, DAP_UART_2, DAP_UART_2_BAUD, NULL); // TODO - fix sema
	if (result != DAP_SUCCESS) {
		ASSERT(ASSERT_FAIL, "UART 2 Initialization: Can not initialize", "-1")
        return DAP_DATA_INIT_ERROR;
	}

    // create an epoll object (before creating the epoll thread)
	result = dap_uart_epoll_init (&uep, &uart1, &uart2);
	if (result != DAP_SUCCESS) {
		ASSERT(ASSERT_FAIL, "UART epoll initialization failed", "-1")
        return DAP_DATA_INIT_ERROR;
	}

	// create thread for reading UART inputs
	result = pthread_create(&tid_uart, NULL, dap_uart_epoll_thr, NULL);
    if (result != 0) {
		ASSERT(ASSERT_FAIL, "UART epoll thread creation failed", "-1")
        return DAP_DATA_INIT_ERROR;
    }

    return result;
}

void dap_uart_shutdown (void) {

    // close uart 1
	dap_port_close (&uart1);

	// close uart 2
	dap_port_close (&uart2);

    // shut down epoll thread
    uep.numOpenFds = 0;

    // TODO add close for thread and epoll
}
