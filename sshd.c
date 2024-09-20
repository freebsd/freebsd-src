/* $OpenBSD: sshd.c,v 1.612 2024/09/15 01:11:26 djm Exp $ */
/*
 * Copyright (c) 2000, 2001, 2002 Markus Friedl.  All rights reserved.
 * Copyright (c) 2002 Niels Provos.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "includes.h"

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#include "openbsd-compat/sys-tree.h"
#include "openbsd-compat/sys-queue.h"
#include <sys/wait.h>

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#ifdef HAVE_PATHS_H
#include <paths.h>
#endif
#include <grp.h>
#ifdef HAVE_POLL_H
#include <poll.h>
#endif
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#ifdef WITH_OPENSSL
#include <openssl/evp.h>
#include <openssl/rand.h>
#include "openbsd-compat/openssl-compat.h"
#endif

#ifdef HAVE_SECUREWARE
#include <sys/security.h>
#include <prot.h>
#endif

#include "xmalloc.h"
#include "ssh.h"
#include "sshpty.h"
#include "log.h"
#include "sshbuf.h"
#include "misc.h"
#include "servconf.h"
#include "compat.h"
#include "digest.h"
#include "sshkey.h"
#include "authfile.h"
#include "pathnames.h"
#include "canohost.h"
#include "hostfile.h"
#include "auth.h"
#include "authfd.h"
#include "msg.h"
#include "version.h"
#include "ssherr.h"
#include "sk-api.h"
#include "addr.h"
#include "srclimit.h"

/* Re-exec fds */
#define REEXEC_DEVCRYPTO_RESERVED_FD	(STDERR_FILENO + 1)
#define REEXEC_STARTUP_PIPE_FD		(STDERR_FILENO + 2)
#define REEXEC_CONFIG_PASS_FD		(STDERR_FILENO + 3)
#define REEXEC_MIN_FREE_FD		(STDERR_FILENO + 4)

extern char *__progname;

/* Server configuration options. */
ServerOptions options;

/*
 * Debug mode flag.  This can be set on the command line.  If debug
 * mode is enabled, extra debugging output will be sent to the system
 * log, the daemon will not go to background, and will exit after processing
 * the first connection.
 */
int debug_flag = 0;

/* Saved arguments to main(). */
static char **saved_argv;
static int saved_argc;

/*
 * The sockets that the server is listening; this is used in the SIGHUP
 * signal handler.
 */
#define	MAX_LISTEN_SOCKS	16
static int listen_socks[MAX_LISTEN_SOCKS];
static int num_listen_socks = 0;

/*
 * Any really sensitive data in the application is contained in this
 * structure. The idea is that this structure could be locked into memory so
 * that the pages do not get written into swap.  However, there are some
 * problems. The private key contains BIGNUMs, and we do not (in principle)
 * have access to the internals of them, and locking just the structure is
 * not very useful.  Currently, memory locking is not implemented.
 */
struct {
	struct sshkey	**host_keys;		/* all private host keys */
	struct sshkey	**host_pubkeys;		/* all public host keys */
	struct sshkey	**host_certificates;	/* all public host certificates */
	int		have_ssh2_key;
} sensitive_data;

/* This is set to true when a signal is received. */
static volatile sig_atomic_t received_siginfo = 0;
static volatile sig_atomic_t received_sigchld = 0;
static volatile sig_atomic_t received_sighup = 0;
static volatile sig_atomic_t received_sigterm = 0;

/* record remote hostname or ip */
u_int utmp_len = HOST_NAME_MAX+1;

/*
 * The early_child/children array below is used for tracking children of the
 * listening sshd process early in their lifespans, before they have
 * completed authentication. This tracking is needed for four things:
 *
 * 1) Implementing the MaxStartups limit of concurrent unauthenticated
 *    connections.
 * 2) Avoiding a race condition for SIGHUP processing, where child processes
 *    may have listen_socks open that could collide with main listener process
 *    after it restarts.
 * 3) Ensuring that rexec'd sshd processes have received their initial state
 *    from the parent listen process before handling SIGHUP.
 * 4) Tracking and logging unsuccessful exits from the preauth sshd monitor,
 *    including and especially those for LoginGraceTime timeouts.
 *
 * Child processes signal that they have completed closure of the listen_socks
 * and (if applicable) received their rexec state by sending a char over their
 * sock.
 *
 * Child processes signal that authentication has completed by sending a
 * second char over the socket before closing it, otherwise the listener will
 * continue tracking the child (and using up a MaxStartups slot) until the
 * preauth subprocess exits, whereupon the listener will log its exit status.
 * preauth processes will exit with a status of EXIT_LOGIN_GRACE to indicate
 * they did not authenticate before the LoginGraceTime alarm fired.
 */
struct early_child {
	int pipefd;
	int early;		/* Indicates child closed listener */
	char *id;		/* human readable connection identifier */
	pid_t pid;
	struct xaddr addr;
	int have_addr;
	int status, have_status;
};
static struct early_child *children;
static int children_active;
static int startup_pipe = -1;		/* in child */

/* sshd_config buffer */
struct sshbuf *cfg;

/* Included files from the configuration file */
struct include_list includes = TAILQ_HEAD_INITIALIZER(includes);

/* message to be displayed after login */
struct sshbuf *loginmsg;

/* Unprivileged user */
struct passwd *privsep_pw = NULL;

static char *listener_proctitle;

/*
 * Close all listening sockets
 */
static void
close_listen_socks(void)
{
	int i;

	for (i = 0; i < num_listen_socks; i++)
		close(listen_socks[i]);
	num_listen_socks = 0;
}

/* Allocate and initialise the children array */
static void
child_alloc(void)
{
	int i;

	children = xcalloc(options.max_startups, sizeof(*children));
	for (i = 0; i < options.max_startups; i++) {
		children[i].pipefd = -1;
		children[i].pid = -1;
	}
}

/* Register a new connection in the children array; child pid comes later */
static struct early_child *
child_register(int pipefd, int sockfd)
{
	int i, lport, rport;
	char *laddr = NULL, *raddr = NULL;
	struct early_child *child = NULL;
	struct sockaddr_storage addr;
	socklen_t addrlen = sizeof(addr);
	struct sockaddr *sa = (struct sockaddr *)&addr;

	for (i = 0; i < options.max_startups; i++) {
		if (children[i].pipefd != -1 || children[i].pid > 0)
			continue;
		child = &(children[i]);
		break;
	}
	if (child == NULL) {
		fatal_f("error: accepted connection when all %d child "
		    " slots full", options.max_startups);
	}
	child->pipefd = pipefd;
	child->early = 1;
	/* record peer address, if available */
	if (getpeername(sockfd, sa, &addrlen) == 0 &&
	   addr_sa_to_xaddr(sa, addrlen, &child->addr) == 0)
		child->have_addr = 1;
	/* format peer address string for logs */
	if ((lport = get_local_port(sockfd)) == 0 ||
	    (rport = get_peer_port(sockfd)) == 0) {
		/* Not a TCP socket */
		raddr = get_peer_ipaddr(sockfd);
		xasprintf(&child->id, "connection from %s", raddr);
	} else {
		laddr = get_local_ipaddr(sockfd);
		raddr = get_peer_ipaddr(sockfd);
		xasprintf(&child->id, "connection from %s to %s", raddr, laddr);
	}
	free(laddr);
	free(raddr);
	if (++children_active > options.max_startups)
		fatal_f("internal error: more children than max_startups");

	return child;
}

