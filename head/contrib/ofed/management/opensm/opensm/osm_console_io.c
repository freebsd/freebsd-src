/*
 * Copyright (c) 2005-2007 Voltaire, Inc. All rights reserved.
 * Copyright (c) 2008 HNR Consulting. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

/*
 * Abstract:
 *    Provide a framework for the Console which decouples the connection
 *    or I/O from the functionality, or commands.
 *
 *    Extensible - allows a variety of connection methods independent of
 *    the console commands.
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif				/* HAVE_CONFIG_H */

#define _GNU_SOURCE		/* for getline */
#ifdef ENABLE_OSM_CONSOLE_SOCKET
#include <tcpd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <opensm/osm_console_io.h>

static int is_local(char *str)
{
	// convenience - checks if just stdin/stdout
	if (str)
		return (strcmp(str, OSM_LOCAL_CONSOLE) == 0);
	return 0;
}

static int is_loopback(char *str)
{
	// convenience - checks if socket based connection
	if (str)
		return (strcmp(str, OSM_LOOPBACK_CONSOLE) == 0);
	return 0;
}

static int is_remote(char *str)
{
	// convenience - checks if socket based connection
	if (str)
		return (strcmp(str, OSM_REMOTE_CONSOLE) == 0)
		    || is_loopback(str);
	return 0;
}

int is_console_enabled(osm_subn_opt_t * p_opt)
{
	// checks for a variety of types of consoles - default is off or 0
	if (p_opt)
		return (is_local(p_opt->console)
			|| is_loopback(p_opt->console)
			|| is_remote(p_opt->console));
	return 0;
}


#ifdef ENABLE_OSM_CONSOLE_SOCKET
static int cio_close(osm_console_t * p_oct)
{
	int rtnval = -1;
	if (p_oct && (p_oct->in_fd > 0)) {
		rtnval = close(p_oct->in_fd);
		p_oct->in_fd = -1;
		p_oct->out_fd = -1;
		p_oct->in = NULL;
		p_oct->out = NULL;
	}
	return rtnval;
}
#endif

/* close the connection */
static void osm_console_close(osm_console_t * p_oct, osm_log_t * p_log)
{
#ifdef ENABLE_OSM_CONSOLE_SOCKET
	if ((p_oct->socket > 0) && (p_oct->in_fd != -1)) {
		OSM_LOG(p_log, OSM_LOG_INFO,
			"Console connection closed: %s (%s)\n",
			p_oct->client_hn, p_oct->client_ip);
		cio_close(p_oct);
	}
	if (p_oct->socket > 0) {
		close(p_oct->socket);
		p_oct->socket = -1;
	}
#endif
}


/**********************************************************************
 * Do authentication & authorization check
 **********************************************************************/
#ifdef ENABLE_OSM_CONSOLE_SOCKET
int is_authorized(osm_console_t * p_oct)
{
	/* allowed to use the console? */
	p_oct->authorized = !is_remote(p_oct->client_type) ||
	    hosts_ctl(OSM_DAEMON_NAME, p_oct->client_hn, p_oct->client_ip,
		      "STRING_UNKNOWN");
	return p_oct->authorized;
}
#endif

void osm_console_prompt(FILE * out)
{
	if (out) {
		fprintf(out, "OpenSM %s", OSM_COMMAND_PROMPT);
		fflush(out);
	}
}

int osm_console_init(osm_subn_opt_t * opt, osm_console_t * p_oct, osm_log_t * p_log)
{
	p_oct->socket = -1;
	strncpy(p_oct->client_type, opt->console, sizeof(p_oct->client_type));

	/* set up the file descriptors for the console */
	if (strcmp(opt->console, OSM_LOCAL_CONSOLE) == 0) {
		p_oct->in = stdin;
		p_oct->out = stdout;
		p_oct->in_fd = fileno(stdin);
		p_oct->out_fd = fileno(stdout);

		osm_console_prompt(p_oct->out);
#ifdef ENABLE_OSM_CONSOLE_SOCKET
	} else if (strcmp(opt->console, OSM_REMOTE_CONSOLE) == 0
		   || strcmp(opt->console, OSM_LOOPBACK_CONSOLE) == 0) {
		struct sockaddr_in sin;
		int optval = 1;

		if ((p_oct->socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
			OSM_LOG(p_log, OSM_LOG_ERROR,
				"ERR 4B01: Failed to open console socket: %s\n",
				strerror(errno));
			return -1;
		}
		setsockopt(p_oct->socket, SOL_SOCKET, SO_REUSEADDR,
			   &optval, sizeof(optval));
		sin.sin_family = AF_INET;
		sin.sin_port = htons(opt->console_port);
		if (strcmp(opt->console, OSM_REMOTE_CONSOLE) == 0)
			sin.sin_addr.s_addr = htonl(INADDR_ANY);
		else
			sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		if (bind(p_oct->socket, &sin, sizeof(sin)) < 0) {
			OSM_LOG(p_log, OSM_LOG_ERROR,
				"ERR 4B02: Failed to bind console socket: %s\n",
				strerror(errno));
			return -1;
		}
		if (listen(p_oct->socket, 1) < 0) {
			OSM_LOG(p_log, OSM_LOG_ERROR,
				"ERR 4B03: Failed to listen on socket: %s\n",
				strerror(errno));
			return -1;
		}

		signal(SIGPIPE, SIG_IGN);	/* protect ourselves from closed pipes */
		p_oct->in = NULL;
		p_oct->out = NULL;
		p_oct->in_fd = -1;
		p_oct->out_fd = -1;
		OSM_LOG(p_log, OSM_LOG_INFO,
			"Console listening on port %d\n", opt->console_port);
#endif
	}

	return 0;
}

/* clean up and release resources */
void osm_console_exit(osm_console_t * p_oct, osm_log_t * p_log)
{
	// clean up and release resources, currently just close the socket
	osm_console_close(p_oct, p_log);
}

#ifdef ENABLE_OSM_CONSOLE_SOCKET
int cio_open(osm_console_t * p_oct, int new_fd, osm_log_t * p_log)
{
	// returns zero if opened fine, -1 otherwise
	char *p_line;
	size_t len;
	ssize_t n;

	if (p_oct->in_fd >= 0) {
		FILE *file = fdopen(new_fd, "w+");

		fprintf(file, "OpenSM Console connection already in use\n"
			"   kill other session (y/n)? ");
		fflush(file);
		p_line = NULL;
		n = getline(&p_line, &len, file);
		if (n > 0 && (p_line[0] == 'y' || p_line[0] == 'Y')) {
			osm_console_close(p_oct, p_log);
		} else {
			OSM_LOG(p_log, OSM_LOG_INFO,
				"Console connection aborted: %s (%s)\n",
				p_oct->client_hn, p_oct->client_ip);
			close(new_fd);
			return -1;
		}
	}
	p_oct->in_fd = new_fd;
	p_oct->out_fd = p_oct->in_fd;
	p_oct->in = fdopen(p_oct->in_fd, "w+");
	p_oct->out = p_oct->in;
	osm_console_prompt(p_oct->out);
	OSM_LOG(p_log, OSM_LOG_INFO,
		"Console connection accepted: %s (%s)\n",
		p_oct->client_hn, p_oct->client_ip);

	return (p_oct->in == NULL) ? -1 : 0;
}
#endif
