/*
 * Copyright 1995, 1996 by David Mazieres <dm@lcs.mit.edu>.
 *
 * Modification and redistribution in source and binary forms is
 * permitted provided that due credit is given to the author and the
 * OpenBSD project (for instance by leaving this copyright notice
 * intact).
 */

#include "includes.h"
RCSID("$OpenBSD: ssh-keyscan.c,v 1.22 2001/03/06 06:11:18 deraadt Exp $");

#include <sys/queue.h>
#include <errno.h>

#include <openssl/bn.h>

#include "xmalloc.h"
#include "ssh.h"
#include "ssh1.h"
#include "key.h"
#include "buffer.h"
#include "bufaux.h"
#include "log.h"
#include "atomicio.h"

static int argno = 1;		/* Number of argument currently being parsed */

int family = AF_UNSPEC;		/* IPv4, IPv6 or both */

#define MAXMAXFD 256

/* The number of seconds after which to give up on a TCP connection */
int timeout = 5;

int maxfd;
#define MAXCON (maxfd - 10)

extern char *__progname;
fd_set *read_wait;
size_t read_wait_size;
int ncon;

/*
 * Keep a connection structure for each file descriptor.  The state
 * associated with file descriptor n is held in fdcon[n].
 */
typedef struct Connection {
	u_char c_status;	/* State of connection on this file desc. */
#define CS_UNUSED 0		/* File descriptor unused */
#define CS_CON 1		/* Waiting to connect/read greeting */
#define CS_SIZE 2		/* Waiting to read initial packet size */
#define CS_KEYS 3		/* Waiting to read public key packet */
	int c_fd;		/* Quick lookup: c->c_fd == c - fdcon */
	int c_plen;		/* Packet length field for ssh packet */
	int c_len;		/* Total bytes which must be read. */
	int c_off;		/* Length of data read so far. */
	char *c_namebase;	/* Address to free for c_name and c_namelist */
	char *c_name;		/* Hostname of connection for errors */
	char *c_namelist;	/* Pointer to other possible addresses */
	char *c_output_name;	/* Hostname of connection for output */
	char *c_data;		/* Data read from this fd */
	struct timeval c_tv;	/* Time at which connection gets aborted */
	TAILQ_ENTRY(Connection) c_link;	/* List of connections in timeout order. */
} con;

TAILQ_HEAD(conlist, Connection) tq;	/* Timeout Queue */
con *fdcon;

/*
 *  This is just a wrapper around fgets() to make it usable.
 */

/* Stress-test.  Increase this later. */
#define LINEBUF_SIZE 16

typedef struct {
	char *buf;
	u_int size;
	int lineno;
	const char *filename;
	FILE *stream;
	void (*errfun) (const char *,...);
} Linebuf;

Linebuf *
Linebuf_alloc(const char *filename, void (*errfun) (const char *,...))
{
	Linebuf *lb;

	if (!(lb = malloc(sizeof(*lb)))) {
		if (errfun)
			(*errfun) ("linebuf (%s): malloc failed\n", lb->filename);
		return (NULL);
	}
	if (filename) {
		lb->filename = filename;
		if (!(lb->stream = fopen(filename, "r"))) {
			xfree(lb);
			if (errfun)
				(*errfun) ("%s: %s\n", filename, strerror(errno));
			return (NULL);
		}
	} else {
		lb->filename = "(stdin)";
		lb->stream = stdin;
	}

	if (!(lb->buf = malloc(lb->size = LINEBUF_SIZE))) {
		if (errfun)
			(*errfun) ("linebuf (%s): malloc failed\n", lb->filename);
		xfree(lb);
		return (NULL);
	}
	lb->errfun = errfun;
	lb->lineno = 0;
	return (lb);
}

void
Linebuf_free(Linebuf * lb)
{
	fclose(lb->stream);
	xfree(lb->buf);
	xfree(lb);
}

void
Linebuf_restart(Linebuf * lb)
{
	clearerr(lb->stream);
	rewind(lb->stream);
	lb->lineno = 0;
}

int
Linebuf_lineno(Linebuf * lb)
{
	return (lb->lineno);
}

