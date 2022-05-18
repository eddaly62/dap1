#ifndef _DAP_H
#define _DAP_H	1

#ifdef __cplusplus
extern "C" {
#endif

// TODO - Review
#include <pthread.h>
#include <regex.h>
#include <stdbool.h>
#include <termios.h>
#include <semaphore.h>

// dap.h

// general use macros
#define ARRAY_SIZE(arr) (sizeof((arr)) / sizeof((arr)[0]))

// Assertion of truth macro
#define ASSERT_FAIL     0
#define ASSERT_PASS     1

#define ASSERT(cond, desc, serror) if( !(cond) )\
{fprintf(stderr, "ASSERT error, %s, line %d, file(%s), function(%s), errno(%s)\n", \
desc, __LINE__, __FILE__, __FUNCTION__, serror);}


// DAP return codes
enum DAP_RETURN_CODES {
    DAP_SUCCESS = 0,
    DAP_ERROR = -1,
    DAP_DATA_FLUSH_ERROR = -1000,
    DAP_DATA_INIT_ERROR,
    DAP_DATA_RX_ERROR,
    DAP_DATA_TX_ERROR,
    DAP_DATA_CTRL_ERROR,
    DAP_PATTERN_FIND_ERROR,
    DAP_AIO_BACKLIGHT_INC_ERROR,
    DAP_AIO_BACKLIGHT_DEC_ERROR,
    DAP_AIO_BACKLIGHT_LEVEL_ERROR,
    DAP_AIO_BACKLIGHT_QUERY_ERROR,
    DAP_AIO_BLACKOUT_SET_ERROR,
    DAP_AIO_BLACKOUT_QUERY_ERROR,
    DAP_AIO_LED_SET_ERROR,
    DAP_AIO_LED_QUERY_ERROR,
    DAP_AIO_POWER_SET_ERROR,
    DAP_AIO_POWER_QUERY_ERROR,
    DAP_AIO_TEMP_REPORT_ENB_ERROR,
    DAP_AIO_TEMP_QUERY_ERROR,
    DAP_AIO_TEMP_TEST_ERROR,
    DAP_AIO_TEMP_TEST_CLR_ERROR,
    DAP_AIO_PRINT_LOG_STRING_ERROR,
    DAP_AIO_HEATER_SET_ERROR,
    DAP_AIO_HEATER_QUERY_ERROR,
    DAP_AIO_HEATER_PWR_GET_ERROR,
    DAP_AIO_VC_PROGRAM_SET_ERROR,
    DAP_AIO_BOOT_ERROR,
    DAP_AIO_PORT_INIT_ERROR,
    DAP_AIO_FIRMWARE_QUERY_ERROR,
    DAP_DATA_CHECKSUM_ERROR,
    DAP_DATA_CHECKSUM_CALC_ERROR,
    DAP_PROG_ERROR,
    DAP_INIT_ERROR,
    DAP_SHUTDOWN_ERROR,
    DAP_DISPLAY_MAP_ERROR,
    DAP_DISPLAY_PAGE_COPY_ERROR,
    DAP_DISPLAY_PAGE_MERGE_ERROR,
    DAP_DISPLAY_PAGE_CLEAR_ERROR,
    DAP_DISPLAY_FE_ERROR,
    DAP_DISPLAY_XYPOS_ERROR,
    DAP_DISPLAY_RCPOS_ERROR,
    DAP_DISPLAY_CUSOR_ERROR,
    DAP_DISPLAY_MOVECURSOR_ERROR,
    DAP_DISPLAY_FONTSELECT_ERROR,
    DAP_DISPLAY_PRIMARYFONTSELECT_ERROR,
    DAP_DISPLAY_SECONDARYFONTSELECT_ERROR,
    DAP_DISPLAY_ALPHA_ATTRIBUTES_ERROR,
    DAP_DISPLAY_EDIT_ERROR,
    DAP_DISPLAY_MONITOR_ERROR,
    DAP_DISPLAY_BRIGHTNESS_ERROR,
    DAP_DISPLAY_RG_ERROR,
    DAP_DISPLAY_LINE_ERROR,
    DAP_DISPLAY_RECTANGLE_ERROR,
    DAP_DISPLAY_CIRCLE_ERROR,
    DAP_TOUCH_STATUS_ERROR,
};

// general definitions used by any dap functions
// =============================================

// dap pattern callback definitions
typedef void(*cb_func_c)(char*);
typedef void(*cb_func_i)(int);


// initialization and shutdown functions
// =======================================
int dap_init(void);
void dap_shutdown(void);

// pattern find
// =============

#define MAX_PATTERN_BUF_SIZE 100

#define MAXNUMTHR 2
#define MAXRESULTTBOXES (MAXNUMTHR+1)
#define MAXTHRIDX (MAXNUMTHR-1)
#define RESULTIDX (MAXNUMTHR)

#define RE_MATCH 0

// Pattern/Callback Look Up Table
struct DAP_PATTERN_CB{
    char pattern[MAX_PATTERN_BUF_SIZE];
    cb_func_c cb;
};

struct DAP_REGEX_RESULTS {
    char out[MAX_PATTERN_BUF_SIZE]; // string matched
    cb_func_c cb;                   // callback function pulled from lut
    regmatch_t  pmatch[1];          // start and end offset info were in string match was found
    int indexlut;                   // index in pattern lut match was found
    long idx;                       // thread index
    pthread_t tid;                  // thread id
    regoff_t len;                   // length of matched string
};

// public function prototypes
int dap_pattern_find(char *s, const struct DAP_PATTERN_CB *ptnlut, int len, struct DAP_REGEX_RESULTS *rt);


// pattern queue
// ==============

// declarations
#define MAX_PATTERN_Q_SIZE 6

struct DAP_PATTERN_QUEUE {
    int front;
    int rear;
    int qcount;
    struct DAP_REGEX_RESULTS rq[MAX_PATTERN_Q_SIZE];
};

// public function prototypes
void dap_pattern_queue_init(struct DAP_PATTERN_QUEUE *q);
void dap_pattern_queue_peek(struct DAP_PATTERN_QUEUE *q, struct DAP_REGEX_RESULTS *data);
bool dap_pattern_queue_is_empty(struct DAP_PATTERN_QUEUE *q);
bool dap_pattern_queue_is_full(struct DAP_PATTERN_QUEUE *q);
int  dap_pattern_queue_size(struct DAP_PATTERN_QUEUE *q);
void dap_pattern_queue_insert(struct DAP_PATTERN_QUEUE *q, struct DAP_REGEX_RESULTS *data);
void dap_pattern_queue_remove(struct DAP_PATTERN_QUEUE *q, struct DAP_REGEX_RESULTS *data);

// util functions

// elapsed time
// =============

enum ELTIME {
    START = 0,
    END,
};

// public function prototypes
// returns elaped time from START to END in usec.
long long elapsed_time(enum ELTIME sts, struct timeval *start, struct timeval *end);


// data source management (uart)
// =============================

// these definitions are platform dependant
#define DAP_UART_BUF_SIZE   1024
#define DAP_UART_1_BAUD     B9600
#define DAP_UART_1          ("/dev/ttymxc1")
#define DAP_UART_2_BAUD     B9600
#define DAP_UART_2          ("/dev/ttymxc3")

enum DAP_DATA_SRC {
    DAP_DATA_SRC1,          // uart 1
    DAP_DATA_SRC2,          // uart 2
    DAP_NUM_OF_SRC,         // total number of uarts
};

struct DAP_UART {
    unsigned char buf_rx[DAP_UART_BUF_SIZE];    // rcv buffer (circular buffer)
    unsigned int num_unread;                    // number of unread bytes
    unsigned char *read_ptr;                    // pntr in buf_rx to start of unread data
    unsigned char buf_tx[DAP_UART_BUF_SIZE];    // tx buffer (linear buffer)
    unsigned int num_to_tx;                     // number of bytes to transmit
    int fd_uart;                                // file descriptor of uart pipe
    speed_t baud;
    struct termios tty;
    sem_t *gotdata_sem;
};

// public function prototypes
// TODO - list needs refining

// initializes UART port
int dap_port_init (struct DAP_UART *u, char *upath, speed_t baud, sem_t *sem);
// close uart
void dap_port_close (struct DAP_UART *u);
// clear uart recieve buffer
void dap_port_clr_rx_buffer (struct DAP_UART *u);
// clear uart transmit buffer
void dap_port_clr_tx_buffer (struct DAP_UART *u);
// transmit data in  buf_tx buffer
int dap_port_transmit (struct DAP_UART *u);
// recieve data in  buf_rx buffer
int dap_port_recieve (struct DAP_UART *u);
// initialize uarts
int dap_uart_init (void);
// shut down uarts
void dap_uart_shutdown (void);




#ifdef __cplusplus
}
#endif

#endif
