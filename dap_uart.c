// dap_uart.c
// TODO - spell recieve correctly (receive)

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>
#include <errno.h>
#include <semaphore.h>
#include "dap.h"

// these definitions are platform dependant
#define DAP_UART_BUF_SIZE   1024
#define DAP_UART_1_BAUD     (B9600)
#define DAP_UART_1          "/dev/ttymxc1" // Toradex UART2
#define DAP_UART_2_BAUD     (B9600)
#define DAP_UART_2          "/dev/ttymxc3" // Toradex UART3
#define DAP_READ_SIZE       20


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
    int status;
    pthread_t tid;                              // thread id
    enum DAP_DATA_SRC id;
};

// UART data structures
static struct DAP_UART uart1;
static struct DAP_UART uart2;
static pthread_t tid_uart;
pthread_mutex_t cpmutex = PTHREAD_MUTEX_INITIALIZER;

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
    u->fd_uart = DAP_ERROR;     // don't use 0, 0 = stdin
}

// TODO - add a routine that does a complete clear of a DAP_UART structure

// set uart attributes (helper function)
static int dap_port_init_attributes (struct DAP_UART *u) {

    if (u->fd_uart < 0) {
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
static int dap_port_init (enum DAP_DATA_SRC uid, struct DAP_UART *u, char *upath, speed_t baud, sem_t *sem) {

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
    u->id = uid;
    u->baud = baud;
    u->gotdata_sem = sem;

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

// TODO Needed?
#if 0
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
#endif

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
static int dap_rx_cp (unsigned int num, const unsigned char *src, struct DAP_UART *u) {

    unsigned int i;
    unsigned int index;
    unsigned char *ptr;
    unsigned char *dst;
    unsigned char *srccp;

    ASSERT((num != 0), "UART Warning: n is equal to 0, nothing to do", "-1")
    if (num == 0) {
        // nothing to do
        return DAP_ERROR;
    }
    ASSERT((src != NULL), "UART: src pointer is NULL", "-1")
    if (src == NULL) {
        return DAP_ERROR;
    }
    ASSERT((u != NULL), "UART: u pointer is NULL", "-1")
    if (u == NULL) {
        return DAP_ERROR;
    }
    printf("rx data before inserting in ring buffer = %s\n", src); //TODO remove


    srccp = (unsigned char *)src;
    ptr = dap_next_addr(u);

    pthread_mutex_lock(&cpmutex);

    // save new read pointer location
    u->read_ptr = ptr;

    index = ptr - u->buf_rx;
    ASSERT((ptr >= u->buf_rx), "UART: index into buf_rx incorrect, seg fault possible", "-1")

    // copy data
    for (i=0; i < num; i++) {
        dst = ptr + index;
        *dst = *srccp;
        srccp++;
        index++;
        index = index % DAP_UART_BUF_SIZE;
        printf("%s: uid=%d, copied char =%c,\n", __FUNCTION__, u->id, *dst); //TODO remove
    }

    u->num_unread += num;
    ASSERT((u->num_unread < DAP_UART_BUF_SIZE), "UART: num_unread to large, seg fault possible", "-1")

    pthread_mutex_unlock(&cpmutex);

    return DAP_SUCCESS;
}

#if 0
// TODO needed?
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
#endif

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
    fprintf(stdout, "buf_tx = %s function(%s)\n",u->buf_tx, __FUNCTION__); // TODO remove
    if (u->fd_uart <= 0) {
        ASSERT(ASSERT_FAIL, "UART: Transmit, port not open", strerror(errno))
        return DAP_DATA_TX_ERROR;
    }

    result = write(u->fd_uart, u->buf_tx, u->num_to_tx);
    fprintf(stdout, "result (bytes written) = %d\n", result); // TODO remove
    if (result == -1) {
        ASSERT((result != -1), "UART: Transmit, write data failed", strerror(errno))
        return DAP_DATA_TX_ERROR;
    }
    ASSERT((result == u->num_to_tx), "UART: Transmit, incomplete data write", strerror(errno))
    //usleep(186000); //TODO - remove

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

    ASSERT((buff != NULL), "UART: dst pointer is NULL", "-1")
    if (buff == NULL) {
        return DAP_ERROR;
    }
    ASSERT((u != NULL), "UART: u pointer is NULL", "-1")
    if (u == NULL) {
        return DAP_ERROR;
    }

    index = u->read_ptr - u->buf_rx;
    printf ("index = %d, funct(%s)\n", index, __FUNCTION__); //TODO remove
    ASSERT((u->read_ptr >= u->buf_rx) && (index < DAP_UART_BUF_SIZE), "UART: index incorrect, seg fault possible", "-1")
    if ((index >= DAP_UART_BUF_SIZE) || (u->read_ptr < u->buf_rx)){
        return DAP_ERROR;
    }

    // copy data
    for (i=0; i < u->num_unread; i++) {
        dst = (unsigned char *)buff + i;
        ptr = u->read_ptr + index;
        *dst = *ptr;
        index++;
        index = index % DAP_UART_BUF_SIZE;
        printf("index=%d, i=%d, *read_ptr+index=%c\n",index, i, *dst); //TODO remove
    }

    numread = u->num_unread;
    pthread_mutex_lock(&cpmutex);
    u->num_unread = 0;
    pthread_mutex_unlock(&cpmutex);

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

    u = dap_src_select(ds);
    if (u == NULL) {
        ASSERT((u != NULL), "UART: Receive, data source out of range", "DAP_DATA_RX_ERROR")
        return DAP_DATA_RX_ERROR;
    }

    len = u->num_unread;
    if (len == 0) {
        // no data received
        return len;
    }

    result = dap_rx_get(buff, u);
    if (result == DAP_ERROR) {
        ASSERT((result != DAP_ERROR), "UART: Receive, could not get rx data", "DAP_DATA_RX_ERROR")
        return DAP_DATA_RX_ERROR;
    }

    // returns number of bytes returned or -1 if failed
    return result;
}

// uart rx thread
static void *dap_uart_thr(void *arg) {

    int s;
    //int src;
    unsigned char buf[DAP_UART_BUF_SIZE];
    struct DAP_UART *up;

    up = (struct DAP_UART *)arg;
    up->tid = pthread_self();
    printf("uid = %d, tid = %lu\n", up->id, up->tid); // TODO remove

    while (up->status) {

        s = read(up->fd_uart, buf, DAP_READ_SIZE);
        //fprintf(stdout,"read unblocked\n");
        //fprintf(stdout,"s = %d, errno(%d), strerror(%s)\n", s, errno, strerror(errno));
        if (s == -1){

            if (errno != EAGAIN) {
                // read error, log, no data to read
                ASSERT((s != -1), "UART: read error", strerror(errno))
            }
            // TODO - add more conditions
        }
        else {
            // store data
            //src = dap_uart_rx_copy (s, up->fd_uart, buf);
            fprintf(stdout, "uid = %d, recieved data = %d bytes\n", up->id, s);
            dap_rx_cp (s, buf, up);

            //ASSERT((src != DAP_ERROR), "UART: rx data not saved", "-1")
        }

        //usleep(200000);
        sleep(1);
    }
    return NULL;
}

// initialzie uarts
int dap_uart_init (void) {

    int result = DAP_SUCCESS;

	// initializes UART port 1
	result = dap_port_init (DAP_DATA_SRC1, &uart1, DAP_UART_1, DAP_UART_1_BAUD, NULL); // TODO - fix sema
	if (result != DAP_SUCCESS) {
		ASSERT(ASSERT_FAIL, "UART 1 Initialization: Can not initialize", "-1")
        return DAP_DATA_INIT_ERROR;
	}

	// initializes UART port 2
	result = dap_port_init (DAP_DATA_SRC2, &uart2, DAP_UART_2, DAP_UART_2_BAUD, NULL); // TODO - fix sema
	if (result != DAP_SUCCESS) {
		ASSERT(ASSERT_FAIL, "UART 2 Initialization: Can not initialize", "-1")
        return DAP_DATA_INIT_ERROR;
	}

    // set threads to loop
    uart1.status = true; // TODO - may be a config option 
    uart2.status = true;

	// create threads for reading UART inputs
	result = pthread_create(&tid_uart, NULL, dap_uart_thr, &uart1);
    if (result != 0) {
		ASSERT(ASSERT_FAIL, "UART 1 Rx thread creation failed", "-1")
        return DAP_DATA_INIT_ERROR;
    }

	result = pthread_create(&tid_uart, NULL, dap_uart_thr, &uart2);
    if (result != 0) {
		ASSERT(ASSERT_FAIL, "UART 2 Rx thread creation failed", "-1")
        return DAP_DATA_INIT_ERROR;
    }

    return result;
}

void dap_uart_shutdown (void) {

    // shut down threads
    uart1.status = false;
    uart2.status = false;

    sleep(1); // TODO - use a join operation

    // close uart 1
	dap_port_close (&uart1);

	// close uart 2
	dap_port_close (&uart2);

}
