/*
 * echo_client.c - ECHO client application
 *
 * Copyright (C) 2013 Ericsson AB
 *
 * Author: Paolo Rovelli <paolo.rovelli@ericsson.com>
 */

#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>

#include "echo_log.h"

#define ECHO_SERVICE "7"
#define ECHO_HOSTNAME "localhost"

struct echo_client_data {
	int fd;
	struct addrinfo *addr;
	int debug;
};

/*
 * echo_client_send - ECHO client send
 */
int echo_client_send(struct echo_client_data *data, const void *buf, size_t count)
{
	int n, ret;
	const char *ptr;

	ptr = buf;
	for (ret = 0; ret < count; ) {
		n = write(data->fd, ptr, count - ret);
		if (n <= 0) {
			if (n == -1 && errno == EINTR)
				continue; /* interrupted, restart write() */
			else
				return -1; /* some other error */
		}
		ret += n;
		ptr += n;
	}
	return ret;
}

/*
 * echo_client_session - ECHO client dummy session
 */
static int echo_client_session(struct echo_client_data *data)
{
	echo_client_send(data, "Paolo", 5);
	echo_client_send(data, " ", 1);
	echo_client_send(data, "Rovelli", 7);
	echo_client_send(data, "\n", 1);

	// sleep(1);	

	return 0;
}

/*
 * echo_client_init - ECHO client initialization
 */
int echo_client_init(struct echo_client_data *data)
{
	int err;
	struct addrinfo *addr;

	echo_log(LOG_DEBUG, "starting ECHO client ...\n");

	/* try to connect to the ECHO server address(es) */
	for (addr = data->addr; addr != NULL; addr = addr->ai_next) {
		/* create connecting socket */
		data->fd = socket(addr->ai_family, addr->ai_socktype,
			addr->ai_protocol);
		if (data->fd == -1)
			continue;

		/* try to connect */
		err = connect(data->fd, addr->ai_addr, addr->ai_addrlen);
		if (err)
			close(data->fd);
		else
			break;
	}

	/* check if any address succeded */
	if (!addr || err) {
		echo_log(LOG_ERR, "can't connect to ECHO server\n");
		goto out1;
	}

	/* dummy session */
	err = echo_client_session(data);

out2:
	echo_log(LOG_DEBUG, "closing ECHO client ...\n");
	close(data->fd);
out1:
	return err;
}

static void echo_client_usage(FILE * out)
{
	static const char usage_str[] =
		("Usage:                                                \n"
		"  echo client [options]                               \n\n"
		"Options:                                               \n"
		"  -a | --address    echo server address/hostname        \n"
		"  -p | --port       echo server port/service            \n"
		"  -d | --debug      echo client debug mode              \n"
		"  -v | --version    show the program version and exit  \n"
		"  -h | --help       show this help and exit          \n\n"
		"Examples:                                              \n"
		"  echo client -d --address localhost --port 4189      \n\n");

	fprintf(out, "%s", usage_str);
	fflush(out);
	return;
}

static void echo_client_version(FILE * out)
{
	static const char prog_str[] = "echo client";
	static const char ver_str[] = "1.0";
	static const char author_str[] = "Paolo Rovelli";

	fprintf(out, "%s %s written by %s\n", prog_str, ver_str, author_str);
	fflush(out);
	return;
}

static const struct option echo_client_options[] = {
	{"addr", required_argument, NULL, 'a'},
	{"port", required_argument, NULL, 'p'},
	{"debug", no_argument, NULL, 'd'},
	{"version", no_argument, NULL, 'v'},
	{"help", no_argument, NULL, 'h'},
	{NULL, 0, NULL, 0}
};

int echo_client_main(int argc, char *argv[])
{
	int err, opt;
	int debug = 0;
	char *port = ECHO_SERVICE;
	char *addr = ECHO_HOSTNAME;
	struct addrinfo hints;
	struct echo_client_data *data;

	/* parse ECHO client command line options */
	while ((opt = getopt_long(argc, argv, "da:p:vh", echo_client_options,
				NULL)) != -1) {
		switch (opt) {
		case 'd':
			debug = 1;
			break;
		case 'a':
			addr = optarg;
			break;
		case 'p':
			port = optarg;
			break;
		case 'v':
			echo_client_version(stdout);
			exit(EXIT_SUCCESS);
		case 'h':
			echo_client_usage(stdout);
			exit(EXIT_SUCCESS);
		default:
			echo_client_usage(stderr);
			exit(EXIT_FAILURE);
		}
	}

	/* init ECHO logger */
	echo_log_open(debug ? LOG_PERROR : LOG_PID);
	echo_log_level(debug ? LOG_DEBUG : LOG_ERR);

	/* allocate ECHO client data */
	data = calloc(sizeof(*data), 1);
	if (!data) {
		err = errno;
		echo_log(LOG_ERR, "failure in calloc(): %s\n",
			strerror(err));
		goto out1;
	}
	data->debug = debug;

	/* obtain address(es) structure matching host/service */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;
	hints.ai_flags = 0;
	err = getaddrinfo(addr, port, &hints, &data->addr);
	if (err) {
		echo_log(LOG_ERR, "failure in getaddrinfo(): %s\n",
			gai_strerror(err));
		goto out2;
	}

	/* init ECHO client */
	err = echo_client_init(data);

	/* free ECHO client resources */
	freeaddrinfo(data->addr);
out2:
	free(data);
out1:
	echo_log_close();
out:
	exit(err ? EXIT_FAILURE : EXIT_SUCCESS);
}