/*
 * Finally free a child entry. Don't call this directly.
 */
static void
child_finish(struct early_child *child)
{
	if (children_active == 0)
		fatal_f("internal error: children_active underflow");
	if (child->pipefd != -1)
		close(child->pipefd);
	free(child->id);
	memset(child, '\0', sizeof(*child));
	child->pipefd = -1;
	child->pid = -1;
	children_active--;
}

/*
 * Close a child's pipe. This will not stop tracking the child immediately
 * (it will still be tracked for waitpid()) unless force_final is set, or
 * child has already exited.
 */
static void
child_close(struct early_child *child, int force_final, int quiet)
{
	if (!quiet)
		debug_f("enter%s", force_final ? " (forcing)" : "");
	if (child->pipefd != -1) {
		close(child->pipefd);
		child->pipefd = -1;
	}
	if (child->pid == -1 || force_final)
		child_finish(child);
}

/* Record a child exit. Safe to call from signal handlers */
static void
child_exit(pid_t pid, int status)
{
	int i;

	if (children == NULL || pid <= 0)
		return;
	for (i = 0; i < options.max_startups; i++) {
		if (children[i].pid == pid) {
			children[i].have_status = 1;
			children[i].status = status;
			break;
		}
	}
}

/*
 * Reap a child entry that has exited, as previously flagged
 * using child_exit().
 * Handles logging of exit condition and will finalise the child if its pipe
 * had already been closed.
 */
static void
child_reap(struct early_child *child)
{
	LogLevel level = SYSLOG_LEVEL_DEBUG1;
	int was_crash, penalty_type = SRCLIMIT_PENALTY_NONE;

	/* Log exit information */
	if (WIFSIGNALED(child->status)) {
		/*
		 * Increase logging for signals potentially associated
		 * with serious conditions.
		 */
		if ((was_crash = signal_is_crash(WTERMSIG(child->status))))
			level = SYSLOG_LEVEL_ERROR;
		do_log2(level, "session process %ld for %s killed by "
		    "signal %d%s", (long)child->pid, child->id,
		    WTERMSIG(child->status), child->early ? " (early)" : "");
		if (was_crash)
			penalty_type = SRCLIMIT_PENALTY_CRASH;
	} else if (!WIFEXITED(child->status)) {
		penalty_type = SRCLIMIT_PENALTY_CRASH;
		error("session process %ld for %s terminated abnormally, "
		    "status=0x%x%s", (long)child->pid, child->id, child->status,
		    child->early ? " (early)" : "");
	} else {
		/* Normal exit. We care about the status */
		switch (WEXITSTATUS(child->status)) {
		case 0:
			debug3_f("preauth child %ld for %s completed "
			    "normally %s", (long)child->pid, child->id,
			    child->early ? " (early)" : "");
			break;
		case EXIT_LOGIN_GRACE:
			penalty_type = SRCLIMIT_PENALTY_GRACE_EXCEEDED;
			logit("Timeout before authentication for %s, "
			    "pid = %ld%s", child->id, (long)child->pid,
			    child->early ? " (early)" : "");
			break;
		case EXIT_CHILD_CRASH:
			penalty_type = SRCLIMIT_PENALTY_CRASH;
			logit("Session process %ld unpriv child crash for %s%s",
			    (long)child->pid, child->id,
			    child->early ? " (early)" : "");
			break;
		case EXIT_AUTH_ATTEMPTED:
			penalty_type = SRCLIMIT_PENALTY_AUTHFAIL;
			debug_f("preauth child %ld for %s exited "
			    "after unsuccessful auth attempt %s",
			    (long)child->pid, child->id,
			    child->early ? " (early)" : "");
			break;
		case EXIT_CONFIG_REFUSED:
			penalty_type = SRCLIMIT_PENALTY_REFUSECONNECTION;
			debug_f("preauth child %ld for %s prohibited by"
			    "RefuseConnection %s",
			    (long)child->pid, child->id,
			    child->early ? " (early)" : "");
			break;
		default:
			penalty_type = SRCLIMIT_PENALTY_NOAUTH;
			debug_f("preauth child %ld for %s exited "
			    "with status %d%s", (long)child->pid, child->id,
			    WEXITSTATUS(child->status),
			    child->early ? " (early)" : "");
			break;
		}
	}

	if (child->have_addr)
		srclimit_penalise(&child->addr, penalty_type);

	child->pid = -1;
	child->have_status = 0;
	if (child->pipefd == -1)
		child_finish(child);
}

/* Reap all children that have exited; called after SIGCHLD */
static void
child_reap_all_exited(void)
{
	int i;
	pid_t pid;
	int status;

	if (children == NULL)
		return;

	for (;;) {
		if ((pid = waitpid(-1, &status, WNOHANG)) == 0)
			break;
		else if (pid == -1) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
			if (errno != ECHILD)
				error_f("waitpid: %s", strerror(errno));
			break;
		}
		child_exit(pid, status);
	}

	for (i = 0; i < options.max_startups; i++) {
		if (!children[i].have_status)
			continue;
		child_reap(&(children[i]));
	}
}

static void
close_startup_pipes(void)
{
	int i;

	if (children == NULL)
		return;
	for (i = 0; i < options.max_startups; i++) {
		if (children[i].pipefd != -1)
			child_close(&(children[i]), 1, 1);
	}
}

/* Called after SIGINFO */
static void
show_info(void)
{
	int i;

	/* XXX print listening sockets here too */
	if (children == NULL)
		return;
	logit("%d active startups", children_active);
	for (i = 0; i < options.max_startups; i++) {
		if (children[i].pipefd == -1 && children[i].pid <= 0)
			continue;
		logit("child %d: fd=%d pid=%ld %s%s", i, children[i].pipefd,
		    (long)children[i].pid, children[i].id,
		    children[i].early ? " (early)" : "");
	}
	srclimit_penalty_info();
}

/*
 * Signal handler for SIGHUP.  Sshd execs itself when it receives SIGHUP;
 * the effect is to reread the configuration file (and to regenerate
 * the server key).
 */

static void
sighup_handler(int sig)
{
	received_sighup = 1;
}

/*
 * Called from the main program after receiving SIGHUP.
 * Restarts the server.
 */
static void
sighup_restart(void)
{
	logit("Received SIGHUP; restarting.");
	if (options.pid_file != NULL)
		unlink(options.pid_file);
	platform_pre_restart();
	close_listen_socks();
	close_startup_pipes();
	ssh_signal(SIGHUP, SIG_IGN); /* will be restored after exec */
	execv(saved_argv[0], saved_argv);
	logit("RESTART FAILED: av[0]='%.100s', error: %.100s.", saved_argv[0],
	    strerror(errno));
	exit(1);
}

/*
 * Generic signal handler for terminating signals in the master daemon.
 */
static void
sigterm_handler(int sig)
{
	received_sigterm = sig;
}

#ifdef SIGINFO
static void
siginfo_handler(int sig)
{
	received_siginfo = 1;
}
#endif

static void
main_sigchld_handler(int sig)
{
	received_sigchld = 1;
}

/*
 * returns 1 if connection should be dropped, 0 otherwise.
 * dropping starts at connection #max_startups_begin with a probability
 * of (max_startups_rate/100). the probability increases linearly until
 * all connections are dropped for startups > max_startups
 */
