// dap_pattern.c
// TODO - add file and function headers

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
#include "dap_config.h"
#include "dap.h"

// TODO - not thread safe, make some changes
// variables
static pthread_t tid;                   // thread id
static pthread_barrier_t b;             // barrier used to synchronize search threads
static char in[MAX_PATTERN_BUF_SIZE];   // input buffer
static struct DAP_REGEX_RESULTS reresults[MAXRESULTTBOXES];
static struct DAP_PATTERN_CB *re_cb_lut_ptr;
static int relutsize;


// clear results array
static void clearresults(void) {
    int i;

    for (i = 0; i < MAXRESULTTBOXES; i++) {
        memset (&reresults[i], 0, sizeof(reresults[i]));
    }
}

// get results
static void getresults(void) {
    long i;

    // check results
    for (i = 0; i < MAXNUMTHR; i++) {

        // check and see if any of the threads found a match
        if (reresults[i].cb != NULL) {
            reresults[RESULTIDX].cb = reresults[i].cb;
            reresults[RESULTIDX].indexlut = reresults[i].indexlut;
            reresults[RESULTIDX].pmatch[0].rm_so = reresults[i].pmatch[0].rm_so;
            reresults[RESULTIDX].pmatch[0].rm_eo = reresults[i].pmatch[0].rm_eo;
            reresults[RESULTIDX].tid = reresults[i].tid;
            reresults[RESULTIDX].idx = reresults[i].idx;
            reresults[RESULTIDX].len = reresults[i].len;
            memcpy(reresults[RESULTIDX].out, reresults[i].out, sizeof(reresults[RESULTIDX].out));
        }
    }
}

// Pattern matching threads.
static void *thr_fn(void *arg)
{
    regex_t     regex;
    regmatch_t  pmatch[1];
    regoff_t    off, len;
    long idx;
    int i;

    idx = (long)arg;

    for (i=0; i < relutsize; i++) {

        // only check every nthr entry in LUT
        if (idx == (i+1)%MAXNUMTHR) {

            if (regcomp(&regex, re_cb_lut_ptr[i].pattern, REG_NEWLINE))
                exit(EXIT_FAILURE);

            if (regexec(&regex, in, ARRAY_SIZE(pmatch), pmatch, 0) == RE_MATCH) {
                off = pmatch[0].rm_so;
                len = pmatch[0].rm_eo - pmatch[0].rm_so;
                reresults[idx].indexlut = i;
                reresults[idx].idx = idx;
                reresults[idx].tid = reresults[i].tid;
                reresults[idx].pmatch[0].rm_so = pmatch[0].rm_so;
                reresults[idx].pmatch[0].rm_eo = pmatch[0].rm_eo;
                reresults[idx].cb = re_cb_lut_ptr[i].cb;
                reresults[idx].len = len;
                reresults[idx].tid = pthread_self();
                memcpy(&reresults[idx].out[0], &in[off], len);
            }

            // cleanup
            regfree(&regex);
        }
    }

    // wait until all threads have finished
    pthread_barrier_wait(&b);

    return((void *)0);
}

int dap_pattern_find(char *s, const struct DAP_PATTERN_CB *ptnlut, int len, struct DAP_REGEX_RESULTS *rt) {

    long i;
    int err;
    int ret = DAP_SUCCESS;

    if ((s[0] == 0) || (ptnlut == NULL) || (len == 0)) {
        return DAP_PATTERN_FIND_ERROR;
    }

    // store input args to private vars so threads can access
    re_cb_lut_ptr = (struct DAP_PATTERN_CB*)ptnlut;
    relutsize = len;

    // clear result structures
    clearresults();

    // get data packet to process
    strncpy(in, s, sizeof(in));

    // create thread barrier
    pthread_barrier_init(&b, NULL, MAXRESULTTBOXES);

    // create nthr number of threads to search table
    for (i = 0; i < MAXNUMTHR; i++) {
        err = pthread_create(&tid, NULL, thr_fn, (void *)i);
        if (err != 0) {
            return DAP_PATTERN_FIND_ERROR;
        }
    }

    // wait until all threads have finished
    pthread_barrier_wait(&b);

    // all threads have finished, get the results
    getresults();

    // copy result
    memcpy(rt, &reresults[RESULTIDX], sizeof(reresults[RESULTIDX]));

    // deinitialize barrier
    pthread_barrier_destroy(&b);

    return ret;
}


