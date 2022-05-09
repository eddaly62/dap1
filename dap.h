#ifndef _DAP_H
#define _DAP_H	1

#ifdef __cplusplus
extern "C" {
#endif

// dap.h

// general use macros
#define ARRAY_SIZE(arr) (sizeof((arr)) / sizeof((arr)[0]))


// TODO change printf to write
// Assertion of truth macro
#define ASSERT(cond, desc) if( !(cond) )\
{printf( "assertion error, %s, line %d, file(%s)\n", \
desc, __LINE__, __FILE__ );}


// DAP return codes
enum DAP_RETURN_CODES {
    DAP_SUCCESS = 0,
    DAP_DATA_FLUSH_ERROR = -1000,
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


// pattern find
// =============

#define MAX_PATTERN_BUF_SIZE 100

#define MAXNUMTHR 2
#define MAXRESULTTBOXES (MAXNUMTHR+1)
#define MAXTHRIDX (MAXNUMTHR-1)
#define RESULTIDX (MAXNUMTHR)

#define RE_MATCH 0

// dap pattern callback definitions
typedef void(*cb_func_c)(char*);
typedef void(*cb_func_i)(int);

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

// function prototypes
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

// function prototypes
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

// returns elaped time from START to END in usec.
long long elapsed_time(enum ELTIME sts, struct timeval *start, struct timeval *end);

#ifdef __cplusplus
}
#endif

#endif
