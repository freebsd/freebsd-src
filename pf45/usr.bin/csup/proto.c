/*-
 * Copyright (c) 2003-2006, Maxime Henrion <mux@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "auth.h"
#include "config.h"
#include "detailer.h"
#include "fattr.h"
#include "fixups.h"
#include "globtree.h"
#include "keyword.h"
#include "lister.h"
#include "misc.h"
#include "mux.h"
#include "proto.h"
#include "queue.h"
#include "stream.h"
#include "threads.h"
#include "updater.h"

struct killer {
	pthread_t thread;
	sigset_t sigset;
	struct mux *mux;
	int killedby;
};

static void		 killer_start(struct killer *, struct mux *);
static void		*killer_run(void *);
static void		 killer_stop(struct killer *);

static int		 proto_waitconnect(int);
static int		 proto_greet(struct config *);
static int		 proto_negproto(struct config *);
static int		 proto_fileattr(struct config *);
static int		 proto_xchgcoll(struct config *);
static struct mux	*proto_mux(struct config *);

static int		 proto_escape(struct stream *, const char *);
static void		 proto_unescape(char *);

static int
proto_waitconnect(int s)
{
	fd_set readfd;
	socklen_t len;
	int error, rv, soerror;

	FD_ZERO(&readfd);
	FD_SET(s, &readfd);

	do {
		rv = select(s + 1, &readfd, NULL, NULL, NULL);
	} while (rv == -1 && errno == EINTR);
	if (rv == -1)
		return (-1);
	/* Check that the connection was really successful. */
	len = sizeof(soerror);
	error = getsockopt(s, SOL_SOCKET, SO_ERROR, &soerror, &len);
	if (error) {
		/* We have no choice but faking an error here. */
		errno = ECONNREFUSED;
		return (-1);
	}
	if (soerror) {
		errno = soerror;
		return (-1);
	}
	return (0);
}

/* Connect to the CVSup server. */
int
proto_connect(struct config *config, int family, uint16_t port)
{
	char addrbuf[NI_MAXHOST];
	/* Enough to hold sizeof("cvsup") or any port number. */
	char servname[8];
	struct addrinfo *res, *ai, hints;
	int error, opt, s;

	s = -1;
	if (port != 0)
		snprintf(servname, sizeof(servname), "%d", port);
	else {
		strncpy(servname, "cvsup", sizeof(servname) - 1);
		servname[sizeof(servname) - 1] = '\0';
	}
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = family;
	hints.ai_socktype = SOCK_STREAM;
	error = getaddrinfo(config->host, servname, &hints, &res);
	/*
	 * Try with the hardcoded port number for OSes that don't
	 * have cvsup defined in the /etc/services file.
	 */
	if (error == EAI_SERVICE) {
		strncpy(servname, "5999", sizeof(servname) - 1);
		servname[sizeof(servname) - 1] = '\0';
		error = getaddrinfo(config->host, servname, &hints, &res);
	}
	if (error) {
		lprintf(0, "Name lookup failure for \"%s\": %s\n", config->host,
		    gai_strerror(error));
		return (STATUS_TRANSIENTFAILURE);
	}
	for (ai = res; ai != NULL; ai = ai->ai_next) {
		s = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (s != -1) {
			error = 0;
			if (config->laddr != NULL) {
				opt = 1;
				(void)setsockopt(s, SOL_SOCKET, SO_REUSEADDR,
				    &opt, sizeof(opt));
				error = bind(s, config->laddr,
				    config->laddrlen);
			}
			if (!error) {
				error = connect(s, ai->ai_addr, ai->ai_addrlen);
				if (error && errno == EINTR)
					error = proto_waitconnect(s);
			}
			if (error)
				close(s);
		}
		(void)getnameinfo(ai->ai_addr, ai->ai_addrlen, addrbuf,
		    sizeof(addrbuf), NULL, 0, NI_NUMERICHOST);
		if (s == -1 || error) {
			lprintf(0, "Cannot connect to %s: %s\n", addrbuf,
			    strerror(errno));
			continue;
		}
		lprintf(1, "Connected to %s\n", addrbuf);
		freeaddrinfo(res);
		config->socket = s;
		return (STATUS_SUCCESS);
	}
	freeaddrinfo(res);
	return (STATUS_TRANSIENTFAILURE);
}

