// dap_elapsed_time.c
// function that can be used to meaure elapsed time

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

// elapse time function
// returns elaped time from START to END in usec.
long long elapsed_time(enum ELTIME sts, struct timeval *start, struct timeval *end)
{
	long long startusec = 0, endusec = 0;
	long long elapsed = 0;

	if (sts == START)
	{
		gettimeofday(start, NULL);
	}
	else
	{
		gettimeofday(end, NULL);
		startusec = start->tv_sec * 1000000 + start->tv_usec;
		endusec = end->tv_sec * 1000000 + end->tv_usec;
		elapsed = endusec - startusec;				// usec
	}
	return elapsed;
}