char *
Linebuf_getline(Linebuf * lb)
{
	int n = 0;

	lb->lineno++;
	for (;;) {
		/* Read a line */
		if (!fgets(&lb->buf[n], lb->size - n, lb->stream)) {
			if (ferror(lb->stream) && lb->errfun)
				(*lb->errfun) ("%s: %s\n", lb->filename,
				    strerror(errno));
			return (NULL);
		}
		n = strlen(lb->buf);

		/* Return it or an error if it fits */
		if (n > 0 && lb->buf[n - 1] == '\n') {
			lb->buf[n - 1] = '\0';
			return (lb->buf);
		}
		if (n != lb->size - 1) {
			if (lb->errfun)
				(*lb->errfun) ("%s: skipping incomplete last line\n",
				    lb->filename);
			return (NULL);
		}
		/* Double the buffer if we need more space */
		if (!(lb->buf = realloc(lb->buf, (lb->size *= 2)))) {
			if (lb->errfun)
				(*lb->errfun) ("linebuf (%s): realloc failed\n",
				    lb->filename);
			return (NULL);
		}
	}
}

int
fdlim_get(int hard)
{
	struct rlimit rlfd;

	if (getrlimit(RLIMIT_NOFILE, &rlfd) < 0)
		return (-1);
	if ((hard ? rlfd.rlim_max : rlfd.rlim_cur) == RLIM_INFINITY)
		return 10000;
	else
		return hard ? rlfd.rlim_max : rlfd.rlim_cur;
}

int
fdlim_set(int lim)
{
	struct rlimit rlfd;
	if (lim <= 0)
		return (-1);
	if (getrlimit(RLIMIT_NOFILE, &rlfd) < 0)
		return (-1);
	rlfd.rlim_cur = lim;
	if (setrlimit(RLIMIT_NOFILE, &rlfd) < 0)
		return (-1);
	return (0);
}

/*
 * This is an strsep function that returns a null field for adjacent
 * separators.  This is the same as the 4.4BSD strsep, but different from the
 * one in the GNU libc.
 */
char *
xstrsep(char **str, const char *delim)
{
	char *s, *e;

	if (!**str)
		return (NULL);

	s = *str;
	e = s + strcspn(s, delim);

	if (*e != '\0')
		*e++ = '\0';
	*str = e;

	return (s);
}

/*
 * Get the next non-null token (like GNU strsep).  Strsep() will return a
 * null token for two adjacent separators, so we may have to loop.
 */
char *
strnnsep(char **stringp, char *delim)
{
	char *tok;

	do {
		tok = xstrsep(stringp, delim);
	} while (tok && *tok == '\0');
	return (tok);
}

void
keyprint(char *host, char *output_name, char *kd, int len)
{
	static Key *rsa;
	static Buffer msg;

	if (rsa == NULL) {
		buffer_init(&msg);
		rsa = key_new(KEY_RSA1);
	}
	buffer_append(&msg, kd, len);
	buffer_consume(&msg, 8 - (len & 7));	/* padding */
	if (buffer_get_char(&msg) != (int) SSH_SMSG_PUBLIC_KEY) {
		error("%s: invalid packet type", host);
		buffer_clear(&msg);
		return;
	}
	buffer_consume(&msg, 8);		/* cookie */

	/* server key */
	(void) buffer_get_int(&msg);
	buffer_get_bignum(&msg, rsa->rsa->e);
	buffer_get_bignum(&msg, rsa->rsa->n);

	/* host key */
	(void) buffer_get_int(&msg);
	buffer_get_bignum(&msg, rsa->rsa->e);
	buffer_get_bignum(&msg, rsa->rsa->n);
	buffer_clear(&msg);

	fprintf(stdout, "%s ", output_name ? output_name : host);
	key_write(rsa, stdout);
	fputs("\n", stdout);
}

int
tcpconnect(char *host)
{
	struct addrinfo hints, *ai, *aitop;
	char strport[NI_MAXSERV];
	int gaierr, s = -1;

	snprintf(strport, sizeof strport, "%d", SSH_DEFAULT_PORT);
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = family;
	hints.ai_socktype = SOCK_STREAM;
	if ((gaierr = getaddrinfo(host, strport, &hints, &aitop)) != 0)
		fatal("getaddrinfo %s: %s", host, gai_strerror(gaierr));
	for (ai = aitop; ai; ai = ai->ai_next) {
		s = socket(ai->ai_family, SOCK_STREAM, 0);
		if (s < 0) {
			error("socket: %s", strerror(errno));
			continue;
		}
		if (fcntl(s, F_SETFL, O_NONBLOCK) < 0)
			fatal("F_SETFL: %s", strerror(errno));
		if (connect(s, ai->ai_addr, ai->ai_addrlen) < 0 &&
		    errno != EINPROGRESS)
			error("connect (`%s'): %s", host, strerror(errno));
		else
			break;
		close(s);
		s = -1;
	}
	freeaddrinfo(aitop);
	return s;
}