/* Greet the server. */
static int
proto_greet(struct config *config)
{
	char *line, *cmd, *msg, *swver;
	struct stream *s;

	s = config->server;
	line = stream_getln(s, NULL);
	cmd = proto_get_ascii(&line);
	if (cmd == NULL)
		goto bad;
	if (strcmp(cmd, "OK") == 0) {
		(void)proto_get_ascii(&line);	/* major number */
		(void)proto_get_ascii(&line);	/* minor number */
		swver = proto_get_ascii(&line);
	} else if (strcmp(cmd, "!") == 0) {
		msg = proto_get_rest(&line);
		if (msg == NULL)
			goto bad;
		lprintf(-1, "Rejected by server: %s\n", msg);
		return (STATUS_TRANSIENTFAILURE);
	} else
		goto bad;
	lprintf(2, "Server software version: %s\n",
	    swver != NULL ? swver : ".");
	return (STATUS_SUCCESS);
bad:
	lprintf(-1, "Invalid greeting from server\n");
	return (STATUS_FAILURE);
}

/* Negotiate protocol version with the server. */
static int
proto_negproto(struct config *config)
{
	struct stream *s;
	char *cmd, *line, *msg;
	int error, maj, min;

	s = config->server;
	proto_printf(s, "PROTO %d %d %s\n", PROTO_MAJ, PROTO_MIN, PROTO_SWVER);
	stream_flush(s);
	line = stream_getln(s, NULL);
	cmd = proto_get_ascii(&line);
	if (cmd == NULL || line == NULL)
		goto bad;
	if (strcmp(cmd, "!") == 0) {
		msg = proto_get_rest(&line);
		lprintf(-1, "Protocol negotiation failed: %s\n", msg);
		return (1);
	} else if (strcmp(cmd, "PROTO") != 0)
		goto bad;
	error = proto_get_int(&line, &maj, 10);
	if (!error)
		error = proto_get_int(&line, &min, 10);
	if (error)
		goto bad;
	if (maj != PROTO_MAJ || min != PROTO_MIN) {
		lprintf(-1, "Server protocol version %d.%d not supported "
		    "by client\n", maj, min);
		return (STATUS_FAILURE);
	}
	return (STATUS_SUCCESS);
bad:
	lprintf(-1, "Invalid PROTO command from server\n");
	return (STATUS_FAILURE);
}

/*
 * File attribute support negotiation.
 */
static int
proto_fileattr(struct config *config)
{
	fattr_support_t support;
	struct stream *s;
	char *line, *cmd;
	int error, i, n, attr;

	s = config->server;
	lprintf(2, "Negotiating file attribute support\n");
	proto_printf(s, "ATTR %d\n", FT_NUMBER);
	for (i = 0; i < FT_NUMBER; i++)
		proto_printf(s, "%x\n", fattr_supported(i));
	proto_printf(s, ".\n");
	stream_flush(s);
	line = stream_getln(s, NULL);
	if (line == NULL)
		goto bad;
	cmd = proto_get_ascii(&line);
	error = proto_get_int(&line, &n, 10);
	if (error || line != NULL || strcmp(cmd, "ATTR") != 0 || n > FT_NUMBER)
		goto bad;
	for (i = 0; i < n; i++) {
		line = stream_getln(s, NULL);
		if (line == NULL)
			goto bad;
		error = proto_get_int(&line, &attr, 16);
		if (error)
			goto bad;
		support[i] = fattr_supported(i) & attr;
	}
	for (i = n; i < FT_NUMBER; i++)
		support[i] = 0;
	line = stream_getln(s, NULL);
	if (line == NULL || strcmp(line, ".") != 0)
		goto bad;
	memcpy(config->fasupport, support, sizeof(config->fasupport));
	return (STATUS_SUCCESS);
bad:
	lprintf(-1, "Protocol error negotiating attribute support\n");
	return (STATUS_FAILURE);
}

/*
 * Exchange collection information.
 */