static int
should_drop_connection(int startups)
{
	int p, r;

	if (startups < options.max_startups_begin)
		return 0;
	if (startups >= options.max_startups)
		return 1;
	if (options.max_startups_rate == 100)
		return 1;

	p  = 100 - options.max_startups_rate;
	p *= startups - options.max_startups_begin;
	p /= options.max_startups - options.max_startups_begin;
	p += options.max_startups_rate;
	r = arc4random_uniform(100);

	debug_f("p %d, r %d", p, r);
	return (r < p) ? 1 : 0;
}

/*
 * Check whether connection should be accepted by MaxStartups or for penalty.
 * Returns 0 if the connection is accepted. If the connection is refused,
 * returns 1 and attempts to send notification to client.
 * Logs when the MaxStartups condition is entered or exited, and periodically
 * while in that state.
 */
static int
drop_connection(int sock, int startups, int notify_pipe)
{
	char *laddr, *raddr;
	const char *reason = NULL, msg[] = "Not allowed at this time\r\n";
	static time_t last_drop, first_drop;
	static u_int ndropped;
	LogLevel drop_level = SYSLOG_LEVEL_VERBOSE;
	time_t now;

	if (!srclimit_penalty_check_allow(sock, &reason)) {
		drop_level = SYSLOG_LEVEL_INFO;
		goto handle;
	}

	now = monotime();
	if (!should_drop_connection(startups) &&
	    srclimit_check_allow(sock, notify_pipe) == 1) {
		if (last_drop != 0 &&
		    startups < options.max_startups_begin - 1) {
			/* XXX maybe need better hysteresis here */
			logit("exited MaxStartups throttling after %s, "
			    "%u connections dropped",
			    fmt_timeframe(now - first_drop), ndropped);
			last_drop = 0;
		}
		return 0;
	}

#define SSHD_MAXSTARTUPS_LOG_INTERVAL	(5 * 60)
	if (last_drop == 0) {
		error("beginning MaxStartups throttling");
		drop_level = SYSLOG_LEVEL_INFO;
		first_drop = now;
		ndropped = 0;
	} else if (last_drop + SSHD_MAXSTARTUPS_LOG_INTERVAL < now) {
		/* Periodic logs */
		error("in MaxStartups throttling for %s, "
		    "%u connections dropped",
		    fmt_timeframe(now - first_drop), ndropped + 1);
		drop_level = SYSLOG_LEVEL_INFO;
	}
	last_drop = now;
	ndropped++;
	reason = "past Maxstartups";

 handle:
	laddr = get_local_ipaddr(sock);
	raddr = get_peer_ipaddr(sock);
	do_log2(drop_level, "drop connection #%d from [%s]:%d on [%s]:%d %s",
	    startups,
	    raddr, get_peer_port(sock),
	    laddr, get_local_port(sock),
	    reason);
	free(laddr);
	free(raddr);
	/* best-effort notification to client */
	(void)write(sock, msg, sizeof(msg) - 1);
	return 1;
}

static void
usage(void)
{
	fprintf(stderr, "%s, %s\n", SSH_RELEASE, SSH_OPENSSL_VERSION);
	fprintf(stderr,
"usage: sshd [-46DdeGiqTtV] [-C connection_spec] [-c host_cert_file]\n"
"            [-E log_file] [-f config_file] [-g login_grace_time]\n"
"            [-h host_key_file] [-o option] [-p port] [-u len]\n"
	);
	exit(1);
}

static struct sshbuf *
pack_hostkeys(void)
{
	struct sshbuf *keybuf = NULL, *hostkeys = NULL;
	int r;
	u_int i;

	if ((keybuf = sshbuf_new()) == NULL ||
	    (hostkeys = sshbuf_new()) == NULL)
		fatal_f("sshbuf_new failed");

	/* pack hostkeys into a string. Empty key slots get empty strings */
	for (i = 0; i < options.num_host_key_files; i++) {
		/* private key */
		sshbuf_reset(keybuf);
		if (sensitive_data.host_keys[i] != NULL &&
		    (r = sshkey_private_serialize(sensitive_data.host_keys[i],
		    keybuf)) != 0)
			fatal_fr(r, "serialize hostkey private");
		if ((r = sshbuf_put_stringb(hostkeys, keybuf)) != 0)
			fatal_fr(r, "compose hostkey private");
		/* public key */
		if (sensitive_data.host_pubkeys[i] != NULL) {
			if ((r = sshkey_puts(sensitive_data.host_pubkeys[i],
			    hostkeys)) != 0)
				fatal_fr(r, "compose hostkey public");
		} else {
			if ((r = sshbuf_put_string(hostkeys, NULL, 0)) != 0)
				fatal_fr(r, "compose hostkey empty public");
		}
		/* cert */
		if (sensitive_data.host_certificates[i] != NULL) {
			if ((r = sshkey_puts(
			    sensitive_data.host_certificates[i],
			    hostkeys)) != 0)
				fatal_fr(r, "compose host cert");
		} else {
			if ((r = sshbuf_put_string(hostkeys, NULL, 0)) != 0)
				fatal_fr(r, "compose host cert empty");
		}
	}

	sshbuf_free(keybuf);
	return hostkeys;
}

static void
send_rexec_state(int fd, struct sshbuf *conf)
{
	struct sshbuf *m = NULL, *inc = NULL, *hostkeys = NULL;
	struct include_item *item = NULL;
	int r, sz;

	debug3_f("entering fd = %d config len %zu", fd,
	    sshbuf_len(conf));

	if ((m = sshbuf_new()) == NULL ||
	    (inc = sshbuf_new()) == NULL)
		fatal_f("sshbuf_new failed");

	/* pack includes into a string */
	TAILQ_FOREACH(item, &includes, entry) {
		if ((r = sshbuf_put_cstring(inc, item->selector)) != 0 ||
		    (r = sshbuf_put_cstring(inc, item->filename)) != 0 ||
		    (r = sshbuf_put_stringb(inc, item->contents)) != 0)
			fatal_fr(r, "compose includes");
	}

	hostkeys = pack_hostkeys();

	/*
	 * Protocol from reexec master to child:
	 *	string	configuration
	 *	uint64	timing_secret
	 *	string	host_keys[] {
	 *		string private_key
	 *		string public_key
	 *		string certificate
	 *	}
	 *	string	included_files[] {
	 *		string	selector
	 *		string	filename
	 *		string	contents
	 *	}
	 */
	if ((r = sshbuf_put_stringb(m, conf)) != 0 ||
	    (r = sshbuf_put_u64(m, options.timing_secret)) != 0 ||
	    (r = sshbuf_put_stringb(m, hostkeys)) != 0 ||
	    (r = sshbuf_put_stringb(m, inc)) != 0)
		fatal_fr(r, "compose config");

	/* We need to fit the entire message inside the socket send buffer */
	sz = ROUNDUP(sshbuf_len(m) + 5, 16*1024);
	if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sz, sizeof sz) == -1)
		fatal_f("setsockopt SO_SNDBUF: %s", strerror(errno));

	if (ssh_msg_send(fd, 0, m) == -1)
		error_f("ssh_msg_send failed");

	sshbuf_free(m);
	sshbuf_free(inc);
	sshbuf_free(hostkeys);

	debug3_f("done");
}

