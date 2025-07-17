/* $OpenBSD: ssh-keyscan.c,v 1.161 2024/09/09 02:39:57 djm Exp $ */
/*
 * Copyright 1995, 1996 by David Mazieres <dm@lcs.mit.edu>.
 *
 * Modification and redistribution in source and binary forms is
 * permitted provided that due credit is given to the author and the
 * OpenBSD project by leaving this copyright notice intact.
 */

#include "includes.h"
 
#include <sys/types.h>
#include "openbsd-compat/sys-queue.h"
#include <sys/resource.h>
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif

#include <netinet/in.h>
#include <arpa/inet.h>

#ifdef WITH_OPENSSL
#include <openssl/bn.h>
#endif

#include <limits.h>
#include <netdb.h>
#include <errno.h>
#ifdef HAVE_POLL_H
#include <poll.h>
#endif
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "xmalloc.h"
#include "ssh.h"
#include "sshbuf.h"
#include "sshkey.h"
#include "cipher.h"
#include "digest.h"
#include "kex.h"
#include "compat.h"
#include "myproposal.h"
#include "packet.h"
#include "dispatch.h"
#include "log.h"
#include "atomicio.h"
#include "misc.h"
#include "hostfile.h"
#include "ssherr.h"
#include "ssh_api.h"
#include "dns.h"
#include "addr.h"

/* Flag indicating whether IPv4 or IPv6.  This can be set on the command line.
   Default value is AF_UNSPEC means both IPv4 and IPv6. */
int IPv4or6 = AF_UNSPEC;

int ssh_port = SSH_DEFAULT_PORT;

#define KT_DSA		(1)
#define KT_RSA		(1<<1)
#define KT_ECDSA	(1<<2)
#define KT_ED25519	(1<<3)
#define KT_XMSS		(1<<4)
#define KT_ECDSA_SK	(1<<5)
#define KT_ED25519_SK	(1<<6)

#define KT_MIN		KT_DSA
#define KT_MAX		KT_ED25519_SK

int get_cert = 0;
int get_keytypes = KT_RSA|KT_ECDSA|KT_ED25519|KT_ECDSA_SK|KT_ED25519_SK;

int hash_hosts = 0;		/* Hash hostname on output */

int print_sshfp = 0;		/* Print SSHFP records instead of known_hosts */

int found_one = 0;		/* Successfully found a key */

int hashalg = -1;		/* Hash for SSHFP records or -1 for all */

int quiet = 0;			/* Don't print key comment lines */

#define MAXMAXFD 256

/* The number of seconds after which to give up on a TCP connection */
int timeout = 5;

int maxfd;
#define MAXCON (maxfd - 10)

extern char *__progname;
struct pollfd *read_wait;
int ncon;

/*
 * Keep a connection structure for each file descriptor.  The state
 * associated with file descriptor n is held in fdcon[n].
 */
typedef struct Connection {
	u_char c_status;	/* State of connection on this file desc. */
#define CS_UNUSED 0		/* File descriptor unused */
#define CS_CON 1		/* Waiting to connect/read greeting */
	int c_fd;		/* Quick lookup: c->c_fd == c - fdcon */
	int c_keytype;		/* Only one of KT_* */
	sig_atomic_t c_done;	/* SSH2 done */
	char *c_namebase;	/* Address to free for c_name and c_namelist */
	char *c_name;		/* Hostname of connection for errors */
	char *c_namelist;	/* Pointer to other possible addresses */
	char *c_output_name;	/* Hostname of connection for output */
	struct ssh *c_ssh;	/* SSH-connection */
	struct timespec c_ts;	/* Time at which connection gets aborted */
	TAILQ_ENTRY(Connection) c_link;	/* List of connections in timeout order. */
} con;

TAILQ_HEAD(conlist, Connection) tq;	/* Timeout Queue */
con *fdcon;

static void keyprint(con *c, struct sshkey *key);