static int
proto_xchgcoll(struct config *config)
{
	struct coll *coll;
	struct stream *s;
	struct globtree *diraccept, *dirrefuse;
	struct globtree *fileaccept, *filerefuse;
	char *line, *cmd, *collname, *pat;
	char *msg, *release, *ident, *rcskey, *prefix;
	size_t i, len;
	int error, flags, options;

	s = config->server;
	lprintf(2, "Exchanging collection information\n");
	STAILQ_FOREACH(coll, &config->colls, co_next) {
		if (coll->co_options & CO_SKIP)
			continue;
		proto_printf(s, "COLL %s %s %o %d\n", coll->co_name,
		    coll->co_release, coll->co_umask, coll->co_options);
		for (i = 0; i < pattlist_size(coll->co_accepts); i++) {
		    proto_printf(s, "ACC %s\n",
			pattlist_get(coll->co_accepts, i));
		}
		for (i = 0; i < pattlist_size(coll->co_refusals); i++) {
		    proto_printf(s, "REF %s\n",
			pattlist_get(coll->co_refusals, i));
		}
		proto_printf(s, ".\n");
	}
	proto_printf(s, ".\n");
	stream_flush(s);

	STAILQ_FOREACH(coll, &config->colls, co_next) {
		if (coll->co_options & CO_SKIP)
			continue;
		coll->co_norsync = globtree_false();
		line = stream_getln(s, NULL);
		if (line == NULL)
			goto bad;
		cmd = proto_get_ascii(&line);
		collname = proto_get_ascii(&line);
		release = proto_get_ascii(&line);
		error = proto_get_int(&line, &options, 10);
		if (error || line != NULL)
			goto bad;
		if (strcmp(cmd, "COLL") != 0 ||
		    strcmp(collname, coll->co_name) != 0 ||
		    strcmp(release, coll->co_release) != 0)
			goto bad;
		coll->co_options =
		    (coll->co_options | (options & CO_SERVMAYSET)) &
		    ~(~options & CO_SERVMAYCLEAR);
		while ((line = stream_getln(s, NULL)) != NULL) {
		 	if (strcmp(line, ".") == 0)
				break;
			cmd = proto_get_ascii(&line);
			if (cmd == NULL)
				goto bad;
			if (strcmp(cmd, "!") == 0) {
				msg = proto_get_rest(&line);
				if (msg == NULL)
					goto bad;
				lprintf(-1, "Server message: %s\n", msg);
			} else if (strcmp(cmd, "PRFX") == 0) {
				prefix = proto_get_ascii(&line);
				if (prefix == NULL || line != NULL)
					goto bad;
				coll->co_cvsroot = xstrdup(prefix);
			} else if (strcmp(cmd, "KEYALIAS") == 0) {
				ident = proto_get_ascii(&line);
				rcskey = proto_get_ascii(&line);
				if (rcskey == NULL || line != NULL)
					goto bad;
				error = keyword_alias(coll->co_keyword, ident,
				    rcskey);
				if (error)
					goto bad;
			} else if (strcmp(cmd, "KEYON") == 0) {
				ident = proto_get_ascii(&line);
				if (ident == NULL || line != NULL)
					goto bad;
				error = keyword_enable(coll->co_keyword, ident);
				if (error)
					goto bad;
			} else if (strcmp(cmd, "KEYOFF") == 0) {
				ident = proto_get_ascii(&line);
				if (ident == NULL || line != NULL)
					goto bad;
				error = keyword_disable(coll->co_keyword,
				    ident);
				if (error)
					goto bad;
			} else if (strcmp(cmd, "NORS") == 0) {
				pat = proto_get_ascii(&line);
				if (pat == NULL || line != NULL)
					goto bad;
				coll->co_norsync = globtree_or(coll->co_norsync,
				    globtree_match(pat, FNM_PATHNAME));
			} else if (strcmp(cmd, "RNORS") == 0) {
				pat = proto_get_ascii(&line);
				if (pat == NULL || line != NULL)
					goto bad;
				coll->co_norsync = globtree_or(coll->co_norsync,
				    globtree_match(pat, FNM_PATHNAME |
				    FNM_LEADING_DIR));
			} else
				goto bad;
		}
		if (line == NULL)
			goto bad;
		keyword_prepare(coll->co_keyword);

		diraccept = globtree_true();
		fileaccept = globtree_true();
		dirrefuse = globtree_false();
		filerefuse = globtree_false();

		if (pattlist_size(coll->co_accepts) > 0) {
			globtree_free(diraccept);
			globtree_free(fileaccept);
			diraccept = globtree_false();
			fileaccept = globtree_false();
			flags = FNM_PATHNAME | FNM_LEADING_DIR |
			    FNM_PREFIX_DIRS;
			for (i = 0; i < pattlist_size(coll->co_accepts); i++) {
				pat = pattlist_get(coll->co_accepts, i);
				diraccept = globtree_or(diraccept,
				    globtree_match(pat, flags));

				len = strlen(pat);
				if (coll->co_options & CO_CHECKOUTMODE &&
				    (len == 0 || pat[len - 1] != '*')) {
					/* We must modify the pattern so that it
					   refers to the RCS file, rather than
					   the checked-out file. */
					xasprintf(&pat, "%s,v", pat);
					fileaccept = globtree_or(fileaccept,
					    globtree_match(pat, flags));
					free(pat);
				} else {
					fileaccept = globtree_or(fileaccept,
					    globtree_match(pat, flags));
				}
			}
		}

		for (i = 0; i < pattlist_size(coll->co_refusals); i++) {
			pat = pattlist_get(coll->co_refusals, i);
			dirrefuse = globtree_or(dirrefuse,
			    globtree_match(pat, 0));
			len = strlen(pat);
			if (coll->co_options & CO_CHECKOUTMODE &&
			    (len == 0 || pat[len - 1] != '*')) {
				/* We must modify the pattern so that it refers
				   to the RCS file, rather than the checked-out
				   file. */
				xasprintf(&pat, "%s,v", pat);
				filerefuse = globtree_or(filerefuse,
				    globtree_match(pat, 0));
				free(pat);
			} else {
				filerefuse = globtree_or(filerefuse,
				    globtree_match(pat, 0));
			}
		}

		coll->co_dirfilter = globtree_and(diraccept,
		    globtree_not(dirrefuse));
		coll->co_filefilter = globtree_and(fileaccept,
		    globtree_not(filerefuse));

		/* Set up a mask of file attributes that we don't want to sync
		   with the server. */
		if (!(coll->co_options & CO_SETOWNER))
			coll->co_attrignore |= FA_OWNER | FA_GROUP;
		if (!(coll->co_options & CO_SETMODE))
			coll->co_attrignore |= FA_MODE;
		if (!(coll->co_options & CO_SETFLAGS))
			coll->co_attrignore |= FA_FLAGS;
	}
	return (STATUS_SUCCESS);
bad:
	lprintf(-1, "Protocol error during collection exchange\n");
	return (STATUS_FAILURE);
}

