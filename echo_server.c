/*
 * echo_server.c - ECHO server application
 *
 * Copyright (C) 2013 Paolo Rovelli
 *
 * Author: Paolo Rovelli <paolorovelli@yahoo.it>
 */

#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <syslog.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include "echo_log.h"
#include "echo_pidfile.h"

#define ECHO_SERVICE "7"
#define ECHO_PIDFILE "/var/run/echo.pid"

#define ECHO_BUFSIZE 1024

struct echo_server_data {
	int cfd;
	int lfd;
	struct addrinfo *addr;
	char hbuf[NI_MAXHOST];
	char sbuf[NI_MAXSERV];
	int debug;
};

static int echo_session(struct echo_server_data *data)
{
	char *buf;
	ssize_t count_r, count_w;
	
	buf = malloc(ECHO_BUFSIZE);
	if (!buf) {
		echo_log(LOG_ERR, "failure in malloc(): %s\n",
			strerror(errno));
		goto out1;
	}

	while (1) {
		/* read a chunk from the incoming stream */
		count_r = read(data->cfd, buf, ECHO_BUFSIZE);
		if (count_r > 0) {
			echo_log(LOG_DEBUG, "recv %d bytes from '%s:%s'\n",
				count_r, data->hbuf, data->sbuf);
			
			/* write back the incoming stream */
			while (count_r > 0) {
				count_w = write(data->cfd, buf, count_r);
				if (count_w > 0) {
					echo_log(LOG_DEBUG, "sent %d bytes to "
						"'%s:%s'\n", count_w,
						data->hbuf, data->sbuf);
					count_r -= count_w;
				} else {
					if (count_w < 0 && errno == EINTR)
						continue; /* interrupted */
					echo_log(LOG_DEBUG, "sent failure to "
						"'%s:%s'\n",
						data->hbuf, data->sbuf);
					goto out2;
				}
			}
		} else if (count_r == 0) {
			echo_log(LOG_DEBUG, "recv 'eof' from '%s:%s'\n",
				data->hbuf, data->sbuf);
			goto out2;
		} else {
			if (errno == EINTR)
				continue; /* interrupted */
			echo_log(LOG_DEBUG, "recv failure from '%s:%s'\n",
				data->hbuf, data->sbuf);
			goto out2;
		}
	}

out2:
	free(buf);
out1:
	return -1;
}

static void echo_sigchld_handler(int signal)
{
	pid_t pid;
	int status;

	/* handle exit of more than one child */
	do {
		pid = waitpid(-1, &status, WNOHANG);
	} while (pid > 0);
}

static void echo_sigterm_handler(int signal)
{
	echo_pidfile_delete(ECHO_PIDFILE);
	exit(EXIT_SUCCESS);
}

static void echo_sigpipe_handler(int signal)
{
	/* catch events when remote peers fail */
}

int echo_server_init(struct echo_server_data *data)
{
	int err, sock_opt;
	struct addrinfo *addr;

	echo_log(LOG_DEBUG, "starting ECHO server ...\n");

	/* try to bind the ECHO server to any address */
	for (addr = data->addr; addr != NULL; addr = addr->ai_next) {

		/* create listening socket */
		data->lfd = socket(addr->ai_family, addr->ai_socktype,
			addr->ai_protocol);
		if (data->lfd == -1)
			continue;

		/* allow address reseuse */
		sock_opt = 1;
		setsockopt(data->lfd, SOL_SOCKET, SO_REUSEADDR,
			(void *)&sock_opt, sizeof(sock_opt));

		/* try to bind to the retrieved address */
		err = bind(data->lfd, addr->ai_addr, addr->ai_addrlen);
		if (err)
			close(data->lfd);
		else
			break;
	}

	/* check if any address succeded */
	if (!addr || err) {
		echo_log(LOG_ERR, "can't bind ECHO server to any address\n");
		goto out;
	}

	/* put the ECHO server listening */
	if (listen(data->lfd, 0) < 0) {
		echo_log(LOG_ERR, "failure in listen(): %s\n",
			strerror(errno));
		goto err;
	}

	/* concurrent ECHO server */
	while (1) {

		socklen_t cl_addrlen;
		struct sockaddr_storage cl_addr;

		/* accept connections from ECHO clients */
		cl_addrlen = sizeof(cl_addr);
		data->cfd = accept(data->lfd, (struct sockaddr *)&cl_addr,
			&cl_addrlen);
		if (data->cfd < 0) {
			echo_log(LOG_ERR, "failure in accept(): %s\n",
				strerror(errno));
			continue;
		}
		err = getnameinfo((struct sockaddr *) &cl_addr, cl_addrlen,
			data->hbuf, sizeof(data->hbuf),
			data->sbuf, sizeof(data->sbuf),
			NI_NUMERICHOST | NI_NUMERICSERV);
		if (!err)
			echo_log(LOG_DEBUG, "accepted ECHO connection from "
				"peer '%s:%s'\n", data->hbuf, data->sbuf);
		else
			echo_log(LOG_DEBUG, "accepted ECHO connection from "
				"peer 'unknown'\n");

		/* create a new process to handle each session */
		switch (fork()) {
		case -1: /* error */
			echo_log(LOG_ERR, "failure in fork(): %s\n",
				strerror(errno));

			/* close the connection socket */
			close(data->cfd);
			break;

		case 0: /* child */
			echo_log(LOG_DEBUG, "starting ECHO session with peer "
				"'%s:%s'\n", data->hbuf, data->sbuf);

			/* close the listening socket */
			close(data->lfd);

			/* register the ECHO session signal handlers */
			signal(SIGINT, SIG_DFL);
			signal(SIGTERM, SIG_DFL);
			signal(SIGCHLD, SIG_DFL);
			signal(SIGPIPE, echo_sigpipe_handler);

			/* handle a new ECHO session */
			err = echo_session(data);
			
			echo_log(LOG_DEBUG, "closing ECHO session with peer "
				"'%s:%s'\n", data->hbuf, data->sbuf);

			/* close the connection socket and exit */
			close(data->cfd);
			exit(err ? EXIT_FAILURE : EXIT_SUCCESS);
			break;

		default: /* parent */
			close(data->cfd);
			continue;
		}
	}

	return;

err:
	echo_log(LOG_DEBUG, "closing ECHO server ...\n");
	close(data->lfd);
out:
	return err;
}