/*
 * Listen for TCP connections
 */
static void
listen_on_addrs(struct listenaddr *la)
{
	int ret, listen_sock;
	struct addrinfo *ai;
	char ntop[NI_MAXHOST], strport[NI_MAXSERV];

	for (ai = la->addrs; ai; ai = ai->ai_next) {
		if (ai->ai_family != AF_INET && ai->ai_family != AF_INET6)
			continue;
		if (num_listen_socks >= MAX_LISTEN_SOCKS)
			fatal("Too many listen sockets. "
			    "Enlarge MAX_LISTEN_SOCKS");
		if ((ret = getnameinfo(ai->ai_addr, ai->ai_addrlen,
		    ntop, sizeof(ntop), strport, sizeof(strport),
		    NI_NUMERICHOST|NI_NUMERICSERV)) != 0) {
			error("getnameinfo failed: %.100s",
			    ssh_gai_strerror(ret));
			continue;
		}
		/* Create socket for listening. */
		listen_sock = socket(ai->ai_family, ai->ai_socktype,
		    ai->ai_protocol);
		if (listen_sock == -1) {
			/* kernel may not support ipv6 */
			verbose("socket: %.100s", strerror(errno));
			continue;
		}
		if (set_nonblock(listen_sock) == -1) {
			close(listen_sock);
			continue;
		}
		if (fcntl(listen_sock, F_SETFD, FD_CLOEXEC) == -1) {
			verbose("socket: CLOEXEC: %s", strerror(errno));
			close(listen_sock);
			continue;
		}
		/* Socket options */
		set_reuseaddr(listen_sock);
		if (la->rdomain != NULL &&
		    set_rdomain(listen_sock, la->rdomain) == -1) {
			close(listen_sock);
			continue;
		}

		/* Only communicate in IPv6 over AF_INET6 sockets. */
		if (ai->ai_family == AF_INET6)
			sock_set_v6only(listen_sock);

		debug("Bind to port %s on %s.", strport, ntop);

		/* Bind the socket to the desired port. */
		if (bind(listen_sock, ai->ai_addr, ai->ai_addrlen) == -1) {
			error("Bind to port %s on %s failed: %.200s.",
			    strport, ntop, strerror(errno));
			close(listen_sock);
			continue;
		}
		listen_socks[num_listen_socks] = listen_sock;
		num_listen_socks++;

		/* Start listening on the port. */
		if (listen(listen_sock, SSH_LISTEN_BACKLOG) == -1)
			fatal("listen on [%s]:%s: %.100s",
			    ntop, strport, strerror(errno));
		logit("Server listening on %s port %s%s%s.",
		    ntop, strport,
		    la->rdomain == NULL ? "" : " rdomain ",
		    la->rdomain == NULL ? "" : la->rdomain);
	}
}

static void
server_listen(void)
{
	u_int i;

	/* Initialise per-source limit tracking. */
	srclimit_init(options.max_startups,
	    options.per_source_max_startups,
	    options.per_source_masklen_ipv4,
	    options.per_source_masklen_ipv6,
	    &options.per_source_penalty,
	    options.per_source_penalty_exempt);

	for (i = 0; i < options.num_listen_addrs; i++) {
		listen_on_addrs(&options.listen_addrs[i]);
		freeaddrinfo(options.listen_addrs[i].addrs);
		free(options.listen_addrs[i].rdomain);
		memset(&options.listen_addrs[i], 0,
		    sizeof(options.listen_addrs[i]));
	}
	free(options.listen_addrs);
	options.listen_addrs = NULL;
	options.num_listen_addrs = 0;

	if (!num_listen_socks)
		fatal("Cannot bind any address.");
}

/*
 * The main TCP accept loop. Note that, for the non-debug case, returns
 * from this function are in a forked subprocess.
 */