static struct mux *
proto_mux(struct config *config)
{
	struct mux *m;
	struct stream *s, *wr;
	struct chan *chan0, *chan1;
	int id;

	s = config->server;
	lprintf(2, "Establishing multiplexed-mode data connection\n");
	proto_printf(s, "MUX\n");
	stream_flush(s);
	m = mux_open(config->socket, &chan0);
	if (m == NULL) {
		lprintf(-1, "Cannot open the multiplexer\n");
		return (NULL);
	}
	id = chan_listen(m);
	if (id == -1) {
		lprintf(-1, "ChannelMux.Listen failed: %s\n", strerror(errno));
		mux_close(m);
		return (NULL);
	}
	wr = stream_open(chan0, NULL, (stream_writefn_t *)chan_write, NULL);
	proto_printf(wr, "CHAN %d\n", id);
	stream_close(wr);
	chan1 = chan_accept(m, id);
	if (chan1 == NULL) {
		lprintf(-1, "ChannelMux.Accept failed: %s\n", strerror(errno));
		mux_close(m);
		return (NULL);
	}
	config->chan0 = chan0;
	config->chan1 = chan1;
	return (m);
}

/*
 * Initializes the connection to the CVSup server, that is handle
 * the protocol negotiation, logging in, exchanging file attributes
 * support and collections information, and finally run the update
 * session.
 */