static int
fdlim_get(int hard)
{
#if defined(HAVE_GETRLIMIT) && defined(RLIMIT_NOFILE)
	struct rlimit rlfd;
	rlim_t lim;

	if (getrlimit(RLIMIT_NOFILE, &rlfd) == -1)
		return -1;
	lim = hard ? rlfd.rlim_max : rlfd.rlim_cur;
	if (lim <= 0)
		return -1;
	if (lim == RLIM_INFINITY)
		lim = SSH_SYSFDMAX;
	if (lim >= INT_MAX)
		lim = INT_MAX;
	return lim;
#else
	return (SSH_SYSFDMAX <= 0) ? -1 :
	    ((SSH_SYSFDMAX >= INT_MAX) ? INT_MAX : SSH_SYSFDMAX);
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
	if (getrlimit(RLIMIT_NOFILE, &rlfd) == -1)
		return (-1);
	rlfd.rlim_cur = lim;
	if (setrlimit(RLIMIT_NOFILE, &rlfd) == -1)
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


static int
key_print_wrapper(struct sshkey *hostkey, struct ssh *ssh)
{
	con *c;

	if ((c = ssh_get_app_data(ssh)) != NULL)
		keyprint(c, hostkey);
	/* always abort key exchange */
	return -1;
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

static void
keygrab_ssh2(con *c)
{
	char *myproposal[PROPOSAL_MAX] = { KEX_CLIENT };
	int r;

	switch (c->c_keytype) {
	case KT_DSA:
		myproposal[PROPOSAL_SERVER_HOST_KEY_ALGS] = get_cert ?
		    "ssh-dss-cert-v01@openssh.com" : "ssh-dss";
		break;
	case KT_RSA:
		myproposal[PROPOSAL_SERVER_HOST_KEY_ALGS] = get_cert ?
		    "rsa-sha2-512-cert-v01@openssh.com,"
		    "rsa-sha2-256-cert-v01@openssh.com,"
		    "ssh-rsa-cert-v01@openssh.com" :
		    "rsa-sha2-512,"
		    "rsa-sha2-256,"
		    "ssh-rsa";
		break;
	case KT_ED25519:
		myproposal[PROPOSAL_SERVER_HOST_KEY_ALGS] = get_cert ?
		    "ssh-ed25519-cert-v01@openssh.com" : "ssh-ed25519";
		break;
	case KT_XMSS:
		myproposal[PROPOSAL_SERVER_HOST_KEY_ALGS] = get_cert ?
		    "ssh-xmss-cert-v01@openssh.com" : "ssh-xmss@openssh.com";
		break;
	case KT_ECDSA:
		myproposal[PROPOSAL_SERVER_HOST_KEY_ALGS] = get_cert ?
		    "ecdsa-sha2-nistp256-cert-v01@openssh.com,"
		    "ecdsa-sha2-nistp384-cert-v01@openssh.com,"
		    "ecdsa-sha2-nistp521-cert-v01@openssh.com" :
		    "ecdsa-sha2-nistp256,"
		    "ecdsa-sha2-nistp384,"
		    "ecdsa-sha2-nistp521";
		break;
	case KT_ECDSA_SK:
		myproposal[PROPOSAL_SERVER_HOST_KEY_ALGS] = get_cert ?
		    "sk-ecdsa-sha2-nistp256-cert-v01@openssh.com" :
		    "sk-ecdsa-sha2-nistp256@openssh.com";
		break;
	case KT_ED25519_SK:
		myproposal[PROPOSAL_SERVER_HOST_KEY_ALGS] = get_cert ?
		    "sk-ssh-ed25519-cert-v01@openssh.com" :
		    "sk-ssh-ed25519@openssh.com";
		break;
	default:
		fatal("unknown key type %d", c->c_keytype);
		break;
	}
	if ((r = kex_setup(c->c_ssh, myproposal)) != 0) {
		free(c->c_ssh);
		fprintf(stderr, "kex_setup: %s\n", ssh_err(r));
		exit(1);
	}
#ifdef WITH_OPENSSL
	c->c_ssh->kex->kex[KEX_DH_GRP1_SHA1] = kex_gen_client;
	c->c_ssh->kex->kex[KEX_DH_GRP14_SHA1] = kex_gen_client;
	c->c_ssh->kex->kex[KEX_DH_GRP14_SHA256] = kex_gen_client;
	c->c_ssh->kex->kex[KEX_DH_GRP16_SHA512] = kex_gen_client;
	c->c_ssh->kex->kex[KEX_DH_GRP18_SHA512] = kex_gen_client;
	c->c_ssh->kex->kex[KEX_DH_GEX_SHA1] = kexgex_client;
	c->c_ssh->kex->kex[KEX_DH_GEX_SHA256] = kexgex_client;
# ifdef OPENSSL_HAS_ECC
	c->c_ssh->kex->kex[KEX_ECDH_SHA2] = kex_gen_client;
# endif
#endif
	c->c_ssh->kex->kex[KEX_C25519_SHA256] = kex_gen_client;
	c->c_ssh->kex->kex[KEX_KEM_SNTRUP761X25519_SHA512] = kex_gen_client;
	c->c_ssh->kex->kex[KEX_KEM_MLKEM768X25519_SHA256] = kex_gen_client;
	ssh_set_verify_host_key_callback(c->c_ssh, key_print_wrapper);
	/*
	 * do the key-exchange until an error occurs or until
	 * the key_print_wrapper() callback sets c_done.
	 */
	ssh_dispatch_run(c->c_ssh, DISPATCH_BLOCK, &c->c_done);
}

static void
keyprint_one(const char *host, struct sshkey *key)
{
	char *hostport = NULL, *hashed = NULL;
	const char *known_host;
	int r = 0;

	found_one = 1;

	if (print_sshfp) {
		export_dns_rr(host, key, stdout, 0, hashalg);
		return;
	}

	hostport = put_host_port(host, ssh_port);
	lowercase(hostport);
	if (hash_hosts && (hashed = host_hash(hostport, NULL, 0)) == NULL)
		fatal("host_hash failed");
	known_host = hash_hosts ? hashed : hostport;
	if (!get_cert)
		r = fprintf(stdout, "%s ", known_host);
	if (r >= 0 && sshkey_write(key, stdout) == 0)
		(void)fputs("\n", stdout);
	free(hashed);
	free(hostport);
}

static void
keyprint(con *c, struct sshkey *key)
{
	char *hosts = c->c_output_name ? c->c_output_name : c->c_name;
	char *host, *ohosts;

	if (key == NULL)
		return;
	if (get_cert || (!hash_hosts && ssh_port == SSH_DEFAULT_PORT)) {
		keyprint_one(hosts, key);
		return;
	}
	ohosts = hosts = xstrdup(hosts);
	while ((host = strsep(&hosts, ",")) != NULL)
		keyprint_one(host, key);
	free(ohosts);
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
	if ((gaierr = getaddrinfo(host, strport, &hints, &aitop)) != 0) {
		error("getaddrinfo %s: %s", host, ssh_gai_strerror(gaierr));
		return -1;
	}
	for (ai = aitop; ai; ai = ai->ai_next) {
		s = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (s == -1) {
			error("socket: %s", strerror(errno));
			continue;
		}
		if (set_nonblock(s) == -1)
			fatal_f("set_nonblock(%d)", s);
		if (connect(s, ai->ai_addr, ai->ai_addrlen) == -1 &&
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
conalloc(const char *iname, const char *oname, int keytype)
{
	char *namebase, *name, *namelist;
	int s;

	namebase = namelist = xstrdup(iname);

	do {
		name = xstrsep(&namelist, ",");
		if (!name) {
			free(namebase);
			return (-1);
		}
	} while ((s = tcpconnect(name)) < 0);

	if (s >= maxfd)
		fatal("conalloc: fdno %d too high", s);
	if (fdcon[s].c_status)
		fatal("conalloc: attempt to reuse fdno %d", s);

	debug3_f("oname %s kt %d", oname, keytype);
	fdcon[s].c_fd = s;
	fdcon[s].c_status = CS_CON;
	fdcon[s].c_namebase = namebase;
	fdcon[s].c_name = name;
	fdcon[s].c_namelist = namelist;
	fdcon[s].c_output_name = xstrdup(oname);
	fdcon[s].c_keytype = keytype;
	monotime_ts(&fdcon[s].c_ts);
	fdcon[s].c_ts.tv_sec += timeout;
	TAILQ_INSERT_TAIL(&tq, &fdcon[s], c_link);
	read_wait[s].fd = s;
	read_wait[s].events = POLLIN;
	ncon++;
	return (s);
}

static void
confree(int s)
{
	if (s >= maxfd || fdcon[s].c_status == CS_UNUSED)
		fatal("confree: attempt to free bad fdno %d", s);
	free(fdcon[s].c_namebase);
	free(fdcon[s].c_output_name);
	fdcon[s].c_status = CS_UNUSED;
	fdcon[s].c_keytype = 0;
	if (fdcon[s].c_ssh) {
		ssh_packet_close(fdcon[s].c_ssh);
		free(fdcon[s].c_ssh);
		fdcon[s].c_ssh = NULL;
	} else
		close(s);
	TAILQ_REMOVE(&tq, &fdcon[s], c_link);
	read_wait[s].fd = -1;
	read_wait[s].events = 0;
	ncon--;
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
	int n = 0, remote_major = 0, remote_minor = 0;
	char buf[256], *cp;
	char remote_version[sizeof buf];
	size_t bufsiz;
	con *c = &fdcon[s];

	/* send client banner */
	n = snprintf(buf, sizeof buf, "SSH-%d.%d-OpenSSH-keyscan\r\n",
	    PROTOCOL_MAJOR_2, PROTOCOL_MINOR_2);
	if (n < 0 || (size_t)n >= sizeof(buf)) {
		error("snprintf: buffer too small");
		confree(s);
		return;
	}
	if (atomicio(vwrite, s, buf, n) != (size_t)n) {
		error("write (%s): %s", c->c_name, strerror(errno));
		confree(s);
		return;
	}

	/*
	 * Read the server banner as per RFC4253 section 4.2.  The "SSH-"
	 * protocol identification string may be preceded by an arbitrarily
	 * large banner which we must read and ignore.  Loop while reading
	 * newline-terminated lines until we have one starting with "SSH-".
	 * The ID string cannot be longer than 255 characters although the
	 * preceding banner lines may (in which case they'll be discarded
	 * in multiple iterations of the outer loop).
	 */
	for (;;) {
		memset(buf, '\0', sizeof(buf));
		bufsiz = sizeof(buf);
		cp = buf;
		while (bufsiz-- &&
		    (n = atomicio(read, s, cp, 1)) == 1 && *cp != '\n') {
			if (*cp == '\r')
				*cp = '\n';
			cp++;
		}
		if (n != 1 || strncmp(buf, "SSH-", 4) == 0)
			break;
	}
	if (n == 0) {
		switch (errno) {
		case EPIPE:
			error("%s: Connection closed by remote host", c->c_name);
			break;
		case ECONNREFUSED:
			break;
		default:
			error("read (%s): %s", c->c_name, strerror(errno));
			break;
		}
		conrecycle(s);
		return;
	}
	if (cp >= buf + sizeof(buf)) {
		error("%s: greeting exceeds allowable length", c->c_name);
		confree(s);
		return;
	}
	if (*cp != '\n' && *cp != '\r') {
		error("%s: bad greeting", c->c_name);
		confree(s);
		return;
	}
	*cp = '\0';
	if ((c->c_ssh = ssh_packet_set_connection(NULL, s, s)) == NULL)
		fatal("ssh_packet_set_connection failed");
	ssh_packet_set_timeout(c->c_ssh, timeout, 1);
	ssh_set_app_data(c->c_ssh, c);	/* back link */
	c->c_ssh->compat = 0;
	if (sscanf(buf, "SSH-%d.%d-%[^\n]\n",
	    &remote_major, &remote_minor, remote_version) == 3)
		compat_banner(c->c_ssh, remote_version);
	if (!ssh2_capable(remote_major, remote_minor)) {
		debug("%s doesn't support ssh2", c->c_name);
		confree(s);
		return;
	}
	if (!quiet) {
		fprintf(stdout, "%c %s:%d %s\n", print_sshfp ? ';' : '#',
		    c->c_name, ssh_port, chop(buf));
	}
	keygrab_ssh2(c);
	confree(s);
}

static void
conread(int s)
{
	con *c = &fdcon[s];

	if (c->c_status != CS_CON)
		fatal("conread: invalid status %d", c->c_status);

	congreet(s);
}

static void
conloop(void)
{
	struct timespec seltime, now;
	con *c;
	int i;

	monotime_ts(&now);
	c = TAILQ_FIRST(&tq);

	if (c && timespeccmp(&c->c_ts, &now, >))
		timespecsub(&c->c_ts, &now, &seltime);
	else
		timespecclear(&seltime);

	while (ppoll(read_wait, maxfd, &seltime, NULL) == -1) {
		if (errno == EAGAIN || errno == EINTR || errno == EWOULDBLOCK)
			continue;
		error("poll error");
	}

	for (i = 0; i < maxfd; i++) {
		if (read_wait[i].revents & (POLLHUP|POLLERR|POLLNVAL))
			confree(i);
		else if (read_wait[i].revents & (POLLIN|POLLHUP))
			conread(i);
	}

	c = TAILQ_FIRST(&tq);
	while (c && timespeccmp(&c->c_ts, &now, <)) {
		int s = c->c_fd;

		c = TAILQ_NEXT(c, c_link);
		conrecycle(s);
	}
}

static void
do_one_host(char *host)
{
	char *name = strnnsep(&host, " \t\n");
	int j;

	if (name == NULL)
		return;
	for (j = KT_MIN; j <= KT_MAX; j *= 2) {
		if (get_keytypes & j) {
			while (ncon >= MAXCON)
				conloop();
			conalloc(name, *host ? host : name, j);
		}
	}
}

static void
do_host(char *host)
{
	char daddr[128];
	struct xaddr addr, end_addr;
	u_int masklen;

	if (host == NULL)
		return;
	if (addr_pton_cidr(host, &addr, &masklen) != 0) {
		/* Assume argument is a hostname */
		do_one_host(host);
	} else {
		/* Argument is a CIDR range */
		debug("CIDR range %s", host);
		end_addr = addr;
		if (addr_host_to_all1s(&end_addr, masklen) != 0)
			goto badaddr;
		/*
		 * Note: we deliberately include the all-zero/ones addresses.
		 */
		for (;;) {
			if (addr_ntop(&addr, daddr, sizeof(daddr)) != 0) {
 badaddr:
				error("Invalid address %s", host);
				return;
			}
			debug("CIDR expand: address %s", daddr);
			do_one_host(daddr);
			if (addr_cmp(&addr, &end_addr) == 0)
				break;
			addr_increment(&addr);
		};
	}
}

void
sshfatal(const char *file, const char *func, int line, int showfunc,
    LogLevel level, const char *suffix, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	sshlogv(file, func, line, showfunc, level, suffix, fmt, args);
	va_end(args);
	cleanup_exit(255);
}

static void
usage(void)
{
	fprintf(stderr,
	    "usage: ssh-keyscan [-46cDHqv] [-f file] [-O option] [-p port] [-T timeout]\n"
	    "                   [-t type] [host | addrlist namelist]\n");
	exit(1);
}

int
main(int argc, char **argv)
{
	int debug_flag = 0, log_level = SYSLOG_LEVEL_INFO;
	int opt, fopt_count = 0, j;
	char *tname, *cp, *line = NULL;
	size_t linesize = 0;
	FILE *fp;

	extern int optind;
	extern char *optarg;

	__progname = ssh_get_progname(argv[0]);
	seed_rng();
	TAILQ_INIT(&tq);

	/* Ensure that fds 0, 1 and 2 are open or directed to /dev/null */
	sanitise_stdfd();

	if (argc <= 1)
		usage();

	while ((opt = getopt(argc, argv, "cDHqv46O:p:T:t:f:")) != -1) {
		switch (opt) {
		case 'H':
			hash_hosts = 1;
			break;
		case 'c':
			get_cert = 1;
			break;
		case 'D':
			print_sshfp = 1;
			break;
		case 'p':
			ssh_port = a2port(optarg);
			if (ssh_port <= 0) {
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
		case 'q':
			quiet = 1;
			break;
		case 'f':
			if (strcmp(optarg, "-") == 0)
				optarg = NULL;
			argv[fopt_count++] = optarg;
			break;
		case 'O':
			/* Maybe other misc options in the future too */
			if (strncmp(optarg, "hashalg=", 8) != 0)
				fatal("Unsupported -O option");
			if ((hashalg = ssh_digest_alg_by_name(
			    optarg + 8)) == -1)
				fatal("Unsupported hash algorithm");
			break;
		case 't':
			get_keytypes = 0;
			tname = strtok(optarg, ",");
			while (tname) {
				int type = sshkey_type_from_shortname(tname);

				switch (type) {
#ifdef WITH_DSA
				case KEY_DSA:
					get_keytypes |= KT_DSA;
					break;
#endif
				case KEY_ECDSA:
					get_keytypes |= KT_ECDSA;
					break;
				case KEY_RSA:
					get_keytypes |= KT_RSA;
					break;
				case KEY_ED25519:
					get_keytypes |= KT_ED25519;
					break;
				case KEY_XMSS:
					get_keytypes |= KT_XMSS;
					break;
				case KEY_ED25519_SK:
					get_keytypes |= KT_ED25519_SK;
					break;
				case KEY_ECDSA_SK:
					get_keytypes |= KT_ECDSA_SK;
					break;
				case KEY_UNSPEC:
				default:
					fatal("Unknown key type \"%s\"", tname);
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
	fdcon = xcalloc(maxfd, sizeof(con));
	read_wait = xcalloc(maxfd, sizeof(struct pollfd));
	for (j = 0; j < maxfd; j++)
		read_wait[j].fd = -1;

	for (j = 0; j < fopt_count; j++) {
		if (argv[j] == NULL)
			fp = stdin;
		else if ((fp = fopen(argv[j], "r")) == NULL)
			fatal("%s: %s: %s", __progname,
			    fp == stdin ? "<stdin>" : argv[j], strerror(errno));

		while (getline(&line, &linesize, fp) != -1) {
			/* Chomp off trailing whitespace and comments */
			if ((cp = strchr(line, '#')) == NULL)
				cp = line + strlen(line) - 1;
			while (cp >= line) {
				if (*cp == ' ' || *cp == '\t' ||
				    *cp == '\n' || *cp == '#')
					*cp-- = '\0';
				else
					break;
			}

			/* Skip empty lines */
			if (*line == '\0')
				continue;

			do_host(line);
		}

		if (ferror(fp))
			fatal("%s: %s: %s", __progname,
			    fp == stdin ? "<stdin>" : argv[j], strerror(errno));

		if (fp != stdin)
			fclose(fp);
	}
	free(line);

	while (optind < argc)
		do_host(argv[optind++]);

	while (ncon > 0)
		conloop();

	return found_one ? 0 : 1;
}