static void
server_accept_loop(int *sock_in, int *sock_out, int *newsock, int *config_s,
    int log_stderr)
{
	struct pollfd *pfd = NULL;
	int i, ret, npfd;
	int oactive = -1, listening = 0, lameduck = 0;
	int startup_p[2] = { -1 , -1 }, *startup_pollfd;
	char c = 0;
	struct sockaddr_storage from;
	struct early_child *child;
	socklen_t fromlen;
	u_char rnd[256];
	sigset_t nsigset, osigset;

	/* pipes connected to unauthenticated child sshd processes */
	child_alloc();
	startup_pollfd = xcalloc(options.max_startups, sizeof(int));

	/*
	 * Prepare signal mask that we use to block signals that might set
	 * received_sigterm/hup/chld/info, so that we are guaranteed
	 * to immediately wake up the ppoll if a signal is received after
	 * the flag is checked.
	 */
	sigemptyset(&nsigset);
	sigaddset(&nsigset, SIGHUP);
	sigaddset(&nsigset, SIGCHLD);
#ifdef SIGINFO
	sigaddset(&nsigset, SIGINFO);
#endif
	sigaddset(&nsigset, SIGTERM);
	sigaddset(&nsigset, SIGQUIT);

	/* sized for worst-case */
	pfd = xcalloc(num_listen_socks + options.max_startups,
	    sizeof(struct pollfd));

	/*
	 * Stay listening for connections until the system crashes or
	 * the daemon is killed with a signal.
	 */
	for (;;) {
		sigprocmask(SIG_BLOCK, &nsigset, &osigset);
		if (received_sigterm) {
			logit("Received signal %d; terminating.",
			    (int) received_sigterm);
			close_listen_socks();
			if (options.pid_file != NULL)
				unlink(options.pid_file);
			exit(received_sigterm == SIGTERM ? 0 : 255);
		}
		if (received_sigchld) {
			child_reap_all_exited();
			received_sigchld = 0;
		}
		if (received_siginfo) {
			show_info();
			received_siginfo = 0;
		}
		if (oactive != children_active) {
			setproctitle("%s [listener] %d of %d-%d startups",
			    listener_proctitle, children_active,
			    options.max_startups_begin, options.max_startups);
			oactive = children_active;
		}
		if (received_sighup) {
			if (!lameduck) {
				debug("Received SIGHUP; waiting for children");
				close_listen_socks();
				lameduck = 1;
			}
			if (listening <= 0) {
				sigprocmask(SIG_SETMASK, &osigset, NULL);
				sighup_restart();
			}
		}

		for (i = 0; i < num_listen_socks; i++) {
			pfd[i].fd = listen_socks[i];
			pfd[i].events = POLLIN;
		}
		npfd = num_listen_socks;
		for (i = 0; i < options.max_startups; i++) {
			startup_pollfd[i] = -1;
			if (children[i].pipefd != -1) {
				pfd[npfd].fd = children[i].pipefd;
				pfd[npfd].events = POLLIN;
				startup_pollfd[i] = npfd++;
			}
		}

		/* Wait until a connection arrives or a child exits. */
		ret = ppoll(pfd, npfd, NULL, &osigset);
		if (ret == -1 && errno != EINTR) {
			error("ppoll: %.100s", strerror(errno));
			if (errno == EINVAL)
				cleanup_exit(1); /* can't recover */
		}
		sigprocmask(SIG_SETMASK, &osigset, NULL);
		if (ret == -1)
			continue;

		for (i = 0; i < options.max_startups; i++) {
			if (children[i].pipefd == -1 ||
			    startup_pollfd[i] == -1 ||
			    !(pfd[startup_pollfd[i]].revents & (POLLIN|POLLHUP)))
				continue;
			switch (read(children[i].pipefd, &c, sizeof(c))) {
			case -1:
				if (errno == EINTR || errno == EAGAIN)
					continue;
				if (errno != EPIPE) {
					error_f("startup pipe %d (fd=%d): "
					    "read %s", i, children[i].pipefd,
					    strerror(errno));
				}
				/* FALLTHROUGH */
			case 0:
				/* child exited preauth */
				if (children[i].early)
					listening--;
				srclimit_done(children[i].pipefd);
				child_close(&(children[i]), 0, 0);
				break;
			case 1:
				if (children[i].early && c == '\0') {
					/* child has finished preliminaries */
					listening--;
					children[i].early = 0;
					debug2_f("child %lu for %s received "
					    "config", (long)children[i].pid,
					    children[i].id);
				} else if (!children[i].early && c == '\001') {
					/* child has completed auth */
					debug2_f("child %lu for %s auth done",
					    (long)children[i].pid,
					    children[i].id);
					child_close(&(children[i]), 1, 0);
				} else {
					error_f("unexpected message 0x%02x "
					    "child %ld for %s in state %d",
					    (int)c, (long)children[i].pid,
					    children[i].id, children[i].early);
				}
				break;
			}
		}
		for (i = 0; i < num_listen_socks; i++) {
			if (!(pfd[i].revents & POLLIN))
				continue;
			fromlen = sizeof(from);
			*newsock = accept(listen_socks[i],
			    (struct sockaddr *)&from, &fromlen);
			if (*newsock == -1) {
				if (errno != EINTR && errno != EWOULDBLOCK &&
				    errno != ECONNABORTED && errno != EAGAIN)
					error("accept: %.100s",
					    strerror(errno));
				if (errno == EMFILE || errno == ENFILE)
					usleep(100 * 1000);
				continue;
			}
			if (unset_nonblock(*newsock) == -1) {
				close(*newsock);
				continue;
			}
			if (pipe(startup_p) == -1) {
				error_f("pipe(startup_p): %s", strerror(errno));
				close(*newsock);
				continue;
			}
			if (drop_connection(*newsock,
			    children_active, startup_p[0])) {
				close(*newsock);
				close(startup_p[0]);
				close(startup_p[1]);
				continue;
			}

			if (socketpair(AF_UNIX,
			    SOCK_STREAM, 0, config_s) == -1) {
				error("reexec socketpair: %s",
				    strerror(errno));
				close(*newsock);
				close(startup_p[0]);
				close(startup_p[1]);
				continue;
			}

			/*
			 * Got connection.  Fork a child to handle it, unless
			 * we are in debugging mode.
			 */
			if (debug_flag) {
				/*
				 * In debugging mode.  Close the listening
				 * socket, and start processing the
				 * connection without forking.
				 */
				debug("Server will not fork when running in debugging mode.");
				close_listen_socks();
				*sock_in = *newsock;
				*sock_out = *newsock;
				close(startup_p[0]);
				close(startup_p[1]);
				startup_pipe = -1;
				send_rexec_state(config_s[0], cfg);
				close(config_s[0]);
				free(pfd);
				return;
			}

			/*
			 * Normal production daemon.  Fork, and have
			 * the child process the connection. The
			 * parent continues listening.
			 */
			platform_pre_fork();
			listening++;
			child = child_register(startup_p[0], *newsock);
			if ((child->pid = fork()) == 0) {
				/*
				 * Child.  Close the listening and
				 * max_startup sockets.  Start using
				 * the accepted socket. Reinitialize
				 * logging (since our pid has changed).
				 * We return from this function to handle
				 * the connection.
				 */
				platform_post_fork_child();
				startup_pipe = startup_p[1];
				close_startup_pipes();
				close_listen_socks();
				*sock_in = *newsock;
				*sock_out = *newsock;
				log_init(__progname,
				    options.log_level,
				    options.log_facility,
				    log_stderr);
				close(config_s[0]);
				free(pfd);
				return;
			}

			/* Parent.  Stay in the loop. */
			platform_post_fork_parent(child->pid);
			if (child->pid == -1)
				error("fork: %.100s", strerror(errno));
			else
				debug("Forked child %ld.", (long)child->pid);

			close(startup_p[1]);

			close(config_s[1]);
			send_rexec_state(config_s[0], cfg);
			close(config_s[0]);
			close(*newsock);

			/*
			 * Ensure that our random state differs
			 * from that of the child
			 */
			arc4random_stir();
			arc4random_buf(rnd, sizeof(rnd));
#ifdef WITH_OPENSSL
			RAND_seed(rnd, sizeof(rnd));
			if ((RAND_bytes((u_char *)rnd, 1)) != 1)
				fatal("%s: RAND_bytes failed", __func__);
#endif
			explicit_bzero(rnd, sizeof(rnd));
		}
	}
}

static void
accumulate_host_timing_secret(struct sshbuf *server_cfg,
    struct sshkey *key)
{
	static struct ssh_digest_ctx *ctx;
	u_char *hash;
	size_t len;
	struct sshbuf *buf;
	int r;

	if (ctx == NULL && (ctx = ssh_digest_start(SSH_DIGEST_SHA512)) == NULL)
		fatal_f("ssh_digest_start");
	if (key == NULL) { /* finalize */
		/* add server config in case we are using agent for host keys */
		if (ssh_digest_update(ctx, sshbuf_ptr(server_cfg),
		    sshbuf_len(server_cfg)) != 0)
			fatal_f("ssh_digest_update");
		len = ssh_digest_bytes(SSH_DIGEST_SHA512);
		hash = xmalloc(len);
		if (ssh_digest_final(ctx, hash, len) != 0)
			fatal_f("ssh_digest_final");
		options.timing_secret = PEEK_U64(hash);
		freezero(hash, len);
		ssh_digest_free(ctx);
		ctx = NULL;
		return;
	}
	if ((buf = sshbuf_new()) == NULL)
		fatal_f("could not allocate buffer");
	if ((r = sshkey_private_serialize(key, buf)) != 0)
		fatal_fr(r, "encode %s key", sshkey_ssh_name(key));
	if (ssh_digest_update(ctx, sshbuf_ptr(buf), sshbuf_len(buf)) != 0)
		fatal_f("ssh_digest_update");
	sshbuf_reset(buf);
	sshbuf_free(buf);
}

static char *
prepare_proctitle(int ac, char **av)
{
	char *ret = NULL;
	int i;

	for (i = 0; i < ac; i++)
		xextendf(&ret, " ", "%s", av[i]);
	return ret;
}

static void
print_config(struct connection_info *connection_info)
{
	connection_info->test = 1;
	parse_server_match_config(&options, &includes, connection_info);
	dump_config(&options);
	exit(0);
}

/*
 * Main program for the daemon.
 */
