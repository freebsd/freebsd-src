/*
 * Copyright 1995, 1996 by David Mazieres <dm@lcs.mit.edu>.
 *
 * Modification and redistribution in source and binary forms is
 * permitted provided that due credit is given to the author and the
 * OpenBSD project by leaving this copyright notice intact.
 */

#include "includes.h"
RCSID("$OpenBSD: ssh-keyscan.c,v 1.52 2005/03/01 15:47:14 jmc Exp $");

#include "openbsd-compat/sys-queue.h"

#include <openssl/bn.h>

#include <setjmp.h>
#include "xmalloc.h"
#include "ssh.h"
#include "ssh1.h"
#include "key.h"
#include "kex.h"
#include "compat.h"
#include "myproposal.h"
#include "packet.h"
#include "dispatch.h"
#include "buffer.h"
#include "bufaux.h"
#include "log.h"
#include "atomicio.h"
#include "misc.h"
#include "hostfile.h"

/* Flag indicating whether IPv4 or IPv6.  This can be set on the command line.
   Default value is AF_UNSPEC means both IPv4 and IPv6. */
int IPv4or6 = AF_UNSPEC;

int ssh_port = SSH_DEFAULT_PORT;

#define KT_RSA1	1
#define KT_DSA	2
#define KT_RSA	4

int get_keytypes = KT_RSA1;	/* Get only RSA1 keys by default */

int hash_hosts = 0;		/* Hash hostname on output */

#define MAXMAXFD 256

/* The number of seconds after which to give up on a TCP connection */
int timeout = 5;

int maxfd;
#define MAXCON (maxfd - 10)

extern char *__progname;
fd_set *read_wait;
size_t read_wait_size;
int ncon;
int nonfatal_fatal = 0;
jmp_buf kexjmp;
Key *kexjmp_key;

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
	int c_keytype;		/* Only one of KT_RSA1, KT_DSA, or KT_RSA */
	char *c_namebase;	/* Address to free for c_name and c_namelist */
	char *c_name;		/* Hostname of connection for errors */
	char *c_namelist;	/* Pointer to other possible addresses */
	char *c_output_name;	/* Hostname of connection for output */
	char *c_data;		/* Data read from this fd */
	Kex *c_kex;		/* The key-exchange struct for ssh2 */
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