int
proto_run(struct config *config)
{
	struct thread_args lister_args;
	struct thread_args detailer_args;
	struct thread_args updater_args;
	struct thread_args *args;
	struct killer killer;
	struct threads *workers;
	struct mux *m;
	int i, status;

	/*
	 * We pass NULL for the close() function because we'll reuse
	 * the socket after the stream is closed.
	 */
	config->server = stream_open_fd(config->socket, stream_read_fd,
	    stream_write_fd, NULL);
	status = proto_greet(config);
	if (status == STATUS_SUCCESS)
		status = proto_negproto(config);
	if (status == STATUS_SUCCESS)
		status = auth_login(config);
	if (status == STATUS_SUCCESS)
		status = proto_fileattr(config);
	if (status == STATUS_SUCCESS)
		status = proto_xchgcoll(config);
	if (status != STATUS_SUCCESS)
		return (status);

	/* Multi-threaded action starts here. */
	m = proto_mux(config);
	if (m == NULL)
		return (STATUS_FAILURE);

	stream_close(config->server);
	config->server = NULL;
	config->fixups = fixups_new();
	killer_start(&killer, m);

	/* Start the worker threads. */
	workers = threads_new();
	args = &lister_args;
	args->config = config;
	args->status = -1;
	args->errmsg = NULL;
	args->rd = NULL;
	args->wr = stream_open(config->chan0,
	    NULL, (stream_writefn_t *)chan_write, NULL);
	threads_create(workers, lister, args);

	args = &detailer_args;
	args->config = config;
	args->status = -1;
	args->errmsg = NULL;
	args->rd = stream_open(config->chan0,
	    (stream_readfn_t *)chan_read, NULL, NULL);
	args->wr = stream_open(config->chan1,
	    NULL, (stream_writefn_t *)chan_write, NULL);
	threads_create(workers, detailer, args);

	args = &updater_args;
	args->config = config;
	args->status = -1;
	args->errmsg = NULL;
	args->rd = stream_open(config->chan1,
	    (stream_readfn_t *)chan_read, NULL, NULL);
	args->wr = NULL;
	threads_create(workers, updater, args);

	lprintf(2, "Running\n");
	/* Wait for all the worker threads to finish. */
	status = STATUS_SUCCESS;
	for (i = 0; i < 3; i++) {
		args = threads_wait(workers);
		if (args->rd != NULL)
			stream_close(args->rd);
		if (args->wr != NULL)
			stream_close(args->wr);
		if (args->status != STATUS_SUCCESS) {
			assert(args->errmsg != NULL);
			if (status == STATUS_SUCCESS) {
				status = args->status;
				/* Shutdown the multiplexer to wake up all
				   the other threads. */
				mux_shutdown(m, args->errmsg, status);
			}
			free(args->errmsg);
		}
	}
	threads_free(workers);
	if (status == STATUS_SUCCESS) {
		lprintf(2, "Shutting down connection to server\n");
		chan_close(config->chan0);
		chan_close(config->chan1);
		chan_wait(config->chan0);
		chan_wait(config->chan1);
		mux_shutdown(m, NULL, STATUS_SUCCESS);
	}
	killer_stop(&killer);
	fixups_free(config->fixups);
	status = mux_close(m);
	if (status == STATUS_SUCCESS) {
		lprintf(1, "Finished successfully\n");
	} else if (status == STATUS_INTERRUPTED) {
		lprintf(-1, "Interrupted\n");
		if (killer.killedby != -1)
			kill(getpid(), killer.killedby);
	}
	return (status);
}

/*
 * Write a string into the stream, escaping characters as needed.
 * Characters escaped:
 *
 * SPACE	-> "\_"
 * TAB		->  "\t"
 * NEWLINE	-> "\n"
 * CR		-> "\r"
 * \		-> "\\"
 */