int
main(int ac, char **av)
{
	extern char *optarg;
	extern int optind;
	int log_stderr = 0, inetd_flag = 0, test_flag = 0, no_daemon_flag = 0;
	char *config_file_name = _PATH_SERVER_CONFIG_FILE;
	int r, opt, do_dump_cfg = 0, keytype, already_daemon, have_agent = 0;
	int sock_in = -1, sock_out = -1, newsock = -1, rexec_argc = 0;
	int devnull, config_s[2] = { -1 , -1 }, have_connection_info = 0;
	int need_chroot = 1;
	char *fp, *line, *logfile = NULL, **rexec_argv = NULL;
	struct stat sb;
	u_int i, j;
	mode_t new_umask;
	struct sshkey *key;
	struct sshkey *pubkey;
	struct connection_info connection_info;
	sigset_t sigmask;

	memset(&connection_info, 0, sizeof(connection_info));
#ifdef HAVE_SECUREWARE
	(void)set_auth_parameters(ac, av);
#endif
	__progname = ssh_get_progname(av[0]);

	sigemptyset(&sigmask);
	sigprocmask(SIG_SETMASK, &sigmask, NULL);

	/* Save argv. Duplicate so setproctitle emulation doesn't clobber it */
	saved_argc = ac;
	rexec_argc = ac;
	saved_argv = xcalloc(ac + 1, sizeof(*saved_argv));
	for (i = 0; (int)i < ac; i++)
		saved_argv[i] = xstrdup(av[i]);
	saved_argv[i] = NULL;

#ifndef HAVE_SETPROCTITLE
	/* Prepare for later setproctitle emulation */
	compat_init_setproctitle(ac, av);
	av = saved_argv;
#endif

	if (geteuid() == 0 && setgroups(0, NULL) == -1)
		debug("setgroups(): %.200s", strerror(errno));

	/* Ensure that fds 0, 1 and 2 are open or directed to /dev/null */
	sanitise_stdfd();

	/* Initialize configuration options to their default values. */
	initialize_server_options(&options);

	/* Parse command-line arguments. */
	while ((opt = getopt(ac, av,
	    "C:E:b:c:f:g:h:k:o:p:u:46DGQRTdeiqrtV")) != -1) {
		switch (opt) {
		case '4':
			options.address_family = AF_INET;
			break;
		case '6':
			options.address_family = AF_INET6;
			break;
		case 'f':
			config_file_name = optarg;
			break;
		case 'c':
			servconf_add_hostcert("[command-line]", 0,
			    &options, optarg);
			break;
		case 'd':
			if (debug_flag == 0) {
				debug_flag = 1;
				options.log_level = SYSLOG_LEVEL_DEBUG1;
			} else if (options.log_level < SYSLOG_LEVEL_DEBUG3)
				options.log_level++;
			break;
		case 'D':
			no_daemon_flag = 1;
			break;
		case 'G':
			do_dump_cfg = 1;
			break;
		case 'E':
			logfile = optarg;
			/* FALLTHROUGH */
		case 'e':
			log_stderr = 1;
			break;
		case 'i':
			inetd_flag = 1;
			break;
		case 'r':
			logit("-r option is deprecated");
			break;
		case 'R':
			fatal("-R not supported here");
			break;
		case 'Q':
			/* ignored */
			break;
		case 'q':
			options.log_level = SYSLOG_LEVEL_QUIET;
			break;
		case 'b':
			/* protocol 1, ignored */
			break;
		case 'p':
			options.ports_from_cmdline = 1;
			if (options.num_ports >= MAX_PORTS) {
				fprintf(stderr, "too many ports.\n");
				exit(1);
			}
			options.ports[options.num_ports++] = a2port(optarg);
			if (options.ports[options.num_ports-1] <= 0) {
				fprintf(stderr, "Bad port number.\n");
				exit(1);
			}
			break;
		case 'g':
			if ((options.login_grace_time = convtime(optarg)) == -1) {
				fprintf(stderr, "Invalid login grace time.\n");
				exit(1);
			}
			break;
		case 'k':
			/* protocol 1, ignored */
			break;
		case 'h':
			servconf_add_hostkey("[command-line]", 0,
			    &options, optarg, 1);
			break;
		case 't':
			test_flag = 1;
			break;
		case 'T':
			test_flag = 2;
			break;
		case 'C':
			if (parse_server_match_testspec(&connection_info,
			    optarg) == -1)
				exit(1);
			have_connection_info = 1;
			break;
		case 'u':
			utmp_len = (u_int)strtonum(optarg, 0, HOST_NAME_MAX+1+1, NULL);
			if (utmp_len > HOST_NAME_MAX+1) {
				fprintf(stderr, "Invalid utmp length.\n");
				exit(1);
			}
			break;
		case 'o':
			line = xstrdup(optarg);
			if (process_server_config_line(&options, line,
			    "command-line", 0, NULL, NULL, &includes) != 0)
				exit(1);
			free(line);
			break;
		case 'V':
			fprintf(stderr, "%s, %s\n",
			    SSH_RELEASE, SSH_OPENSSL_VERSION);
			exit(0);
		default:
			usage();
			break;
		}
	}
	if (!test_flag && !inetd_flag && !do_dump_cfg && !path_absolute(av[0]))
		fatal("sshd requires execution with an absolute path");

	closefrom(STDERR_FILENO + 1);

	/* Reserve fds we'll need later for reexec things */
	if ((devnull = open(_PATH_DEVNULL, O_RDWR)) == -1)
		fatal("open %s: %s", _PATH_DEVNULL, strerror(errno));
	while (devnull < REEXEC_MIN_FREE_FD) {
		if ((devnull = dup(devnull)) == -1)
			fatal("dup %s: %s", _PATH_DEVNULL, strerror(errno));
	}

	seed_rng();

	/* If requested, redirect the logs to the specified logfile. */
	if (logfile != NULL) {
		char *cp, pid_s[32];

		snprintf(pid_s, sizeof(pid_s), "%ld", (unsigned long)getpid());
		cp = percent_expand(logfile,
		    "p", pid_s,
		    "P", "sshd",
		    (char *)NULL);
		log_redirect_stderr_to(cp);
		free(cp);
	}

	/*
	 * Force logging to stderr until we have loaded the private host
	 * key (unless started from inetd)
	 */
	log_init(__progname,
	    options.log_level == SYSLOG_LEVEL_NOT_SET ?
	    SYSLOG_LEVEL_INFO : options.log_level,
	    options.log_facility == SYSLOG_FACILITY_NOT_SET ?
	    SYSLOG_FACILITY_AUTH : options.log_facility,
	    log_stderr || !inetd_flag || debug_flag);

	/*
	 * Unset KRB5CCNAME, otherwise the user's session may inherit it from
	 * root's environment
	 */
	if (getenv("KRB5CCNAME") != NULL)
		(void) unsetenv("KRB5CCNAME");

	sensitive_data.have_ssh2_key = 0;

	/*
	 * If we're not doing an extended test do not silently ignore connection
	 * test params.
	 */
	if (test_flag < 2 && have_connection_info)
		fatal("Config test connection parameter (-C) provided without "
		    "test mode (-T)");

	/* Fetch our configuration */
	if ((cfg = sshbuf_new()) == NULL)
		fatal("sshbuf_new config failed");
	if (strcasecmp(config_file_name, "none") != 0)
		load_server_config(config_file_name, cfg);

	parse_server_config(&options, config_file_name, cfg,
	    &includes, NULL, 0);

	/* Fill in default values for those options not explicitly set. */
	fill_default_server_options(&options);

	/* Check that options are sensible */
	if (options.authorized_keys_command_user == NULL &&
	    (options.authorized_keys_command != NULL &&
	    strcasecmp(options.authorized_keys_command, "none") != 0))
		fatal("AuthorizedKeysCommand set without "
		    "AuthorizedKeysCommandUser");
	if (options.authorized_principals_command_user == NULL &&
	    (options.authorized_principals_command != NULL &&
	    strcasecmp(options.authorized_principals_command, "none") != 0))
		fatal("AuthorizedPrincipalsCommand set without "
		    "AuthorizedPrincipalsCommandUser");

	/*
	 * Check whether there is any path through configured auth methods.
	 * Unfortunately it is not possible to verify this generally before
	 * daemonisation in the presence of Match blocks, but this catches
	 * and warns for trivial misconfigurations that could break login.
	 */
	if (options.num_auth_methods != 0) {
		for (i = 0; i < options.num_auth_methods; i++) {
			if (auth2_methods_valid(options.auth_methods[i],
			    1) == 0)
				break;
		}
		if (i >= options.num_auth_methods)
			fatal("AuthenticationMethods cannot be satisfied by "
			    "enabled authentication methods");
	}

	/* Check that there are no remaining arguments. */
	if (optind < ac) {
		fprintf(stderr, "Extra argument %s.\n", av[optind]);
		exit(1);
	}

	debug("sshd version %s, %s", SSH_VERSION, SSH_OPENSSL_VERSION);

	if (do_dump_cfg)
		print_config(&connection_info);

	/* load host keys */
	sensitive_data.host_keys = xcalloc(options.num_host_key_files,
	    sizeof(struct sshkey *));
	sensitive_data.host_pubkeys = xcalloc(options.num_host_key_files,
	    sizeof(struct sshkey *));

	if (options.host_key_agent) {
		if (strcmp(options.host_key_agent, SSH_AUTHSOCKET_ENV_NAME))
			setenv(SSH_AUTHSOCKET_ENV_NAME,
			    options.host_key_agent, 1);
		if ((r = ssh_get_authentication_socket(NULL)) == 0)
			have_agent = 1;
		else
			error_r(r, "Could not connect to agent \"%s\"",
			    options.host_key_agent);
	}

	for (i = 0; i < options.num_host_key_files; i++) {
		int ll = options.host_key_file_userprovided[i] ?
		    SYSLOG_LEVEL_ERROR : SYSLOG_LEVEL_DEBUG1;

		if (options.host_key_files[i] == NULL)
			continue;
		if ((r = sshkey_load_private(options.host_key_files[i], "",
		    &key, NULL)) != 0 && r != SSH_ERR_SYSTEM_ERROR)
			do_log2_r(r, ll, "Unable to load host key \"%s\"",
			    options.host_key_files[i]);
		if (sshkey_is_sk(key) &&
		    key->sk_flags & SSH_SK_USER_PRESENCE_REQD) {
			debug("host key %s requires user presence, ignoring",
			    options.host_key_files[i]);
			key->sk_flags &= ~SSH_SK_USER_PRESENCE_REQD;
		}
		if (r == 0 && key != NULL &&
		    (r = sshkey_shield_private(key)) != 0) {
			do_log2_r(r, ll, "Unable to shield host key \"%s\"",
			    options.host_key_files[i]);
			sshkey_free(key);
			key = NULL;
		}
		if ((r = sshkey_load_public(options.host_key_files[i],
		    &pubkey, NULL)) != 0 && r != SSH_ERR_SYSTEM_ERROR)
			do_log2_r(r, ll, "Unable to load host key \"%s\"",
			    options.host_key_files[i]);
		if (pubkey != NULL && key != NULL) {
			if (!sshkey_equal(pubkey, key)) {
				error("Public key for %s does not match "
				    "private key", options.host_key_files[i]);
				sshkey_free(pubkey);
				pubkey = NULL;
			}
		}
		if (pubkey == NULL && key != NULL) {
			if ((r = sshkey_from_private(key, &pubkey)) != 0)
				fatal_r(r, "Could not demote key: \"%s\"",
				    options.host_key_files[i]);
		}
		if (pubkey != NULL && (r = sshkey_check_rsa_length(pubkey,
		    options.required_rsa_size)) != 0) {
			error_fr(r, "Host key %s", options.host_key_files[i]);
			sshkey_free(pubkey);
			sshkey_free(key);
			continue;
		}
		sensitive_data.host_keys[i] = key;
		sensitive_data.host_pubkeys[i] = pubkey;

		if (key == NULL && pubkey != NULL && have_agent) {
			debug("will rely on agent for hostkey %s",
			    options.host_key_files[i]);
			keytype = pubkey->type;
		} else if (key != NULL) {
			keytype = key->type;
			accumulate_host_timing_secret(cfg, key);
		} else {
			do_log2(ll, "Unable to load host key: %s",
			    options.host_key_files[i]);
			sensitive_data.host_keys[i] = NULL;
			sensitive_data.host_pubkeys[i] = NULL;
			continue;
		}

		switch (keytype) {
		case KEY_RSA:
		case KEY_DSA:
		case KEY_ECDSA:
		case KEY_ED25519:
		case KEY_ECDSA_SK:
		case KEY_ED25519_SK:
		case KEY_XMSS:
			if (have_agent || key != NULL)
				sensitive_data.have_ssh2_key = 1;
			break;
		}
		if ((fp = sshkey_fingerprint(pubkey, options.fingerprint_hash,
		    SSH_FP_DEFAULT)) == NULL)
			fatal("sshkey_fingerprint failed");
		debug("%s host key #%d: %s %s",
		    key ? "private" : "agent", i, sshkey_ssh_name(pubkey), fp);
		free(fp);
	}
	accumulate_host_timing_secret(cfg, NULL);
	if (!sensitive_data.have_ssh2_key) {
		logit("sshd: no hostkeys available -- exiting.");
		exit(1);
	}

	/*
	 * Load certificates. They are stored in an array at identical
	 * indices to the public keys that they relate to.
	 */
	sensitive_data.host_certificates = xcalloc(options.num_host_key_files,
	    sizeof(struct sshkey *));
	for (i = 0; i < options.num_host_key_files; i++)
		sensitive_data.host_certificates[i] = NULL;

	for (i = 0; i < options.num_host_cert_files; i++) {
		if (options.host_cert_files[i] == NULL)
			continue;
		if ((r = sshkey_load_public(options.host_cert_files[i],
		    &key, NULL)) != 0) {
			error_r(r, "Could not load host certificate \"%s\"",
			    options.host_cert_files[i]);
			continue;
		}
		if (!sshkey_is_cert(key)) {
			error("Certificate file is not a certificate: %s",
			    options.host_cert_files[i]);
			sshkey_free(key);
			continue;
		}
		/* Find matching private key */
		for (j = 0; j < options.num_host_key_files; j++) {
			if (sshkey_equal_public(key,
			    sensitive_data.host_pubkeys[j])) {
				sensitive_data.host_certificates[j] = key;
				break;
			}
		}
		if (j >= options.num_host_key_files) {
			error("No matching private key for certificate: %s",
			    options.host_cert_files[i]);
			sshkey_free(key);
			continue;
		}
		sensitive_data.host_certificates[j] = key;
		debug("host certificate: #%u type %d %s", j, key->type,
		    sshkey_type(key));
	}

	/* Ensure privsep directory is correctly configured. */
	need_chroot = ((getuid() == 0 || geteuid() == 0) ||
	    options.kerberos_authentication);
	if ((getpwnam(SSH_PRIVSEP_USER)) == NULL && need_chroot) {
		fatal("Privilege separation user %s does not exist",
		    SSH_PRIVSEP_USER);
	}
	endpwent();

	if (need_chroot) {
		if ((stat(_PATH_PRIVSEP_CHROOT_DIR, &sb) == -1) ||
		    (S_ISDIR(sb.st_mode) == 0))
			fatal("Missing privilege separation directory: %s",
			    _PATH_PRIVSEP_CHROOT_DIR);
#ifdef HAVE_CYGWIN
		if (check_ntsec(_PATH_PRIVSEP_CHROOT_DIR) &&
		    (sb.st_uid != getuid () ||
		    (sb.st_mode & (S_IWGRP|S_IWOTH)) != 0))
#else
		if (sb.st_uid != 0 || (sb.st_mode & (S_IWGRP|S_IWOTH)) != 0)
#endif
			fatal("%s must be owned by root and not group or "
			    "world-writable.", _PATH_PRIVSEP_CHROOT_DIR);
	}

	if (test_flag > 1)
		print_config(&connection_info);

	/* Configuration looks good, so exit if in test mode. */
	if (test_flag)
		exit(0);

	/*
	 * Clear out any supplemental groups we may have inherited.  This
	 * prevents inadvertent creation of files with bad modes (in the
	 * portable version at least, it's certainly possible for PAM
	 * to create a file, and we can't control the code in every
	 * module which might be used).
	 */
	if (setgroups(0, NULL) < 0)
		debug("setgroups() failed: %.200s", strerror(errno));

	/* Prepare arguments for sshd-session */
	if (rexec_argc < 0)
		fatal("rexec_argc %d < 0", rexec_argc);
	rexec_argv = xcalloc(rexec_argc + 3, sizeof(char *));
	/* Point to the sshd-session binary instead of sshd */
	rexec_argv[0] = options.sshd_session_path;
	for (i = 1; i < (u_int)rexec_argc; i++) {
		debug("rexec_argv[%d]='%s'", i, saved_argv[i]);
		rexec_argv[i] = saved_argv[i];
	}
	rexec_argv[rexec_argc++] = "-R";
	rexec_argv[rexec_argc] = NULL;
	if (stat(rexec_argv[0], &sb) != 0 || !(sb.st_mode & (S_IXOTH|S_IXUSR)))
		fatal("%s does not exist or is not executable", rexec_argv[0]);
	debug3("using %s for re-exec", rexec_argv[0]);

	listener_proctitle = prepare_proctitle(ac, av);

	/* Ensure that umask disallows at least group and world write */
	new_umask = umask(0077) | 0022;
	(void) umask(new_umask);

	/* Initialize the log (it is reinitialized below in case we forked). */
	if (debug_flag && !inetd_flag)
		log_stderr = 1;
	log_init(__progname, options.log_level,
	    options.log_facility, log_stderr);
	for (i = 0; i < options.num_log_verbose; i++)
		log_verbose_add(options.log_verbose[i]);

	/*
	 * If not in debugging mode, not started from inetd and not already
	 * daemonized (eg re-exec via SIGHUP), disconnect from the controlling
	 * terminal, and fork.  The original process exits.
	 */
	already_daemon = daemonized();
	if (!(debug_flag || inetd_flag || no_daemon_flag || already_daemon)) {

		if (daemon(0, 0) == -1)
			fatal("daemon() failed: %.200s", strerror(errno));

		disconnect_controlling_tty();
	}
	/* Reinitialize the log (because of the fork above). */
	log_init(__progname, options.log_level, options.log_facility, log_stderr);

	/*
	 * Chdir to the root directory so that the current disk can be
	 * unmounted if desired.
	 */
	if (chdir("/") == -1)
		error("chdir(\"/\"): %s", strerror(errno));

	/* ignore SIGPIPE */
	ssh_signal(SIGPIPE, SIG_IGN);

	/* Get a connection, either from inetd or a listening TCP socket */
	if (inetd_flag) {
		/* Send configuration to ancestor sshd-session process */
		if (socketpair(AF_UNIX, SOCK_STREAM, 0, config_s) == -1)
			fatal("socketpair: %s", strerror(errno));
		send_rexec_state(config_s[0], cfg);
		close(config_s[0]);
	} else {
		platform_pre_listen();
		server_listen();

		ssh_signal(SIGHUP, sighup_handler);
		ssh_signal(SIGCHLD, main_sigchld_handler);
		ssh_signal(SIGTERM, sigterm_handler);
		ssh_signal(SIGQUIT, sigterm_handler);
#ifdef SIGINFO
		ssh_signal(SIGINFO, siginfo_handler);
#endif

		platform_post_listen();

		/*
		 * Write out the pid file after the sigterm handler
		 * is setup and the listen sockets are bound
		 */
		if (options.pid_file != NULL && !debug_flag) {
			FILE *f = fopen(options.pid_file, "w");

			if (f == NULL) {
				error("Couldn't create pid file \"%s\": %s",
				    options.pid_file, strerror(errno));
			} else {
				fprintf(f, "%ld\n", (long) getpid());
				fclose(f);
			}
		}

		/* Accept a connection and return in a forked child */
		server_accept_loop(&sock_in, &sock_out,
		    &newsock, config_s, log_stderr);
	}

	/* This is the child processing a new connection. */
	setproctitle("%s", "[accepted]");

	/*
	 * Create a new session and process group since the 4.4BSD
	 * setlogin() affects the entire process group.  We don't
	 * want the child to be able to affect the parent.
	 */
	if (!debug_flag && !inetd_flag && setsid() == -1)
		error("setsid: %.100s", strerror(errno));

	debug("rexec start in %d out %d newsock %d pipe %d sock %d/%d",
	    sock_in, sock_out, newsock, startup_pipe, config_s[0], config_s[1]);
	if (!inetd_flag) {
		if (dup2(newsock, STDIN_FILENO) == -1)
			fatal("dup2 stdin: %s", strerror(errno));
		if (dup2(STDIN_FILENO, STDOUT_FILENO) == -1)
			fatal("dup2 stdout: %s", strerror(errno));
		if (newsock > STDOUT_FILENO)
			close(newsock);
	}
	if (config_s[1] != REEXEC_CONFIG_PASS_FD) {
		if (dup2(config_s[1], REEXEC_CONFIG_PASS_FD) == -1)
			fatal("dup2 config_s: %s", strerror(errno));
		close(config_s[1]);
	}
	if (startup_pipe == -1)
		close(REEXEC_STARTUP_PIPE_FD);
	else if (startup_pipe != REEXEC_STARTUP_PIPE_FD) {
		if (dup2(startup_pipe, REEXEC_STARTUP_PIPE_FD) == -1)
			fatal("dup2 startup_p: %s", strerror(errno));
		close(startup_pipe);
	}
	log_redirect_stderr_to(NULL);
	closefrom(REEXEC_MIN_FREE_FD);

	ssh_signal(SIGHUP, SIG_IGN); /* avoid reset to SIG_DFL */
	execv(rexec_argv[0], rexec_argv);

	fatal("rexec of %s failed: %s", rexec_argv[0], strerror(errno));
}

/* server specific fatal cleanup */
void
cleanup_exit(int i)
{
	_exit(i);
}
