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


// clear results array
static void clearresults(struct DAP_PATTERN_DATA *pd) {
    memset(&pd->rer, 0x0, sizeof(struct DAP_REGEX_RESULTS));
}

// Pattern matching threads.
static int dap_scan_cb_lut(struct DAP_PATTERN_DATA *pd)
{
    int i;
    int r;
    int result;
    char serr[100];

    result = DAP_RE_NO_MATCH;
    for (i = 0; i < pd->relutsize; i++) {

        r = regcomp(&pd->regex, pd->re_cb_lut_ptr[i].pattern, REG_NEWLINE);
        if (r != 0) {
            regerror(r, &pd->regex, serr, 100);
            fprintf(stderr, "DAP PATTERN: regcomp error, regerror(%s)\n", serr);
            return DAP_ERROR;
        }

        r = regexec(&pd->regex, pd->rer.in, ARRAY_SIZE(pd->rer.pmatch), pd->rer.pmatch, 0);
        if (r == 0) {
            pd->off = pd->rer.pmatch[0].rm_so;
            pd->rer.len = pd->rer.pmatch[0].rm_eo - pd->rer.pmatch[0].rm_so;
            pd->rer.indexlut = i;
            pd->rer.cb = pd->re_cb_lut_ptr[i].cb;
            memcpy(&pd->rer.out, &pd->rer.in[pd->off], pd->rer.len);
            result = DAP_RE_MATCH;
            break;
        }
    }

    regfree(&pd->regex);
    return result;
}


int dap_pattern_set(const struct DAP_PATTERN_CB *cblut, int lutsize, struct DAP_PATTERN_DATA *pd) {

    ASSERT((cblut != NULL), "null pointer to call back lut", DAP_PATTERN_FIND_ERROR)
    ASSERT((pd != NULL), "null pointer to pattern data struct", DAP_PATTERN_FIND_ERROR)

    pd->re_cb_lut_ptr = (struct DAP_PATTERN_CB *)cblut;
    pd->relutsize = lutsize;
    return DAP_SUCCESS;
}


int dap_pattern_get(struct DAP_PATTERN_DATA *pd, struct DAP_REGEX_RESULTS *rt) {

    ASSERT((rt != NULL), "null pointer to results struct", DAP_PATTERN_FIND_ERROR)
    ASSERT((pd != NULL), "null pointer to pattern data struct", DAP_PATTERN_FIND_ERROR)

    memcpy(rt, &pd->rer, sizeof(struct DAP_REGEX_RESULTS));
    return DAP_SUCCESS;
}


int dap_pattern_find(char *s, int lin, struct DAP_PATTERN_DATA *pd) {

    int ret;

    ASSERT((s[0] != 0), "DAP PARSE: input string is not initialized", DAP_ERROR)
    ASSERT((lin > 0), "DAP PARSE: zero length input string", DAP_ERROR)
    ASSERT((pd != NULL), "DAP PARSE: null pointer to pattern data struct", DAP_PATTERN_FIND_ERROR)

    // store input args to private vars so threads can access
    //pfv->re_cb_lut_ptr = (struct DAP_PATTERN_CB*)ptnlut;
    pd->lin = lin;

    // clear result structures
    clearresults(pd);

    // get data packet to process
    strncpy(pd->rer.in, s, lin);

    // search callback lut for regular expression match
    ret = dap_scan_cb_lut(pd);

    // returns DAP_RE_MATCH, DAP_RE_NO_MATCH, DAP_ERROR
    return ret;
}


