// dap_pattern_queue.c
// queue functions

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


void dap_pattern_queue_init(struct DAP_PATTERN_QUEUE *q) {
    q->front = 0;
    q->rear = -1;
    q->qcount = 0;
}

void dap_pattern_queue_peek(struct DAP_PATTERN_QUEUE *q, struct DAP_REGEX_RESULTS *data) {
    memcpy(data, &(q->rq[q->front]), sizeof(q->rq[q->front]));
}

bool dap_pattern_queue_is_empty(struct DAP_PATTERN_QUEUE *q) {
   return (q->qcount == 0);
}

bool dap_pattern_queue_is_full(struct DAP_PATTERN_QUEUE *q) {
   return (q->qcount == MAX_PATTERN_Q_SIZE);
}

int dap_pattern_queue_size(struct DAP_PATTERN_QUEUE *q) {
   return (q->qcount);
}

void dap_pattern_queue_insert(struct DAP_PATTERN_QUEUE *q, struct DAP_REGEX_RESULTS *data) {

   if(!dap_pattern_queue_is_full(q)) {

      if(q->rear == MAX_PATTERN_Q_SIZE-1) {
         q->rear = -1;
      }

      q->rear++;
      memcpy(&(q->rq[q->rear]), data, sizeof(q->rq[q->rear]));
      q->qcount++;
   }
}

void dap_pattern_queue_remove(struct DAP_PATTERN_QUEUE *q, struct DAP_REGEX_RESULTS *data) {

    memcpy(data, &(q->rq[q->front]), sizeof(q->rq[q->front]));
    q->front++;

    if(q->front == MAX_PATTERN_Q_SIZE) {
        q->front = 0;
    }

    q->qcount--;
}
