/*
 * echo_log.c - ECHO logger
 *
 * Copyright (C) 2013 Ericsson AB
 *
 * Author: Paolo Rovelli <paolo.rovelli@ericsson.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>

#include "echo_log.h"

static int echo_log_stderr = 0;
static int echo_log_priority = LOG_WARNING;

void echo_log_open(int option)
{
	echo_log_stderr = option & LOG_PERROR;
	if (!echo_log_stderr)
		openlog("ECHO", option, LOG_USER);
}

void echo_log(int priority, const char *format, ...)
{
	va_list args;

	if (priority > echo_log_priority)
		return;

	va_start(args, format);
	if (echo_log_stderr)
		vfprintf(stderr, format, args);
	else
		vsyslog(priority, format, args);
	va_end(args);
}

void echo_log_nothing(int priority, const char *format, ...)
{
	return;
}

void echo_log_level(int level)
{
	echo_log_priority = level;
}

void echo_log_close(void)
{
	if (!echo_log_stderr)
		closelog();
}
