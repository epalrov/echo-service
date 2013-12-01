/*
 * echo.c - ECHO main application
 *
 * Copyright (C) 2013 Ericsson AB
 *
 * Author: Paolo Rovelli <paolo.rovelli@ericsson.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int echo_server_main(int argc, char **argv);
extern int echo_client_main(int argc, char **argv);

static void echo_usage(FILE * out)
{
	static const char usage_str[] =
		("Usage:                              \n"
		"  echo [command] [options]         \n\n"
		"Commands:                            \n"
		"  server    run echo server          \n"
		"  client    run echo client          \n"
		"  help      show this help and exit\n\n"
		"Examples:                            \n"
		"  echo server -d -p 7777             \n"
		"  echo client                      \n\n");

	fprintf(out, "%s", usage_str);
	fflush(out);

	return;
}

int main(int argc, char *argv[])
{
	if (argc > 1 && (strcmp(argv[1], "server") == 0)) {
		argc--;
		argv++;
		echo_server_main(argc, argv);
	} else if (argc > 1 && (strcmp(argv[1], "client") == 0)) {
		argc--;
		argv++;
		echo_client_main(argc, argv);
	} else if (argc > 1 && (strcmp(argv[1], "help") == 0)) {
		echo_usage(stderr);
		exit(EXIT_FAILURE);
	} else {
		echo_usage(stderr);
		exit(EXIT_FAILURE);
	}

	return 0;
}
