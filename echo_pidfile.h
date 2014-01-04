/*
 * echo_pidfile.h - ECHO pidfile interface
 *
 * Copyright (C) 2013 Paolo Rovelli
 *
 * Author: Paolo Rovelli <paolor.rovelli@ericsson.com>
 */

#ifndef ECHO_PIDEFILE_H
#define ECHO_PIDEFILE_H

extern int echo_pidfile_create(char *pathname);
extern int echo_pidfile_delete(char *pathname);
extern int echo_pidfile_check(char *pathname);

#endif /*  ECHO_PIDEFILE_H */