int
conalloc(char *iname, char *oname)
{
	int s;
	char *namebase, *name, *namelist;

	namebase = namelist = xstrdup(iname);

	do {
		name = xstrsep(&namelist, ",");
		if (!name) {
			xfree(namebase);
			return (-1);
		}
	} while ((s = tcpconnect(name)) < 0);

	if (s >= maxfd)
		fatal("conalloc: fdno %d too high", s);
	if (fdcon[s].c_status)
		fatal("conalloc: attempt to reuse fdno %d", s);

	fdcon[s].c_fd = s;
	fdcon[s].c_status = CS_CON;
	fdcon[s].c_namebase = namebase;
	fdcon[s].c_name = name;
	fdcon[s].c_namelist = namelist;
	fdcon[s].c_output_name = xstrdup(oname);
	fdcon[s].c_data = (char *) &fdcon[s].c_plen;
	fdcon[s].c_len = 4;
	fdcon[s].c_off = 0;
	gettimeofday(&fdcon[s].c_tv, NULL);
	fdcon[s].c_tv.tv_sec += timeout;
	TAILQ_INSERT_TAIL(&tq, &fdcon[s], c_link);
	FD_SET(s, read_wait);
	ncon++;
	return (s);
}

void
confree(int s)
{
	if (s >= maxfd || fdcon[s].c_status == CS_UNUSED)
		fatal("confree: attempt to free bad fdno %d", s);
	close(s);
	xfree(fdcon[s].c_namebase);
	xfree(fdcon[s].c_output_name);
	if (fdcon[s].c_status == CS_KEYS)
		xfree(fdcon[s].c_data);
	fdcon[s].c_status = CS_UNUSED;
	TAILQ_REMOVE(&tq, &fdcon[s], c_link);
	FD_CLR(s, read_wait);
	ncon--;
}

void
contouch(int s)
{
	TAILQ_REMOVE(&tq, &fdcon[s], c_link);
	gettimeofday(&fdcon[s].c_tv, NULL);
	fdcon[s].c_tv.tv_sec += timeout;
	TAILQ_INSERT_TAIL(&tq, &fdcon[s], c_link);
}

int
conrecycle(int s)
{
	int ret;
	con *c = &fdcon[s];
	char *iname, *oname;

	iname = xstrdup(c->c_namelist);
	oname = xstrdup(c->c_output_name);
	confree(s);
	ret = conalloc(iname, oname);
	xfree(iname);
	xfree(oname);
	return (ret);
}

void
congreet(int s)
{
	char buf[80], *cp;
	size_t bufsiz;
	int n = 0;
	con *c = &fdcon[s];

	bufsiz = sizeof(buf);
	cp = buf;
	while (bufsiz-- && (n = read(s, cp, 1)) == 1 && *cp != '\n' && *cp != '\r')
		cp++;
	if (n < 0) {
		if (errno != ECONNREFUSED)
			error("read (%s): %s", c->c_name, strerror(errno));
		conrecycle(s);
		return;
	}
	if (*cp != '\n' && *cp != '\r') {
		error("%s: bad greeting", c->c_name);
		confree(s);
		return;
	}
	*cp = '\0';
	fprintf(stderr, "# %s %s\n", c->c_name, buf);
	n = snprintf(buf, sizeof buf, "SSH-1.5-OpenSSH-keyscan\r\n");
	if (atomicio(write, s, buf, n) != n) {
		error("write (%s): %s", c->c_name, strerror(errno));
		confree(s);
		return;
	}
	c->c_status = CS_SIZE;
	contouch(s);
}

void
conread(int s)
{
	int n;
	con *c = &fdcon[s];

	if (c->c_status == CS_CON) {
		congreet(s);
		return;
	}
	n = read(s, c->c_data + c->c_off, c->c_len - c->c_off);
	if (n < 0) {
		error("read (%s): %s", c->c_name, strerror(errno));
		confree(s);
		return;
	}
	c->c_off += n;

	if (c->c_off == c->c_len)
		switch (c->c_status) {
		case CS_SIZE:
			c->c_plen = htonl(c->c_plen);
			c->c_len = c->c_plen + 8 - (c->c_plen & 7);
			c->c_off = 0;
			c->c_data = xmalloc(c->c_len);
			c->c_status = CS_KEYS;
			break;
		case CS_KEYS:
			keyprint(c->c_name, c->c_output_name, c->c_data, c->c_plen);
			confree(s);
			return;
			break;
		default:
			fatal("conread: invalid status %d", c->c_status);
			break;
		}

	contouch(s);
}