static int
proto_escape(struct stream *wr, const char *s)
{
	size_t len;
	ssize_t n;
	char c;

	/* Handle characters that need escaping. */
	do {
		len = strcspn(s, " \t\r\n\\");
		n = stream_write(wr, s, len);
		if (n == -1)
			return (-1);
		c = s[len];
		switch (c) {
		case ' ':
			n = stream_write(wr, "\\_", 2);
			break;
		case '\t':
			n = stream_write(wr, "\\t", 2);
			break;
		case '\r':
			n = stream_write(wr, "\\r", 2);
			break;
		case '\n':
			n = stream_write(wr, "\\n", 2);
			break;
		case '\\':
			n = stream_write(wr, "\\\\", 2);
			break;
		}
		if (n == -1)
			return (-1);
		s += len + 1;
	} while (c != '\0');
	return (0);
}

/*
 * A simple printf() implementation specifically tailored for csup.
 * List of the supported formats:
 *
 * %c		Print a char.
 * %d or %i	Print an int as decimal.
 * %x		Print an int as hexadecimal.
 * %o		Print an int as octal.
 * %t		Print a time_t as decimal.
 * %s		Print a char * escaping some characters as needed.
 * %S		Print a char * without escaping.
 * %f		Print an encoded struct fattr *.
 * %F		Print an encoded struct fattr *, specifying the supported
 * 		attributes.
 */
int
proto_printf(struct stream *wr, const char *format, ...)
{
	fattr_support_t *support;
	long long longval;
	struct fattr *fa;
	const char *fmt;
	va_list ap;
	char *cp, *s, *attr;
	ssize_t n;
	size_t size;
	off_t off;
	int rv, val, ignore;
	char c;

	n = 0;
	rv = 0;
	fmt = format;
	va_start(ap, format);
	while ((cp = strchr(fmt, '%')) != NULL) {
		if (cp > fmt) {
			n = stream_write(wr, fmt, cp - fmt);
			if (n == -1)
				return (-1);
		}
		if (*++cp == '\0')
			goto done;
		switch (*cp) {
		case 'c':
			c = va_arg(ap, int);
			rv = stream_printf(wr, "%c", c);
			break;
		case 'd':
		case 'i':
			val = va_arg(ap, int);
			rv = stream_printf(wr, "%d", val);
			break;
		case 'x':
			val = va_arg(ap, int);
			rv = stream_printf(wr, "%x", val);
			break;
		case 'o':
			val = va_arg(ap, int);
			rv = stream_printf(wr, "%o", val);
			break;
		case 'O':
			off = va_arg(ap, off_t);
			rv = stream_printf(wr, "%llu", off);
			break;
		case 'S':
			s = va_arg(ap, char *);
			assert(s != NULL);
			rv = stream_printf(wr, "%s", s);
			break;
		case 's':
			s = va_arg(ap, char *);
			assert(s != NULL);
			rv = proto_escape(wr, s);
			break;
		case 't':
			longval = (long long)va_arg(ap, time_t);
			rv = stream_printf(wr, "%lld", longval);
			break;
		case 'f':
			fa = va_arg(ap, struct fattr *);
			attr = fattr_encode(fa, NULL, 0);
			rv = proto_escape(wr, attr);
			free(attr);
			break;
		case 'F':
			fa = va_arg(ap, struct fattr *);
			support = va_arg(ap, fattr_support_t *);
			ignore = va_arg(ap, int);
			attr = fattr_encode(fa, *support, ignore);
			rv = proto_escape(wr, attr);
			free(attr);
			break;
		case 'z':
			size = va_arg(ap, size_t);
			rv = stream_printf(wr, "%zu", size);
			break;

		case '%':
			n = stream_write(wr, "%", 1);
			if (n == -1)
				return (-1);
			break;
		}
		if (rv == -1)
			return (-1);
		fmt = cp + 1;
	}
	if (*fmt != '\0') {
		rv = stream_printf(wr, "%s", fmt);
		if (rv == -1)
			return (-1);
	}
done:
	va_end(ap);
	return (0);
}

/*
 * Unescape the string, see proto_escape().
 */