static Linebuf *
Linebuf_alloc(const char *filename, void (*errfun) (const char *,...))
{
	Linebuf *lb;

	if (!(lb = malloc(sizeof(*lb)))) {
		if (errfun)
			(*errfun) ("linebuf (%s): malloc failed\n",
			    filename ? filename : "(stdin)");
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

static void
Linebuf_free(Linebuf * lb)
{
	fclose(lb->stream);
	xfree(lb->buf);
	xfree(lb);
}

#if 0
static void
Linebuf_restart(Linebuf * lb)
{
	clearerr(lb->stream);
	rewind(lb->stream);
	lb->lineno = 0;
}

static int
Linebuf_lineno(Linebuf * lb)
{
	return (lb->lineno);
}
#endif

static char *
Linebuf_getline(Linebuf * lb)
{
	int n = 0;
	void *p;

	lb->lineno++;
	for (;;) {
		/* Read a line */
		if (!fgets(&lb->buf[n], lb->size - n, lb->stream)) {
			if (ferror(lb->stream) && lb->errfun)
				(*lb->errfun)("%s: %s\n", lb->filename,
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
				(*lb->errfun)("%s: skipping incomplete last line\n",
				    lb->filename);
			return (NULL);
		}
		/* Double the buffer if we need more space */
		lb->size *= 2;
		if ((p = realloc(lb->buf, lb->size)) == NULL) {
			lb->size /= 2;
			if (lb->errfun)
				(*lb->errfun)("linebuf (%s): realloc failed\n",
				    lb->filename);
			return (NULL);
		}
		lb->buf = p;
	}
}

static int
fdlim_get(int hard)
{
#if defined(HAVE_GETRLIMIT) && defined(RLIMIT_NOFILE)
	struct rlimit rlfd;

	if (getrlimit(RLIMIT_NOFILE, &rlfd) < 0)
		return (-1);
	if ((hard ? rlfd.rlim_max : rlfd.rlim_cur) == RLIM_INFINITY)
		return SSH_SYSFDMAX;
	else
		return hard ? rlfd.rlim_max : rlfd.rlim_cur;
#else
	return SSH_SYSFDMAX;
#endif
}

static int
fdlim_set(int lim)
{
#if defined(HAVE_SETRLIMIT) && defined(RLIMIT_NOFILE)
	struct rlimit rlfd;
#endif

	if (lim <= 0)
		return (-1);
#if defined(HAVE_SETRLIMIT) && defined(RLIMIT_NOFILE)
	if (getrlimit(RLIMIT_NOFILE, &rlfd) < 0)
		return (-1);
	rlfd.rlim_cur = lim;
	if (setrlimit(RLIMIT_NOFILE, &rlfd) < 0)
		return (-1);
#elif defined (HAVE_SETDTABLESIZE)
	setdtablesize(lim);
#endif
	return (0);
}

/*
 * This is an strsep function that returns a null field for adjacent
 * separators.  This is the same as the 4.4BSD strsep, but different from the
 * one in the GNU libc.
 */
static char *
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
static char *
strnnsep(char **stringp, char *delim)
{
	char *tok;

	do {
		tok = xstrsep(stringp, delim);
	} while (tok && *tok == '\0');
	return (tok);
}

static Key *
keygrab_ssh1(con *c)
{
	static Key *rsa;
	static Buffer msg;

	if (rsa == NULL) {
		buffer_init(&msg);
		rsa = key_new(KEY_RSA1);
	}
	buffer_append(&msg, c->c_data, c->c_plen);
	buffer_consume(&msg, 8 - (c->c_plen & 7));	/* padding */
	if (buffer_get_char(&msg) != (int) SSH_SMSG_PUBLIC_KEY) {
		error("%s: invalid packet type", c->c_name);
		buffer_clear(&msg);
		return NULL;
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

	return (rsa);
}

static int
hostjump(Key *hostkey)
{
	kexjmp_key = hostkey;
	longjmp(kexjmp, 1);
}

static int
ssh2_capable(int remote_major, int remote_minor)
{
	switch (remote_major) {
	case 1:
		if (remote_minor == 99)
			return 1;
		break;
	case 2:
		return 1;
	default:
		break;
	}
	return 0;
}

static Key *
keygrab_ssh2(con *c)
{
	int j;

	packet_set_connection(c->c_fd, c->c_fd);
	enable_compat20();
	myproposal[PROPOSAL_SERVER_HOST_KEY_ALGS] = c->c_keytype == KT_DSA?
	    "ssh-dss": "ssh-rsa";
	c->c_kex = kex_setup(myproposal);
	c->c_kex->kex[KEX_DH_GRP1_SHA1] = kexdh_client;
	c->c_kex->kex[KEX_DH_GRP14_SHA1] = kexdh_client;
	c->c_kex->kex[KEX_DH_GEX_SHA1] = kexgex_client;
	c->c_kex->verify_host_key = hostjump;

	if (!(j = setjmp(kexjmp))) {
		nonfatal_fatal = 1;
		dispatch_run(DISPATCH_BLOCK, &c->c_kex->done, c->c_kex);
		fprintf(stderr, "Impossible! dispatch_run() returned!\n");
		exit(1);
	}
	nonfatal_fatal = 0;
	xfree(c->c_kex);
	c->c_kex = NULL;
	packet_close();

	return j < 0? NULL : kexjmp_key;
}

static void
keyprint(con *c, Key *key)
{
	char *host = c->c_output_name ? c->c_output_name : c->c_name;

	if (!key)
		return;
	if (hash_hosts && (host = host_hash(host, NULL, 0)) == NULL)
		fatal("host_hash failed");

	fprintf(stdout, "%s ", host);
	key_write(key, stdout);
	fputs("\n", stdout);
}

static int
tcpconnect(char *host)
{
	struct addrinfo hints, *ai, *aitop;
	char strport[NI_MAXSERV];
	int gaierr, s = -1;

	snprintf(strport, sizeof strport, "%d", ssh_port);
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = IPv4or6;
	hints.ai_socktype = SOCK_STREAM;
	if ((gaierr = getaddrinfo(host, strport, &hints, &aitop)) != 0)
		fatal("getaddrinfo %s: %s", host, gai_strerror(gaierr));
	for (ai = aitop; ai; ai = ai->ai_next) {
		s = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (s < 0) {
			error("socket: %s", strerror(errno));
			continue;
		}
		if (set_nonblock(s) == -1)
			fatal("%s: set_nonblock(%d)", __func__, s);
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

static int
conalloc(char *iname, char *oname, int keytype)
{
	char *namebase, *name, *namelist;
	int s;

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
	fdcon[s].c_keytype = keytype;
	gettimeofday(&fdcon[s].c_tv, NULL);
	fdcon[s].c_tv.tv_sec += timeout;
	TAILQ_INSERT_TAIL(&tq, &fdcon[s], c_link);
	FD_SET(s, read_wait);
	ncon++;
	return (s);
}

static void
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
	fdcon[s].c_keytype = 0;
	TAILQ_REMOVE(&tq, &fdcon[s], c_link);
	FD_CLR(s, read_wait);
	ncon--;
}

static void
contouch(int s)
{
	TAILQ_REMOVE(&tq, &fdcon[s], c_link);
	gettimeofday(&fdcon[s].c_tv, NULL);
	fdcon[s].c_tv.tv_sec += timeout;
	TAILQ_INSERT_TAIL(&tq, &fdcon[s], c_link);
}

static int
conrecycle(int s)
{
	con *c = &fdcon[s];
	int ret;

	ret = conalloc(c->c_namelist, c->c_output_name, c->c_keytype);
	confree(s);
	return (ret);
}

static void
congreet(int s)
{
	int remote_major = 0, remote_minor = 0, n = 0;
	char buf[256], *cp;
	char remote_version[sizeof buf];
	size_t bufsiz;
	con *c = &fdcon[s];

	bufsiz = sizeof(buf);
	cp = buf;
	while (bufsiz-- && (n = atomicio(read, s, cp, 1)) == 1 && *cp != '\n') {
		if (*cp == '\r')
			*cp = '\n';
		cp++;
	}
	if (n < 0) {
		if (errno != ECONNREFUSED)
			error("read (%s): %s", c->c_name, strerror(errno));
		conrecycle(s);
		return;
	}
	if (n == 0) {
		error("%s: Connection closed by remote host", c->c_name);
		conrecycle(s);
		return;
	}
	if (*cp != '\n' && *cp != '\r') {
		error("%s: bad greeting", c->c_name);
		confree(s);
		return;
	}
	*cp = '\0';
	if (sscanf(buf, "SSH-%d.%d-%[^\n]\n",
	    &remote_major, &remote_minor, remote_version) == 3)
		compat_datafellows(remote_version);
	else
		datafellows = 0;
	if (c->c_keytype != KT_RSA1) {
		if (!ssh2_capable(remote_major, remote_minor)) {
			debug("%s doesn't support ssh2", c->c_name);
			confree(s);
			return;
		}
	} else if (remote_major != 1) {
		debug("%s doesn't support ssh1", c->c_name);
		confree(s);
		return;
	}
	fprintf(stderr, "# %s %s\n", c->c_name, chop(buf));
	n = snprintf(buf, sizeof buf, "SSH-%d.%d-OpenSSH-keyscan\r\n",
	    c->c_keytype == KT_RSA1? PROTOCOL_MAJOR_1 : PROTOCOL_MAJOR_2,
	    c->c_keytype == KT_RSA1? PROTOCOL_MINOR_1 : PROTOCOL_MINOR_2);
	if (atomicio(vwrite, s, buf, n) != n) {
		error("write (%s): %s", c->c_name, strerror(errno));
		confree(s);
		return;
	}
	if (c->c_keytype != KT_RSA1) {
		keyprint(c, keygrab_ssh2(c));
		confree(s);
		return;
	}
	c->c_status = CS_SIZE;
	contouch(s);
}

static void
conread(int s)
{
	con *c = &fdcon[s];
	int n;

	if (c->c_status == CS_CON) {
		congreet(s);
		return;
	}
	n = atomicio(read, s, c->c_data + c->c_off, c->c_len - c->c_off);
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
			keyprint(c, keygrab_ssh1(c));
			confree(s);
			return;
			break;
		default:
			fatal("conread: invalid status %d", c->c_status);
			break;
		}

	contouch(s);
}

static void
conloop(void)
{
	struct timeval seltime, now;
	fd_set *r, *e;
	con *c;
	int i;

	gettimeofday(&now, NULL);
	c = TAILQ_FIRST(&tq);

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

	c = TAILQ_FIRST(&tq);
	while (c && (c->c_tv.tv_sec < now.tv_sec ||
	    (c->c_tv.tv_sec == now.tv_sec && c->c_tv.tv_usec < now.tv_usec))) {
		int s = c->c_fd;

		c = TAILQ_NEXT(c, c_link);
		conrecycle(s);
	}
}

static void
do_host(char *host)
{
	char *name = strnnsep(&host, " \t\n");
	int j;

	if (name == NULL)
		return;
	for (j = KT_RSA1; j <= KT_RSA; j *= 2) {
		if (get_keytypes & j) {
			while (ncon >= MAXCON)
				conloop();
			conalloc(name, *host ? host : name, j);
		}
	}
}

void
fatal(const char *fmt,...)
{
	va_list args;

	va_start(args, fmt);
	do_log(SYSLOG_LEVEL_FATAL, fmt, args);
	va_end(args);
	if (nonfatal_fatal)
		longjmp(kexjmp, -1);
	else
		exit(255);
}

static void
usage(void)
{
	fprintf(stderr, "usage: %s [-46Hv] [-f file] [-p port] [-T timeout] [-t type]\n"
	    "\t\t   [host | addrlist namelist] [...]\n",
	    __progname);
	exit(1);
}

int
main(int argc, char **argv)
{
	int debug_flag = 0, log_level = SYSLOG_LEVEL_INFO;
	int opt, fopt_count = 0;
	char *tname;

	extern int optind;
	extern char *optarg;

	__progname = ssh_get_progname(argv[0]);
	init_rng();
	seed_rng();
	TAILQ_INIT(&tq);

	if (argc <= 1)
		usage();

	while ((opt = getopt(argc, argv, "Hv46p:T:t:f:")) != -1) {
		switch (opt) {
		case 'H':
			hash_hosts = 1;
			break;
		case 'p':
			ssh_port = a2port(optarg);
			if (ssh_port == 0) {
				fprintf(stderr, "Bad port '%s'\n", optarg);
				exit(1);
			}
			break;
		case 'T':
			timeout = convtime(optarg);
			if (timeout == -1 || timeout == 0) {
				fprintf(stderr, "Bad timeout '%s'\n", optarg);
				usage();
			}
			break;
		case 'v':
			if (!debug_flag) {
				debug_flag = 1;
				log_level = SYSLOG_LEVEL_DEBUG1;
			}
			else if (log_level < SYSLOG_LEVEL_DEBUG3)
				log_level++;
			else
				fatal("Too high debugging level.");
			break;
		case 'f':
			if (strcmp(optarg, "-") == 0)
				optarg = NULL;
			argv[fopt_count++] = optarg;
			break;
		case 't':
			get_keytypes = 0;
			tname = strtok(optarg, ",");
			while (tname) {
				int type = key_type_from_name(tname);
				switch (type) {
				case KEY_RSA1:
					get_keytypes |= KT_RSA1;
					break;
				case KEY_DSA:
					get_keytypes |= KT_DSA;
					break;
				case KEY_RSA:
					get_keytypes |= KT_RSA;
					break;
				case KEY_UNSPEC:
					fatal("unknown key type %s", tname);
				}
				tname = strtok(NULL, ",");
			}
			break;
		case '4':
			IPv4or6 = AF_INET;
			break;
		case '6':
			IPv4or6 = AF_INET6;
			break;
		case '?':
		default:
			usage();
		}
	}
	if (optind == argc && !fopt_count)
		usage();

	log_init("ssh-keyscan", log_level, SYSLOG_FACILITY_USER, 1);

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

	if (fopt_count) {
		Linebuf *lb;
		char *line;
		int j;

		for (j = 0; j < fopt_count; j++) {
			lb = Linebuf_alloc(argv[j], error);
			if (!lb)
				continue;
			while ((line = Linebuf_getline(lb)) != NULL)
				do_host(line);
			Linebuf_free(lb);
		}
	}

	while (optind < argc)
		do_host(argv[optind++]);

	while (ncon > 0)
		conloop();

	return (0);
}