void
conloop(void)
{
	fd_set *r, *e;
	struct timeval seltime, now;
	int i;
	con *c;

	gettimeofday(&now, NULL);
	c = tq.tqh_first;

	if (c && (c->c_tv.tv_sec > now.tv_sec ||
	    (c->c_tv.tv_sec == now.tv_sec && c->c_tv.tv_usec > now.tv_usec))) {
		seltime = c->c_tv;
		seltime.tv_sec -= now.tv_sec;
		seltime.tv_usec -= now.tv_usec;
		if (seltime.tv_usec < 0) {
			seltime.tv_usec += 1000000;
			seltime.tv_sec--;
		}
	} else
		seltime.tv_sec = seltime.tv_usec = 0;

	r = xmalloc(read_wait_size);
	memcpy(r, read_wait, read_wait_size);
	e = xmalloc(read_wait_size);
	memcpy(e, read_wait, read_wait_size);

	while (select(maxfd, r, NULL, e, &seltime) == -1 &&
	    (errno == EAGAIN || errno == EINTR))
		;

	for (i = 0; i < maxfd; i++) {
		if (FD_ISSET(i, e)) {
			error("%s: exception!", fdcon[i].c_name);
			confree(i);
		} else if (FD_ISSET(i, r))
			conread(i);
	}
	xfree(r);
	xfree(e);

	c = tq.tqh_first;
	while (c && (c->c_tv.tv_sec < now.tv_sec ||
	    (c->c_tv.tv_sec == now.tv_sec && c->c_tv.tv_usec < now.tv_usec))) {
		int s = c->c_fd;

		c = c->c_link.tqe_next;
		conrecycle(s);
	}
}

char *
nexthost(int argc, char **argv)
{
	static Linebuf *lb;

	for (;;) {
		if (!lb) {
			if (argno >= argc)
				return (NULL);
			if (argv[argno][0] != '-')
				return (argv[argno++]);
			if (!strcmp(argv[argno], "--")) {
				if (++argno >= argc)
					return (NULL);
				return (argv[argno++]);
			} else if (!strncmp(argv[argno], "-f", 2)) {
				char *fname;

				if (argv[argno][2])
					fname = &argv[argno++][2];
				else if (++argno >= argc) {
					error("missing filename for `-f'");
					return (NULL);
				} else
					fname = argv[argno++];
				if (!strcmp(fname, "-"))
					fname = NULL;
				lb = Linebuf_alloc(fname, error);
			} else
				error("ignoring invalid/misplaced option `%s'",
				    argv[argno++]);
		} else {
			char *line;

			line = Linebuf_getline(lb);
			if (line)
				return (line);
			Linebuf_free(lb);
			lb = NULL;
		}
	}
}

void
usage(void)
{
	fatal("usage: %s [-t timeout] { [--] host | -f file } ...", __progname);
	return;
}

int
main(int argc, char **argv)
{
	char *host = NULL;

	TAILQ_INIT(&tq);

	if (argc <= argno)
		usage();

	if (argv[1][0] == '-' && argv[1][1] == 't') {
		argno++;
		if (argv[1][2])
			timeout = atoi(&argv[1][2]);
		else {
			if (argno >= argc)
				usage();
			timeout = atoi(argv[argno++]);
		}
		if (timeout <= 0)
			usage();
	}
	if (argc <= argno)
		usage();

	maxfd = fdlim_get(1);
	if (maxfd < 0)
		fatal("%s: fdlim_get: bad value", __progname);
	if (maxfd > MAXMAXFD)
		maxfd = MAXMAXFD;
	if (MAXCON <= 0)
		fatal("%s: not enough file descriptors", __progname);
	if (maxfd > fdlim_get(0))
		fdlim_set(maxfd);
	fdcon = xmalloc(maxfd * sizeof(con));
	memset(fdcon, 0, maxfd * sizeof(con));

	read_wait_size = howmany(maxfd, NFDBITS) * sizeof(fd_mask);
	read_wait = xmalloc(read_wait_size);
	memset(read_wait, 0, read_wait_size);

	do {
		while (ncon < MAXCON) {
			char *name;

			host = nexthost(argc, argv);
			if (host == NULL)
				break;
			name = strnnsep(&host, " \t\n");
			conalloc(name, *host ? host : name);
		}
		conloop();
	} while (host);
	while (ncon > 0)
		conloop();

	return (0);
}