static void echo_server_usage(FILE * out)
{
	static const char usage_str[] =
		("Usage:                                                 \n"
		"  echo-server [options]                               \n\n"
		"Options:                                                \n"
		"  -a | --address    echo server address                 \n"
		"  -p | --port       echo server port                    \n"
		"  -d | --debug      run the program in debug mode       \n"
		"  -v | --version    show the program version and exit   \n"
		"  -h | --help       show this help and exit           \n\n"
		"Examples:                                               \n"
		"  echo-server                                         \n\n"
		"  echo-server -d --address localhost -port 7777       \n\n");

	fprintf(out, "%s", usage_str);
	fflush(out);
	return;
}

static void echo_server_version(FILE * out)
{
	static const char prog_str[] = "echo server";
	static const char ver_str[] = "1.0";
	static const char author_str[] = "Paolo Rovelli";

	fprintf(out, "%s %s written by %s\n", prog_str, ver_str, author_str);
	fflush(out);
	return;
}

static const struct option echo_server_options[] = {
	{"addr", required_argument, NULL, 'a'},
	{"port", required_argument, NULL, 'p'},
	{"debug", no_argument, NULL, 'd'},
	{"version", no_argument, NULL, 'v'},
	{"help", no_argument, NULL, 'h'},
	{NULL, 0, NULL, 0}
};

int main(int argc, char *argv[])
{
	int err, opt;
	int debug = 0;
	int ppid = getpid();
	char *port = ECHO_SERVICE;
	char *addr = NULL;
	struct addrinfo hints;
	struct echo_server_data *data;

	/* parse ECHO server command line options */
	while ((opt = getopt_long(argc, argv, "da:p:vh", echo_server_options,
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
			echo_server_version(stdout);
			exit(EXIT_SUCCESS);
		case 'h':
			echo_server_usage(stdout);
			exit(EXIT_SUCCESS);
		default:
			echo_server_usage(stderr);
			exit(EXIT_FAILURE);
		}
	}

	/* init ECHO logger */
	echo_log_open(debug ? LOG_PERROR : LOG_PID);
	echo_log_level(debug ? LOG_DEBUG : LOG_ERR);

	/* allocate ECHO server data */
	data = calloc(sizeof(*data), 1);
	if (!data) {
		err = errno;
		echo_log(LOG_ERR, "failure in calloc(): %s\n",
			strerror(err));
		goto out1;
	}
	data->debug = debug;

	/* obtain address(es) structure matching service */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;
	hints.ai_flags = AI_PASSIVE | AI_NUMERICSERV;
	err = getaddrinfo(addr, port, &hints, &data->addr);
	if (err || !data->addr) {
		echo_log(LOG_ERR, "failure in getaddrinfo(): %s\n",
			gai_strerror(err));
		goto out2;
	}

	/* daemonize */
	if (!data->debug) {

		/* pidfile already exists ? exit : fork */
		if (echo_pidfile_check(ECHO_PIDFILE)) {
			exit(EXIT_FAILURE);
		} else {
			/* become background process */
			switch (fork()) {
			case 0: /* child */
				break;
			case -1: /* error */
				exit(EXIT_FAILURE);
			default: /* parent */
				exit(EXIT_SUCCESS);
			}

			/* create a new session ID */
			if (setsid() < 0)
				exit(EXIT_FAILURE);

			/* change the file mode mask */
			umask(0);

			/* change the current working directory */
			if ((chdir("/")) < 0)
				exit(EXIT_FAILURE);

			/* close the standard file descriptors */
			close(STDIN_FILENO);
			close(STDOUT_FILENO);
			close(STDERR_FILENO);
		}
	}

	/* pidfile already exists ? exit : create */
	if (echo_pidfile_check(ECHO_PIDFILE)) {
		if (getpid() != ppid)
			kill (ppid, SIGTERM);
		exit(EXIT_FAILURE);
	} else {
		if (!echo_pidfile_create(ECHO_PIDFILE)) {
			/* can't create pidfile (fatal error, exit) */
			echo_log(LOG_ERR, "can't create ECHO pidfile: %s\n",
				strerror(errno));
			if (getpid() != ppid)
				kill (ppid, SIGTERM);
			exit(EXIT_FAILURE);
		}
	}

	/* register ECHO server signal handlers */
	signal(SIGINT, echo_sigterm_handler);
	signal(SIGTERM, echo_sigterm_handler);
	signal(SIGCHLD, echo_sigchld_handler);

	/* init ECHO server */
	err = echo_server_init(data);

	/* free ECHO server resources */
	freeaddrinfo(data->addr);
out2:
	free(data);
out1:
	echo_log_close();
out:
	exit(err ? EXIT_FAILURE : EXIT_SUCCESS);
}

