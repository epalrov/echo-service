/*
 * syslog.h - ECHO logger interface
 *
 * Copyright (C) 2013 Paolo Rovelli
 *
 * Author: Paolo Rovelli <paolorovelli@yahoo.it>
 */

#ifndef ECHO_LOG_H
#define ECHO_LOG_H

#include <stdarg.h>
#include <syslog.h>

extern void echo_log_open(int option);
extern void echo_log_level(int level);
extern void echo_log(int priority, const char *format, ...);
extern void echo_log_nothing(int priority, const char *format, ...);
extern void echo_log_close(void);

#define echo_log_emerg(format, ...) \
	echo_log(LOG_EMERG, format, ##__VA_ARGS__)
#define echo_log_alert(format, ...) \
	echo_log(LOG_ALERT, format, ##__VA_ARGS__)
#define echo_log_crit(format, ...) \
	echo_log(LOG_CRIT, format, ##__VA_ARGS__)
#define echo_log_err(format, ...) \
	echo_log(LOG_ERR, format, ##__VA_ARGS__)
#define echo_log_warning(format, ...) \
	echo_log(LOG_WARNING, format, ##__VA_ARGS__)
#define echo_log_warn echo_log_warning
#define echo_log_notice(format, ...) \
	echo_log(LOG_NOTICE, format, ##__VA_ARGS__)
#define echo_log_info(format, ...) \
	echo_log(LOG_INFO, format, ##__VA_ARGS__)

/* echo_log_debug() should produce zero code unless DEBUG is defined */
#ifdef DEBUG
#define echo_log_debug(format, ...) \
	echo_log(LOG_DEBUG, format, ##__VA_ARGS__)
#else
#define echo_log_debug(format, ...) \
	echo_log_nothing(LOG_DEBUG, format, ##__VA_ARGS__)
#endif

#endif /* ECHO_LOG_H */