static void
proto_unescape(char *s)
{
	char *cp, *cp2;

	cp = s;
	while ((cp = strchr(cp, '\\')) != NULL) {
		switch (cp[1]) {
		case '_':
			*cp = ' ';
			break;
		case 't':
			*cp = '\t';
			break;
		case 'r':
			*cp = '\r';
			break;
		case 'n':
			*cp = '\n';
			break;
		case '\\':
			*cp = '\\';
			break;
		default:
			*cp = *(cp + 1);
		}
		cp2 = ++cp;
		while (*cp2 != '\0') {
			*cp2 = *(cp2 + 1);
			cp2++;
		}
	}
}

/*
 * Get an ascii token in the string.
 */
char *
proto_get_ascii(char **s)
{
	char *ret;

	ret = strsep(s, " ");
	if (ret == NULL)
		return (NULL);
	/* Make sure we disallow 0-length fields. */
	if (*ret == '\0') {
		*s = NULL;
		return (NULL);
	}
	proto_unescape(ret);
	return (ret);
}

/*
 * Get the rest of the string.
 */
char *
proto_get_rest(char **s)
{
	char *ret;

	if (s == NULL)
		return (NULL);
	ret = *s;
	proto_unescape(ret);
	*s = NULL;
	return (ret);
}

/*
 * Get an int token.
 */
int
proto_get_int(char **s, int *val, int base)
{
	char *cp;
	int error;

	cp = proto_get_ascii(s);
	if (cp == NULL)
		return (-1);
	error = asciitoint(cp, val, base);
	return (error);
}

/*
 * Get a size_t token.
 */
int
proto_get_sizet(char **s, size_t *val, int base)
{
	unsigned long long tmp;
	char *cp, *end;

	cp = proto_get_ascii(s);
	if (cp == NULL)
		return (-1);
	errno = 0;
	tmp = strtoll(cp, &end, base);
	if (errno || *end != '\0')
		return (-1);
	*val = (size_t)tmp;
	return (0);
}

/*
 * Get a time_t token.
 *
 * Ideally, we would use an intmax_t and strtoimax() here, but strtoll()
 * is more portable and 64bits should be enough for a timestamp.
 */
int
proto_get_time(char **s, time_t *val)
{
	long long tmp;
	char *cp, *end;

	cp = proto_get_ascii(s);
	if (cp == NULL)
		return (-1);
	errno = 0;
	tmp = strtoll(cp, &end, 10);
	if (errno || *end != '\0')
		return (-1);
	*val = (time_t)tmp;
	return (0);
}

/* Start the killer thread.  It is used to protect against some signals
   during the multi-threaded run so that we can gracefully fail.  */
static void
killer_start(struct killer *k, struct mux *m)
{
	int error;

	k->mux = m;
	k->killedby = -1;
	sigemptyset(&k->sigset);
	sigaddset(&k->sigset, SIGINT);
	sigaddset(&k->sigset, SIGHUP);
	sigaddset(&k->sigset, SIGTERM);
	sigaddset(&k->sigset, SIGPIPE);
	pthread_sigmask(SIG_BLOCK, &k->sigset, NULL);
	error = pthread_create(&k->thread, NULL, killer_run, k);
	if (error)
		err(1, "pthread_create");
}

/* The main loop of the killer thread. */
static void *
killer_run(void *arg)
{
	struct killer *k;
	int error, sig, old;

	k = arg;
again:
	error = sigwait(&k->sigset, &sig);
	assert(!error);
	if (sig == SIGINT || sig == SIGHUP || sig == SIGTERM) {
		if (k->killedby == -1) {
			k->killedby = sig;
			/* Ensure we don't get canceled during the shutdown. */
			pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, &old);
			mux_shutdown(k->mux, "Cleaning up ...",
			    STATUS_INTERRUPTED);
			pthread_setcancelstate(old, NULL);
		}
	}
	goto again;
}

/* Stop the killer thread. */
static void
killer_stop(struct killer *k)
{
	void *val;
	int error;

	error = pthread_cancel(k->thread);
	assert(!error);
	pthread_join(k->thread, &val);
	assert(val == PTHREAD_CANCELED);
	pthread_sigmask(SIG_UNBLOCK, &k->sigset, NULL);
}
