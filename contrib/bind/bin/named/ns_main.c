#if !defined(lint) && !defined(SABER)
static const char sccsid[] = "@(#)ns_main.c	4.55 (Berkeley) 7/1/91";
static const char rcsid[] = "$Id: ns_main.c,v 8.162.6.2 2003/06/08 22:08:02 marka Exp $";
#endif /* not lint */

/*
 * Copyright (c) 1986, 1989, 1990
 *    The Regents of the University of California.  All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 * 	This product includes software developed by the University of
 * 	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Portions Copyright (c) 1993 by Digital Equipment Corporation.
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Digital Equipment Corporation not be used in advertising or
 * publicity pertaining to distribution of the document or software without
 * specific, written prior permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND DIGITAL EQUIPMENT CORP. DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL DIGITAL EQUIPMENT
 * CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/*
 * Portions Copyright (c) 1996-2000 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#if !defined(lint) && !defined(SABER)
char copyright[] =
"@(#) Copyright (c) 1986, 1989, 1990 The Regents of the University of California.\n"
"portions Copyright (c) 1993 Digital Equipment Corporation\n"
"portions Copyright (c) 1995-1999 Internet Software Consortium\n"
"portions Copyright (c) 1999 Check Point Software Technologies\n"
"All rights reserved.\n";
#endif /* not lint */

/*
 * Internet Name server (see RCF1035 & others).
 */

#include "port_before.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#ifdef SVR4	/* XXX */
# include <sys/sockio.h>
#else
#ifndef __hpux
# include <sys/mbuf.h>
#endif
#endif

#include <netinet/in.h>
#include <net/route.h>
#include <net/if.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <irs.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <netdb.h>
#include <pwd.h>
#include <resolv.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include <isc/eventlib.h>
#include <isc/logging.h>
#include <isc/memcluster.h>
#include <isc/list.h>

#include "port_after.h"

#ifdef HAVE_GETRUSAGE		/* XXX */
#include <sys/resource.h>
#endif

#define MAIN_PROGRAM
#include "named.h"
#undef MAIN_PROGRAM

#ifdef TRUCLUSTER5
# include <clua/clua.h>
#endif

typedef void (*handler)(void);

typedef struct _savedg {
	struct sockaddr_in from;
	int		dfd;
	interface *	ifp;
	time_t		gen;
	u_char *	buf;
	u_int16_t	buflen;
} savedg;

				/* list of interfaces */
static	LIST(struct _interface)	iflist;
static	int 			iflist_initialized = 0;
static	int			iflist_dont_rescan = 0;

static	const int		drbufsize = 32 * 1024,	/* UDP rcv buf size */
				dsbufsize = 48 * 1024,	/* UDP snd buf size */
				sbufsize = 16 * 1024,	/* TCP snd buf size */ 
#ifdef BROKEN_RECVFROM
				nudptrans = 1,
#else
				nudptrans = 20,		/* #/udps per select */
#endif
				listenmax = 50;

static	u_int16_t		nsid_state;
static	u_int16_t               *nsid_pool;  /* optional query id pool */
static	u_int16_t               *nsid_vtable;  /* optional shuffle table */
static	u_int32_t               nsid_hash_state;
static	u_int16_t               nsid_a1, nsid_a2, nsid_a3;
static	u_int16_t               nsid_c1, nsid_c2, nsid_c3;
static	u_int16_t               nsid_state2;
static	int                     nsid_algorithm;

static	int			needs = 0, needs_exit = 0, needs_restart = 0;
static	handler			handlers[main_need_num];
static	void			savedg_waitfunc(evContext, void*, const void*);
static	void			need_waitfunc(evContext, void *, const void *);
static	int			drain_rcvbuf(evContext, interface *, int,
					     int *, int *);
static	int			drain_all_rcvbuf(evContext);

static	struct qstream		*sq_add(void);
static	int			opensocket_d(interface *),
				opensocket_s(interface *);
static	void			sq_query(struct qstream *),
				dq_remove(interface *);
static	int			sq_dowrite(struct qstream *);
static	void			use_desired_debug(void);
static	void			stream_write(evContext, void *, int, int);

static	interface *		if_find(struct in_addr, u_int16_t port,
				        int anyport);

static void			deallocate_everything(void),
				stream_accept(evContext, void *, int,
					      const void *, int,
					      const void *, int),
				stream_getlen(evContext, void *, int, int),
				stream_getmsg(evContext, void *, int, int),
				datagram_read(evContext, void *, int, int),
				dispatch_message(u_char *, int, int,
						 struct qstream *,
						 struct sockaddr_in, int,
						 interface *);
static void			stream_send(evContext, void *, int,
					       const void *, int,
					       const void *, int);
static int			only_digits(const char *);

static void			init_needs(void),
				handle_needs(void),
				exit_handler(void);

#ifndef HAVE_CUSTOM
static void			custom_init(void),
				custom_shutdown(void);
#endif

static void
usage() {
	fprintf(stderr,
"Usage: named [-d #] [-q] [-r] [-v] [-f] [-p port] [[-b|-c] configfile]\n");
#ifdef CAN_CHANGE_ID
	fprintf(stderr,
"             [-u (username|uid)] [-g (groupname|gid)]\n");
#endif
#ifdef HAVE_CHROOT
	fprintf(stderr,
"             [-t directory]\n");
#endif
	exit(1);
}

static const char bad_p_option[] =
"-p remote/local obsolete; use 'listen-on' in config file to specify local";

static const char bad_directory[] = "chdir failed for directory '%s': %s";

/*ARGSUSED*/
int
main(int argc, char *argv[]) {
	int n;
	char *p;
	int ch;
	struct passwd *pw;
	struct group *gr;

#ifdef _AUX_SOURCE
	set42sig();
#endif
	debugfile = savestr(_PATH_DEBUG, 1);

	user_id = getuid();
	group_id = getgid();

	ns_port = htons(NAMESERVER_PORT);
	desired_debug = debug;

	/* BSD has a better random number generator but it's not clear
	 * that we need it here.
	 */
	gettime(&tt);
	srand(((unsigned)getpid()) + (unsigned)tt.tv_usec);

	(void) umask(022);

	/* Save argv[] before getopt() destroys it -- needed for execvp(). */
	saved_argv = malloc(sizeof(char *) * (argc + 1));
	INSIST(saved_argv != NULL);
	for (n = 0; n < argc; n++) {
		saved_argv[n] = strdup(argv[n]);
		INSIST(saved_argv[n] != NULL);
	}
	saved_argv[argc] = NULL;
	/* XXX we need to free() this for clean shutdowns. */

	while ((ch = getopt(argc, argv, "b:c:d:g:p:t:u:vw:qrf")) != -1) {
		switch (ch) {
		case 'b':
		case 'c':
			if (conffile != NULL)
				(void)freestr(conffile);
			conffile = savestr(optarg, 1);
			break;

		case 'd':
			desired_debug = atoi(optarg);
			if (desired_debug <= 0)
				desired_debug = 1;
			break;

		case 'p':
			/* use nonstandard port number.
			 * usage: -p remote/local
			 * remote is the port number to which
			 * we send queries.  local is the port
			 * on which we listen for queries.
			 * local defaults to same as remote.
			 */
			ns_port = htons((u_int16_t) atoi(optarg));
			p = strchr(optarg, '/');
			if (p) {
				syslog(LOG_WARNING, bad_p_option);
				fprintf(stderr, bad_p_option);
				fputc('\n', stderr);
			}
			break;

		case 'w':
			working_dir = savestr(optarg, 1);
			break;
#ifdef QRYLOG
		case 'q':
			qrylog = 1;
			break;
#endif

		case 'r':
			ns_setoption(OPTION_NORECURSE);
			break;

		case 'f':
			foreground = 1;
			break;

		case 't':
			chroot_dir = savestr(optarg, 1);
			break;

		case 'v':
			fprintf(stdout, "%s\n", Version);
			exit(0);

#ifdef CAN_CHANGE_ID
		case 'u':
			user_name = savestr(optarg, 1);
			if (only_digits(user_name))
				user_id = atoi(user_name);
			else {
				pw = getpwnam(user_name);
				if (pw == NULL) {
					fprintf(stderr,
						"user \"%s\" unknown\n",
						user_name);
					exit(1);
				}
				user_id = pw->pw_uid;
				if (group_name == NULL) {
					char name[256];
					
					sprintf(name, "%lu",
						(u_long)pw->pw_gid);
					group_name = savestr(name, 1);
					group_id = pw->pw_gid;
				}
			}
			break;

		case 'g':
			if (group_name != NULL)
				(void)freestr(group_name);
			group_name = savestr(optarg, 1);
			if (only_digits(group_name))
				group_id = atoi(group_name);
			else {
				gr = getgrnam(group_name);
				if (gr == NULL) {
					fprintf(stderr,
						"group \"%s\" unknown\n",
						group_name);
					exit(1);
				}
				group_id = gr->gr_gid;
			}
			break;
#endif /* CAN_CHANGE_ID */

		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc) {
		if (conffile != NULL)
			(void)freestr(conffile);
		conffile = savestr(*argv, 1);
		argc--, argv++;
	}
	if (argc)
		usage();

	if (conffile == NULL)
		conffile = savestr(_PATH_CONF, 1);

	/*
	 * Make sure we don't inherit any open descriptors
	 * other than those that daemon() can deal with.
	 */
	for (n = sysconf(_SC_OPEN_MAX) - 1; n >= 0; n--)
		if (n != STDIN_FILENO &&
		    n != STDOUT_FILENO &&
		    n != STDERR_FILENO)
			(void) close(n);

	/*
	 * Chroot if desired.
	 */
	if (chroot_dir != NULL) {
#ifdef HAVE_CHROOT
		if (chroot(chroot_dir) < 0) {
			fprintf(stderr, "chroot %s failed: %s\n", chroot_dir,
				strerror(errno));
			exit(1);
		}
		if (chdir("/") < 0) {
			fprintf(stderr, "chdir(\"/\") failed: %s\n",
				strerror(errno));
			exit(1);
		}
#else
		fprintf(stderr, "warning: chroot() not available\n");
		chroot_dir = freestr(chroot_dir);
#endif
	}
	/*
	 * Set working directory.
	 */
	if (working_dir != NULL) {
		if (chdir(working_dir) < 0) {
			syslog(LOG_CRIT, bad_directory, working_dir,
			       strerror(errno));
			fprintf(stderr, bad_directory, working_dir,
				strerror(errno));
			fputc('\n', stderr);
			exit(1);
		}
	}

	/* Establish global event context. */
	evCreate(&ev);

	/* Establish global resolver context. */
	res_ninit(&res);
	res.options &= ~(RES_DEFNAMES | RES_DNSRCH | RES_RECURSE);

	/*
	 * Set up logging.
	 */
	n = LOG_PID;
#ifdef LOG_NOWAIT
	n |= LOG_NOWAIT;
#endif
#ifdef LOG_NDELAY
	n |= LOG_NDELAY;
#endif
#if defined(LOG_CONS) && defined(USE_LOG_CONS)
	n |= LOG_CONS;
#endif
#ifdef SYSLOG_42BSD
	openlog("named", n);
#else
	openlog("named", n, ISC_FACILITY);
#endif

	init_logging();
	set_assertion_failure_callback(ns_assertion_failed);

#ifdef DEBUG
	use_desired_debug();
#endif

	/* Perform system-dependent initialization */
	custom_init();

	init_needs();
	init_signals();

	ns_notice(ns_log_default, "starting (%s).  %s", conffile, Version);

	/*
	 * Initialize and load database.
	 */
	gettime(&tt);
	buildservicelist();
	buildprotolist();
	confmtime = ns_init(conffile);
	time(&boottime);
	resettime = boottime;

	nsid_init();

	/*
	 * Fork and go into background now that
	 * we've done any slow initialization
	 * and are ready to answer queries.
	 */

	if (foreground == 0) {
		if (daemon(1, 0))
			ns_panic(ns_log_default, 1, "daemon: %s",
				 strerror(errno));
		update_pid_file();
	}

	/* Check that udp checksums are on. */
	ns_udp();

	/*
	 * We waited until now to log this because we wanted logging to
	 * be set up the way the user prefers.
	 */
	if (chroot_dir != NULL)
		ns_info(ns_log_security, "chrooted to %s", chroot_dir);

#ifdef CAN_CHANGE_ID
	/*
	 * Set user and group if desired.
	 */
	if (group_name != NULL) {
		if (setgid(group_id) < 0)
			ns_panic(ns_log_security, 1, "setgid(%s): %s",
				 group_name, strerror(errno));
		ns_info(ns_log_security, "group = %s", group_name);
	}
	if (user_name != NULL) {
		if (getuid() == 0 && initgroups(user_name, group_id) < 0)
			ns_panic(ns_log_security, 1, "initgroups(%s, %d): %s",
				 user_name, (int)group_id, strerror(errno));
		endgrent();
		endpwent();
		if (setuid(user_id) < 0)
			ns_panic(ns_log_security, 1, "setuid(%s): %s",
				 user_name, strerror(errno));
		ns_info(ns_log_security, "user = %s", user_name);
		if (user_id != 0)
			iflist_dont_rescan++;
	}
#endif /* CAN_CHANGE_ID */

	ns_notice(ns_log_default, "Ready to answer queries.");
	gettime(&tt);
	prime_cache();
	while (!needs_exit) {
		evEvent event;

		ns_debug(ns_log_default, 15, "main loop");
		if (needs != 0)
			handle_needs();
		else if (evGetNext(ev, &event, EV_WAIT) != -1)
			INSIST_ERR(evDispatch(ev, event) != -1);
		else
			INSIST_ERR(errno == EINTR);
	}
	if (needs_restart)
		ns_info(ns_log_default, "named restarting");
	else
		ns_info(ns_log_default, "named shutting down");
#ifdef BIND_UPDATE
	dynamic_about_to_exit();
#endif
	if (server_options && server_options->pid_filename)
		(void)unlink(server_options->pid_filename);
	ns_logstats(ev, NULL, evNowTime(), evConsTime(0, 0));

	if (NS_OPTION_P(OPTION_DEALLOC_ON_EXIT))
		deallocate_everything();
	else
		shutdown_configuration();

	if (needs_restart)
		execvp(saved_argv[0], saved_argv);
	else
		/* Cleanup for system-dependent stuff */
		custom_shutdown();
	
	return (0);
}

static int
sq_closeone(void) {
	struct qstream *sp, *nextsp;
	struct qstream *candidate = NULL;
	time_t lasttime, maxctime = 0;
	int result = 0;

	gettime(&tt);
	
	for (sp = streamq; sp; sp = nextsp) {
		nextsp = sp->s_next;
		if (sp->s_refcnt)
			continue;
		lasttime = tt.tv_sec - sp->s_time;
		if (lasttime >= VQEXPIRY) {
			sq_remove(sp);
			result = 1;
		} else if (lasttime > maxctime) {
			candidate = sp;
			maxctime = lasttime;
		}
	}
	if (candidate) {
		sq_remove(candidate);
		result = 1;
	}
	return (result);
}

static int
ns_socket(int domain, int type, int protocol) {
	int fd, tmp;

 again:
	fd = socket(domain, type, protocol);
#ifdef F_DUPFD		/* XXX */
	/*
	 * Leave a space for stdio to work in.
	 */
	if (fd >= 0 && fd <= 20) {
		int new;
		if ((new = fcntl(fd, F_DUPFD, 20)) == -1)
			ns_notice(ns_log_default, "fcntl(fd, F_DUPFD, 20): %s",
				  strerror(errno));
		tmp = errno;
		close(fd);
		errno = tmp;
		fd = new;
	}
#endif
	tmp = errno;
	if (errno == EMFILE)
		if (sq_closeone())
			goto again;
	errno = tmp;
	return (fd);
}

#ifndef IP_OPT_BUF_SIZE
/* arbitrary size */
#define IP_OPT_BUF_SIZE 50
#endif

static void
stream_accept(evContext lev, void *uap, int rfd,
	      const void *lav, int lalen,
	      const void *rav, int ralen)
{
	interface *ifp = uap;
	struct qstream *sp;
	struct iovec iov;
	ISC_SOCKLEN_T len;
	int n;
	const int on = 1;
#ifdef IP_OPTIONS	/* XXX */
	u_char ip_opts[IP_OPT_BUF_SIZE];
#endif
	const struct sockaddr_in *la, *ra;

	UNUSED(lalen);
	UNUSED(ralen);

	la = (const struct sockaddr_in *)lav;
	ra = (const struct sockaddr_in *)rav;

	INSIST(ifp != NULL);

#ifdef F_DUPFD
	/*
	 * Leave a space for stdio to work in.
	 */
	if (rfd >= 0 && rfd <= 20) {
		int new, tmp;
		new = fcntl(rfd, F_DUPFD, 20);
		tmp = errno;
		if (new == -1)
			ns_notice(ns_log_default,
				  "fcntl(rfd, F_DUPFD, 20): %s",
				  strerror(errno));
		close(rfd);
		errno = tmp;
		rfd = new;
	}
#endif

	if (rfd < 0) {
		switch (errno) {
		case EINTR:
		case EAGAIN:
#if (EWOULDBLOCK != EAGAIN)
		case EWOULDBLOCK:
#endif
		case ECONNABORTED:
#ifdef EPROTO
		case EPROTO:
#endif
		case EHOSTUNREACH:
		case EHOSTDOWN:
		case ENETUNREACH:
		case ENETDOWN:
		case ECONNREFUSED:
#ifdef ENONET
		case ENONET:
#endif
			/*
			 * These errors are expected and harmless, so
			 * we ignore them.
			 */
			return;
		case EBADF:
		case ENOTSOCK:
		case EFAULT:
			/*
			 * If one these happens, we're broken.
			 */
			ns_panic(ns_log_default, 1, "accept: %s",
				 strerror(errno));
		case EMFILE:
			/*
			 * If we're out of file descriptors, find the least
			 * busy fd and close it.  Then we'll return to the
			 * eventlib which will call us right back.
			 */
			if (streamq) {
				(void)sq_closeone();
				return;
			}
			/* fall through */
		default:
			/*
			 * Either we got an error we didn't expect, or we
			 * got EMFILE and didn't have anything left to close.
			 * Log it and press on.
			 */
			ns_info(ns_log_default, "accept: %s", strerror(errno));
			return;
		}
	}

	/* Condition the socket. */

#ifndef CANNOT_SET_SNDBUF
	if (setsockopt(rfd, SOL_SOCKET, SO_SNDBUF,
		       (const char*)&sbufsize, sizeof sbufsize) < 0) {
		ns_info(ns_log_default, "setsockopt(rfd, SO_SNDBUF, %d): %s",
			sbufsize, strerror(errno));
		(void) close(rfd);
		return;
	}
#endif
	if (setsockopt(rfd, SOL_SOCKET, SO_KEEPALIVE,
		       (const char *)&on, sizeof on) < 0) {
		ns_info(ns_log_default, "setsockopt(rfd, KEEPALIVE): %s",
			strerror(errno));
		(void) close(rfd);
		return;
	}

#ifdef USE_FIONBIO_IOCTL
	if (ioctl(ifp->dfd, FIONBIO, (char *) &on) == -1) {
		ns_info(ns_log_default, "ioctl(rfd, FIONBIO): %s",
			strerror(errno));
		(void) close(rfd);
		return;
	}
#else
	if ((n = fcntl(rfd, F_GETFL, 0)) == -1) {
		ns_info(ns_log_default, "fcntl(rfd, F_GETFL): %s",
			strerror(errno));
		(void) close(rfd);
		return;
	}
	if (fcntl(rfd, F_SETFL, n|PORT_NONBLOCK) == -1) {
		ns_info(ns_log_default, "fcntl(rfd, NONBLOCK): %s",
			strerror(errno));
		(void) close(rfd);
		return;
	}
#endif

	/*
	 * We don't like IP options.  Turn them off if the connection came in
	 * with any.  log this event since it usually indicates a security
	 * problem.
	 */
#if defined(IP_OPTIONS)		/* XXX */
	len = sizeof ip_opts;
	if (getsockopt(rfd, IPPROTO_IP, IP_OPTIONS,
		       (char *)ip_opts, &len) < 0) {
		ns_info(ns_log_default, "getsockopt(rfd, IP_OPTIONS): %s",
			strerror(errno));
		(void) close(rfd);
		return;
	}
	if (len != 0) {
		nameserIncr(ra->sin_addr, nssRcvdOpts);
		if (!haveComplained(ina_ulong(ra->sin_addr),
				    (u_long)"rcvd ip options")) {
			ns_info(ns_log_default,
				"rcvd IP_OPTIONS from %s (ignored)",
				sin_ntoa(*ra));
		}
		if (setsockopt(rfd, IPPROTO_IP, IP_OPTIONS, NULL, 0) < 0) {
			ns_info(ns_log_default, "setsockopt(!IP_OPTIONS): %s",
				strerror(errno));
			(void) close(rfd);
		}
	}
#endif

	/* Create and populate a qsp for this socket. */
	if ((sp = sq_add()) == NULL) {
		(void) close(rfd);
		return;
	}
	sp->s_rfd = rfd;	/* stream file descriptor */
	gettime(&tt);
	sp->s_time = tt.tv_sec;	/* last transaction time */
	sp->s_from = *ra;	/* address to respond to */
	sp->s_ifp = ifp;
	INSIST(sizeof sp->s_temp >= INT16SZ);
	iov = evConsIovec(sp->s_temp, INT16SZ);
	if (evRead(lev, rfd, &iov, 1, stream_getlen, sp, &sp->evID_r) == -1) {
		ns_error(ns_log_default, "evRead(fd %d): %s",
			 rfd, strerror(errno));
		sq_remove(sp);
		return;
	}
	sp->flags |= STREAM_READ_EV;
	ns_debug(ns_log_default, 1, "IP/TCP connection from %s (fd %d)",
		 sin_ntoa(sp->s_from), rfd);
}

int
tcp_send(struct qinfo *qp) {
	struct qstream *sp;
	struct sockaddr_in src;
	int on = 1, n;
	int fd;
	
	ns_debug(ns_log_default, 1, "tcp_send");
	if ((fd = ns_socket(AF_INET, SOCK_STREAM, PF_UNSPEC)) == -1)
		return (SERVFAIL);
	if (fd > evHighestFD(ev)) {
		close(fd);
		return (SERVFAIL);
	}
	if ((sp = sq_add()) == NULL) {
		close(fd);
		return (SERVFAIL);
	}
	sp->s_rfd = fd;
	if (setsockopt(sp->s_rfd, SOL_SOCKET, SO_REUSEADDR,
		       (char*)&on, sizeof(on)) < 0)
		ns_info(ns_log_default,
			"tcp_send: setsockopt(SO_REUSEADDR): %s",
			strerror(errno));
#ifdef SO_REUSEPORT
	if (setsockopt(sp->s_rfd, SOL_SOCKET, SO_REUSEPORT,
		       (char*)&on, sizeof(on)) < 0)
		ns_info(ns_log_default,
			"tcp_send: setsockopt(SO_REUSEPORT): %s",
			strerror(errno));
#endif
	src = server_options->query_source;
	src.sin_port = htons(0);
	if (bind(sp->s_rfd, (struct sockaddr *)&src, sizeof(src)) < 0)
		ns_info(ns_log_default, "tcp_send: bind(query_source): %s",
			strerror(errno));
	if (fcntl(sp->s_rfd, F_SETFD, 1) < 0) {
		sq_remove(sp);
		return (SERVFAIL);
	}
#ifdef USE_FIONBIO_IOCTL
	if (ioctl(sp->s_rfd, FIONBIO, (char *) &on) == -1) {
		sq_remove(sp);
		return (SERVFAIL);
	}
#else
	if ((n = fcntl(sp->s_rfd, F_GETFL, 0)) == -1) {
		sq_remove(sp);
		return (SERVFAIL);
	}
	if (fcntl(sp->s_rfd, F_SETFL, n|PORT_NONBLOCK) == -1) {
		sq_remove(sp);
		return (SERVFAIL);
	}
#endif
	if (sq_openw(sp, qp->q_msglen + INT16SZ) == -1) {
		sq_remove(sp);
		return (SERVFAIL);
	}
	if (sq_write(sp, qp->q_msg, qp->q_msglen) == -1) {
		sq_remove(sp);
		return (SERVFAIL);
	}

	if (setsockopt(sp->s_rfd, SOL_SOCKET, SO_KEEPALIVE,
		       (char*)&on, sizeof(on)) < 0)
		ns_info(ns_log_default,
			"tcp_send: setsockopt(SO_KEEPALIVE): %s",
			strerror(errno));
	gettime(&tt);
	sp->s_size = -1;
	sp->s_time = tt.tv_sec;	/* last transaction time */
	sp->s_refcnt = 1;
	sp->flags |= STREAM_DONE_CLOSE;
	sp->s_from = qp->q_addr[qp->q_curaddr].ns_addr;
	if (evConnect(ev, sp->s_rfd, &sp->s_from, sizeof(sp->s_from),
		      stream_send, sp, &sp->evID_c) == -1) {
		sq_remove(sp);
		return (SERVFAIL);
	}
	sp->flags |= STREAM_CONNECT_EV;
	return (NOERROR);
}

static void
stream_send(evContext lev, void *uap, int fd, const void *la, int lalen,
	    const void *ra, int ralen) {
	struct qstream *sp = uap;

	UNUSED(lev);
	UNUSED(la);
	UNUSED(lalen);
	UNUSED(ra);
	UNUSED(ralen);

	ns_debug(ns_log_default, 1, "stream_send");

	sp->flags &= ~STREAM_CONNECT_EV;

	if (fd == -1) {
		/* connect failed */
		sq_remove(sp);
		return;
	}
	if (evSelectFD(ev, sp->s_rfd, EV_WRITE,
		       stream_write, sp, &sp->evID_w) < 0) {
		sq_remove(sp);
		return;
	}
	sp->flags |= STREAM_WRITE_EV;
}

static void
stream_write(evContext ctx, void *uap, int fd, int evmask) {
	struct qstream *sp = uap;
	struct iovec iov;

	ns_debug(ns_log_default, 1, "stream_write");
	INSIST(evmask & EV_WRITE);
	INSIST(fd == sp->s_rfd);
	if (sq_dowrite(sp) < 0) {
		sq_remove(sp);
		return;
	}
	if (sp->s_wbuf_free != sp->s_wbuf_send)
		return;

	if (sp->s_wbuf) {
		memput(sp->s_wbuf, sp->s_wbuf_end - sp->s_wbuf);
		sp->s_wbuf_send = sp->s_wbuf_free = NULL;
		sp->s_wbuf_end = sp->s_wbuf = NULL;
	}
	(void) evDeselectFD(ev, sp->evID_w);
	sp->flags &= ~STREAM_WRITE_EV;
	sp->s_refcnt = 0;
	iov = evConsIovec(sp->s_temp, INT16SZ);
	if (evRead(ctx, fd, &iov, 1, stream_getlen, sp, &sp->evID_r) == -1) {
		ns_error(ns_log_default, "evRead(fd %d): %s",
			 fd, strerror(errno));
		sq_remove(sp);
		return;
	}
	sp->flags |= STREAM_READ_EV;
}

static void
stream_getlen(evContext lev, void *uap, int fd, int bytes) {
	struct qstream *sp = uap;
	struct iovec iov;

	UNUSED(fd);

	sp->flags &= ~STREAM_READ_EV;
	if (bytes != INT16SZ) {
		/*
		 * bytes == 0 is normal EOF; see if something unusual 
		 * happened.
		 */
		if (bytes < 0) {
			/*
			 * ECONNRESET happens frequently and is not worth
			 * logging.
			 */
			if (errno != ECONNRESET)
				ns_info(ns_log_default,
					"stream_getlen(%s): %s",
					sin_ntoa(sp->s_from), strerror(errno));
		} else if (bytes != 0)
			ns_error(ns_log_default,
				 "stream_getlen(%s): unexpected byte count %d",
				sin_ntoa(sp->s_from), bytes);
		sq_remove(sp);
		return;
	}

	/*
	 * Unpack the size, allocate memory for the query.  This is
	 * tricky since in a low memory situation with possibly very
	 * large (64KB) queries, we want to make sure we can read at
	 * least the header since we need it to send back a SERVFAIL
	 * (owing to the out-of-memory condition).
	 */
	sp->s_size = ns_get16(sp->s_temp);
	ns_debug(ns_log_default, 5, "stream message: %d bytes", sp->s_size);
	if (sp->s_size < HFIXEDSZ) {
		ns_error(ns_log_default,
			 "stream_getlen(%s): request too small",
			 sin_ntoa(sp->s_from));
		sq_remove(sp);
		return;
	}

	if (!(sp->flags & STREAM_MALLOC)) {
		sp->s_bufsize = 64*1024-1; /* maximum tcp message size */
		sp->s_buf = (u_char *)memget(sp->s_bufsize);
		if (sp->s_buf != NULL)
			sp->flags |= STREAM_MALLOC;
		else {
			sp->s_buf = sp->s_temp;
			sp->s_bufsize = HFIXEDSZ;
		}
	}

	iov = evConsIovec(sp->s_buf, (sp->s_size <= sp->s_bufsize) ?
				     sp->s_size : sp->s_bufsize);
	if (evRead(lev, sp->s_rfd, &iov, 1, stream_getmsg, sp, &sp->evID_r)
	    == -1) {
		ns_error(ns_log_default, "evRead(fd %d): %s",
			 sp->s_rfd, strerror(errno));
		sq_remove(sp);
		return;
	}
	sp->flags |= STREAM_READ_EV;
}

static void
stream_getmsg(evContext lev, void *uap, int fd, int bytes) {
	struct qstream *sp = uap;

	UNUSED(lev);
	UNUSED(fd);

	sp->flags &= ~STREAM_READ_EV;
	if (bytes == -1) {
		ns_info(ns_log_default, "stream_getmsg(%s): %s",
			sin_ntoa(sp->s_from), strerror(errno));
		sq_remove(sp);
		return;
	}

	gettime(&tt);
	sp->s_time = tt.tv_sec;

	if (ns_wouldlog(ns_log_default,5)) {
		ns_debug(ns_log_default, 5,
			 "sp %p rfd %d size %d time %ld next %p",
			 sp, sp->s_rfd, sp->s_size, (long)sp->s_time,
			 sp->s_next);
		ns_debug(ns_log_default, 5, "\tbufsize %d bytes %d", sp->s_bufsize,
			 bytes);
	}

	/*
	 * Do we have enough memory for the query?  If not, and if we have a
	 * query id, then we will send a SERVFAIL error back to the client.
	 */
	if (bytes != sp->s_size) {
		HEADER *hp = (HEADER *)sp->s_buf;

		hp->qr = 1;
		hp->ra = (NS_OPTION_P(OPTION_NORECURSE) == 0);
		hp->ancount = htons(0);
		hp->qdcount = htons(0);
		hp->nscount = htons(0);
		hp->arcount = htons(0);
		hp->rcode = SERVFAIL;
		writestream(sp, sp->s_buf, HFIXEDSZ);
		sp->flags |= STREAM_DONE_CLOSE;
		return;
	}

	nameserIncr(sp->s_from.sin_addr, nssRcvdTCP);
	sq_query(sp);
	dispatch_message(sp->s_buf, bytes, sp->s_bufsize, sp, sp->s_from, -1,
			 sp->s_ifp);
}

static void
datagram_read(evContext lev, void *uap, int fd, int evmask) {
	interface *ifp = uap;
	struct sockaddr_in from;
	ISC_SOCKLEN_T from_len = sizeof from;
	int n, nudp;
	union {
		HEADER h;			/* Force alignment of 'buf'. */
		u_char buf[EDNS_MESSAGE_SZ+1];
	} u;

	UNUSED(lev);
	UNUSED(evmask);

	tt = evTimeVal(evNowTime());
	nudp = 0;

 more:
	n = recvfrom(fd, (char *)u.buf, sizeof u.buf, 0,
		     (struct sockaddr *)&from, &from_len);

	if (n < 0) {
		switch (errno) {
		case EINTR:
		case EAGAIN:
#if (EWOULDBLOCK != EAGAIN)
		case EWOULDBLOCK:
#endif
		case EHOSTUNREACH:
		case EHOSTDOWN:
		case ENETUNREACH:
		case ENETDOWN:
		case ECONNREFUSED:
#ifdef ENONET
		case ENONET:
#endif
			/*
			 * These errors are expected and harmless, so we
			 * ignore them.
			 */
			return;
		default:
			/*
			 * An error we don't expect.  Log it and press
			 * on.
			 */
			ns_info(ns_log_default, "recvfrom: %s",
				strerror(errno));
			return;
		}
	}

	/* Handle bogosity on systems that need it. */
	if (n == 0)
		return;

	if (ns_wouldlog(ns_log_default, 1)) {
		ns_debug(ns_log_default, 1, "datagram from %s, fd %d, len %d",
			 sin_ntoa(from), fd, n);
	}

	if (n > EDNS_MESSAGE_SZ) {
		/*
		 * The message is too big.  It's probably a response to
		 * one of our questions, so we truncate it and press on.
		 */
		n = trunc_adjust(u.buf, EDNS_MESSAGE_SZ, EDNS_MESSAGE_SZ);
		ns_debug(ns_log_default, 1, "truncated oversize UDP packet");
	}

	dispatch_message(u.buf, n, EDNS_MESSAGE_SZ, NULL, from, fd, ifp);
	if (++nudp < nudptrans)
		goto more;
}

static void
savedg_waitfunc(evContext ctx, void *uap, const void *tag) {
	savedg *dg = (savedg *)uap;

	UNUSED(ctx);
	UNUSED(tag);

	if (!EMPTY(iflist) && HEAD(iflist)->gen == dg->gen) {
		u_char buf[EDNS_MESSAGE_SZ];

		memcpy(buf, dg->buf, dg->buflen);
		dispatch_message(buf, dg->buflen, sizeof buf, NULL,
				 dg->from, dg->dfd, dg->ifp);
	}
	memput(dg->buf, dg->buflen);
	memput(dg, sizeof *dg);
}

static void
dispatch_message(u_char *msg, int msglen, int buflen, struct qstream *qsp,
		 struct sockaddr_in from, int dfd, interface *ifp)
{
	HEADER *hp = (HEADER *)msg;

	if (msglen < HFIXEDSZ) {
		ns_debug(ns_log_default, 1, "dropping undersize message");
		if (qsp) {
			qsp->flags |= STREAM_DONE_CLOSE;
			sq_done(qsp);
		}
		return;
	}

	if (server_options->blackhole_acl != NULL && 
	    ip_match_address(server_options->blackhole_acl,
			     from.sin_addr) == 1) {
		ns_debug(ns_log_default, 1,
			 "dropping blackholed %s from %s",
			 hp->qr ? "response" : "query",
			 sin_ntoa(from));
		if (qsp) {
			qsp->flags |= STREAM_DONE_CLOSE;
			sq_done(qsp);
		}
		return;
	}

	/* Drop UDP packets from port zero.  They are invariable forged. */
	if (qsp == NULL && ntohs(from.sin_port) == 0) {
		ns_notice(ns_log_security,
			  "dropping source port zero packet from %s",
			  sin_ntoa(from));
		return;
	}

	if (hp->qr) {
		ns_resp(msg, msglen, from, qsp);
		if (qsp)
			sq_done(qsp);
		/* Now is a safe time for housekeeping. */
		if (needs_prime_cache)
			prime_cache();
	} else if (ifp != NULL)
		ns_req(msg, msglen, buflen, qsp, from, dfd);
	else {
		ns_notice(ns_log_security,
			  "refused query on non-query socket from %s",
			  sin_ntoa(from));
		if (qsp) {
			qsp->flags |= STREAM_DONE_CLOSE;
			sq_done(qsp);
		}
		/* XXX Send refusal here. */
	}
}

void
getnetconf(int periodic_scan) {
	struct ifconf ifc;
	struct ifreq ifreq;
	struct in_addr ina;
	interface *ifp;
	char *buf, *cp, *cplim;
	static int bufsiz = 4095;
	time_t my_generation = time(NULL);
	int s, cpsize, n;
	int found;
	listen_info li;
	ip_match_element ime;
	u_char *mask_ptr;
	struct in_addr mask;
#ifdef TRUCLUSTER5
	struct sockaddr clua_addr;
	int clua_cnt, clua_tot;
#endif
	int clua_buf;

	if (iflist_initialized) {
		if (iflist_dont_rescan)
			return;
	} else {
		INIT_LIST(iflist);
		iflist_initialized = 1;
	}

	ns_debug(ns_log_default, 1, "getnetconf(generation %lu)",
		 (u_long)my_generation);

	/* Get interface list from system. */
	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		if (!periodic_scan)
			ns_panic(ns_log_default, 1, "socket(SOCK_RAW): %s",
				 strerror(errno));
		ns_error(ns_log_default, "socket(SOCK_RAW): %s",
			 strerror(errno));
		return;
	}

	if (local_addresses != NULL)
		free_ip_match_list(local_addresses);
	local_addresses = new_ip_match_list();
	if (local_networks != NULL)
		free_ip_match_list(local_networks);
	local_networks = new_ip_match_list();

#ifdef TRUCLUSTER5
	/* Find out how many cluster aliases there are */
	clua_cnt = 0;
	clua_tot = 0;
	while (clua_getaliasaddress(&clua_addr, &clua_cnt) == CLUA_SUCCESS)
		clua_tot ++;
	clua_buf = clua_tot * sizeof(ifreq);
#else
	clua_buf = 0;
#endif

	for (;;) {
		buf = memget(bufsiz + clua_buf);
		if (!buf)
			ns_panic(ns_log_default, 1, "memget(interface)");
		ifc.ifc_len = bufsiz;
		ifc.ifc_buf = buf;
#ifdef IRIX_EMUL_IOCTL_SIOCGIFCONF
		/*
		 * This is a fix for IRIX OS in which the call to ioctl with
		 * the flag SIOCGIFCONF may not return an entry for all the
		 * interfaces like most flavors of Unix.
		 */
		if (emul_ioctl(&ifc) >= 0)
			break;
#else
		if ((n = ioctl(s, SIOCGIFCONF, (char *)&ifc)) != -1) {
			/*
			 * Some OS's just return what will fit rather
			 * than set EINVAL if the buffer is too small
			 * to fit all the interfaces in.  If 
			 * ifc.ifc_len is too near to the end of the
			 * buffer we will grow it just in case and
			 * retry.
			 */
			if ((int)(ifc.ifc_len + 2 * sizeof(ifreq)) < bufsiz)
				break;
		}
#endif
		if ((n == -1) && errno != EINVAL)
			ns_panic(ns_log_default, 1,
				"get interface configuration: %s",
				strerror(errno));

		if (bufsiz > 1000000)
			ns_panic(ns_log_default, 1,
				"get interface configuration: maximum buffer size exceeded");
		memput(buf, bufsiz + clua_buf);
		bufsiz += 4096;
	}

#ifdef TRUCLUSTER5
	/* Get the cluster aliases and create interface entries for them */
	clua_cnt = 0;
	while (clua_tot--) {
		memset(&ifreq, 0, sizeof (ifreq));
		if (clua_getaliasaddress(&ifreq.ifr_addr, &clua_cnt) !=
		    CLUA_SUCCESS)
			/*
			 * It is possible the count of aliases has changed; if
			 * it has increased, they won't be found this pass.
			 * If has decreased, stop the loop early. */
			break;
		strcpy(ifreq.ifr_name, "lo0");
		memcpy(ifc.ifc_buf + ifc.ifc_len, &ifreq, sizeof (ifreq));
		ifc.ifc_len += sizeof (ifreq);
		bufsiz += sizeof (ifreq);
	}
#endif

	ns_debug(ns_log_default, 2, "getnetconf: SIOCGIFCONF: ifc_len = %d",
		 ifc.ifc_len);

	/* Parse system's interface list and open some sockets. */
	cplim = buf + ifc.ifc_len;    /* skip over if's with big ifr_addr's */
	for (cp = buf; cp < cplim; cp += cpsize) {
		memcpy(&ifreq, cp, sizeof ifreq);
#ifdef HAVE_SA_LEN
#ifdef FIX_ZERO_SA_LEN
		if (ifreq.ifr_addr.sa_len == 0)
			ifreq.ifr_addr.sa_len = 16;
#endif
#ifdef HAVE_MINIMUM_IFREQ
		ns_debug(ns_log_default, 2, "%s sa_len = %d",
			 ifreq.ifr_name, (int)ifreq.ifr_addr.sa_len);
		cpsize = sizeof ifreq;
		if (ifreq.ifr_addr.sa_len > sizeof (struct sockaddr))
			cpsize += (int)ifreq.ifr_addr.sa_len -
				(int)(sizeof (struct sockaddr));
#else
		cpsize = sizeof ifreq.ifr_name + ifreq.ifr_addr.sa_len;
#endif /* HAVE_MINIMUM_IFREQ */
#elif defined SIOCGIFCONF_ADDR
		cpsize = sizeof ifreq;
#else
		cpsize = sizeof ifreq.ifr_name;
		if (ioctl(s, SIOCGIFADDR, (char *)&ifreq) < 0) {
			ns_notice(ns_log_default,
				  "get interface addr (%s): %s",
				  ifreq.ifr_name, strerror(errno));
			continue;
		}
#endif
		if (ifreq.ifr_addr.sa_family != AF_INET) {
			ns_debug(ns_log_default, 2, 
				 "getnetconf: %s AF %d != INET",
				 ifreq.ifr_name, ifreq.ifr_addr.sa_family);
			continue;
		}
		ina = ina_get((u_char *)&((struct sockaddr_in *)
					   &ifreq.ifr_addr)->sin_addr);
		ns_debug(ns_log_default, 1,
			 "getnetconf: considering %s [%s]",
			 ifreq.ifr_name, inet_ntoa(ina));
		/*
		 * Don't test IFF_UP, packets may still be received at this
		 * address if any other interface is up.
		 */
		if (ina_hlong(ina) == INADDR_ANY) {
			ns_debug(ns_log_default, 2,
				 "getnetconf: INADDR_ANY, ignoring.");
			continue;
		}

		INSIST(server_options != NULL);
		INSIST(server_options->listen_list != NULL);

		found=0;
		for (li = server_options->listen_list->first;
		     li != NULL;
		     li = li->next) {
			if (ip_match_address(li->list, ina) > 0) {
				found++;
				/* 
				 * Look for an already existing source
				 * interface address/port pair.
				 * This happens mostly when reinitializing.
				 * Also, if the machine has multiple point to
				 * point interfaces, then the local address
				 * may appear more than once.
				 */
				ifp = if_find(ina, li->port, 0);
				if (ifp != NULL) {
					ns_debug(ns_log_default, 1,
					  "dup interface addr [%s].%u (%s)",
						 inet_ntoa(ina),
						 ntohs(li->port),
						 ifreq.ifr_name);
					ifp->gen = my_generation;
					continue;
				}

				ifp = (interface *)memget(sizeof *ifp);
				if (!ifp)
					ns_panic(ns_log_default, 1,
						 "memget(interface)");
				memset(ifp, 0, sizeof *ifp);
				INIT_LINK(ifp, link);
				APPEND(iflist, ifp, link);
				ifp->addr = ina;
				ifp->port = li->port;
				ifp->gen = my_generation;
				ifp->flags = 0;
				ifp->dfd = -1;
				ifp->sfd = -1;
				if (opensocket_d(ifp) < 0 ||
				    opensocket_s(ifp) < 0) {
					dq_remove(ifp);
					found = 0;
					break;
				}
				ns_info(ns_log_default,
					"listening on [%s].%u (%s)",
					inet_ntoa(ina), ntohs(li->port),
					ifreq.ifr_name);
			}
		}
		if (!found)
			ns_debug(ns_log_default, 1,
				 "not listening on addr [%s] (%s)",
				 inet_ntoa(ina), ifreq.ifr_name);

		/*
		 * Add this interface's address to the list of local
		 * addresses if we haven't added it already.
		 */
		if (ip_match_address(local_addresses, ina) < 0) {
			ime = new_ip_match_pattern(ina, 32);
			add_to_ip_match_list(local_addresses, ime);
		}

		/*
		 * Get interface flags.
		 */
		if (ioctl(s, SIOCGIFFLAGS, (char *)&ifreq) < 0) {
			ns_notice(ns_log_default, "get interface flags: %s",
				  strerror(errno));
			continue;
		}

		if ((ifreq.ifr_flags & IFF_POINTOPOINT)) {
			/*
			 * The local network for a PPP link is just the
			 * two ends of the link, so for each endpoint we
			 * add a pattern that will only match the endpoint.
			 */
			if (ioctl(s, SIOCGIFDSTADDR, (char *)&ifreq) < 0) {
				ns_notice(ns_log_default, "get dst addr: %s",
					  strerror(errno));
				continue;
			}

			mask.s_addr = htonl(INADDR_BROADCAST);

			/*
			 * Our end.
			 *
			 * Only add it if we haven't seen it before.
			 */
			if (ip_match_network(local_networks, ina, mask) < 0) {
				ime = new_ip_match_pattern(ina, 32);
				add_to_ip_match_list(local_networks, ime);
			}

			/*
			 * The other end.
			 */
			ina = ((struct sockaddr_in *)
			       &ifreq.ifr_addr)->sin_addr;
			/*
			 * Only add it if we haven't seen it before.
			 */
			if (ip_match_network(local_networks, ina, mask) < 0) {
				ime = new_ip_match_pattern(ina, 32);
				add_to_ip_match_list(local_networks, ime);
			}
		} else {
			/*
			 * Add this interface's network and netmask to the
			 * list of local networks.
			 */

#ifdef SIOCGIFNETMASK	/* XXX */
			if (ioctl(s, SIOCGIFNETMASK, (char *)&ifreq) < 0) {
				ns_notice(ns_log_default, "get netmask: %s",
					  strerror(errno));
				continue;
			}
			/*
			 * Use ina_get because the ifreq structure might not
			 * be aligned.
			 */
			mask_ptr = (u_char *)
			  &((struct sockaddr_in *)&ifreq.ifr_addr)->sin_addr;
			mask = ina_get(mask_ptr);
#else
			mask = net_mask(ina);
#endif

			ina.s_addr &= mask.s_addr;   /* make network address */

			/*
			 * Only add it if we haven't seen it before.
			 */
			if (ip_match_network(local_networks, ina, mask) < 0) {
				ime = new_ip_match_mask(ina, mask);
				add_to_ip_match_list(local_networks, ime);
			}
		}
	}
	close(s);
	memput(buf, bufsiz);

	ns_debug(ns_log_default, 7, "local addresses:");
	dprint_ip_match_list(ns_log_default, local_addresses, 2, "", "");
	ns_debug(ns_log_default, 7, "local networks:");
	dprint_ip_match_list(ns_log_default, local_networks, 2, "", "");

	/*
	 * now go through the iflist and delete anything that
	 * does not have the current generation number.  this is
	 * how we catch interfaces that go away or change their
	 * addresses.  note that 0.0.0.0 is the wildcard element
	 * and should never be deleted by this code.
	 */
	dq_remove_gen(my_generation);

	if (EMPTY(iflist))
		ns_warning(ns_log_default, "not listening on any interfaces");
}

/* opensocket_d(ifp)
 *	Open datagram socket bound to interface address.
 * Returns:
 *	0 on success.
 *	-1 on failure.
 */
static int
opensocket_d(interface *ifp) {
	struct sockaddr_in nsa;
	const int on = 1;
	ISC_SOCKLEN_T m;
	int n;

	memset(&nsa, 0, sizeof nsa);
	nsa.sin_family = AF_INET;
	nsa.sin_addr = ifp->addr;
	nsa.sin_port = ifp->port;

	if ((ifp->dfd = ns_socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		ns_error(ns_log_default, "socket(SOCK_DGRAM): %s",
			 strerror(errno));
		return (-1);
	}
	if (ifp->dfd > evHighestFD(ev)) {
		ns_error(ns_log_default, "socket too high: %d", ifp->dfd);
		close(ifp->dfd);
		return (-1);
	}
#ifdef USE_FIONBIO_IOCTL
	if (ioctl(ifp->dfd, FIONBIO, (char *) &on) == -1) {
		ns_info(ns_log_default, "ioctl(ifp->dfd, FIONBIO): %s",
			strerror(errno));
		(void) close(ifp->dfd);
		return (-1);
	}
#else
	if ((n = fcntl(ifp->dfd, F_GETFL, 0)) == -1) {
		ns_info(ns_log_default, "fcntl(ifp->dfd, F_GETFL): %s",
			strerror(errno));
		(void) close(ifp->dfd);
		return (-1);
	}
	if (fcntl(ifp->dfd, F_SETFL, n|PORT_NONBLOCK) == -1) {
		ns_info(ns_log_default, "fcntl(ifp->dfd, NONBLOCK): %s",
			strerror(errno));
		(void) close(ifp->dfd);
		return (-1);
	}
#endif
	if (fcntl(ifp->dfd, F_SETFD, 1) < 0) {
		ns_error(ns_log_default, "F_SETFD: %s", strerror(errno));
		close(ifp->dfd);
		return (-1);
	}
	ns_debug(ns_log_default, 1, "ifp->addr %s d_dfd %d",
		 sin_ntoa(nsa), ifp->dfd);
	if (setsockopt(ifp->dfd, SOL_SOCKET, SO_REUSEADDR,
		       (const char *)&on, sizeof(on)) != 0) {
		ns_notice(ns_log_default, "setsockopt(REUSEADDR): %s",
			  strerror(errno));
		/* XXX press on regardless, this is not too serious. */
	}
#ifdef SO_RCVBUF	/* XXX */
	m = sizeof n;
	if ((getsockopt(ifp->dfd, SOL_SOCKET, SO_RCVBUF, (char*)&n, &m) >= 0)
	    && (m == sizeof n)
	    && (n < drbufsize)) {
		(void) setsockopt(ifp->dfd, SOL_SOCKET, SO_RCVBUF,
				  (const char *)&drbufsize, sizeof drbufsize);
	}
#endif /* SO_RCVBUF */
#ifndef CANNOT_SET_SNDBUF
	if (setsockopt(ifp->dfd, SOL_SOCKET, SO_SNDBUF,
		       (const char*)&dsbufsize, sizeof dsbufsize) < 0) {
		ns_info(ns_log_default,
			"setsockopt(dfd=%d, SO_SNDBUF, %d): %s",
			ifp->dfd, dsbufsize, strerror(errno));
		/* XXX press on regardless, this is not too serious. */
	}
#endif
#ifdef SO_BSDCOMPAT
	if (setsockopt(ifp->dfd, SOL_SOCKET, SO_BSDCOMPAT,
		      (char*)&on, sizeof on) < 0) {
		ns_info(ns_log_default,
			"setsockopt(dfd=%d, SO_BSDCOMPAT): %s",
			ifp->dfd, strerror(errno));
	}
#endif
	if (bind(ifp->dfd, (struct sockaddr *)&nsa, sizeof nsa)) {
		ns_error(ns_log_default, "bind(dfd=%d, %s): %s",
			 ifp->dfd, sin_ntoa(nsa), strerror(errno));
		return (-1);
	}
	if (evSelectFD(ev, ifp->dfd, EV_READ, datagram_read, ifp,
		       &ifp->evID_d) == -1) {
		ns_error(ns_log_default, "evSelectFD(dfd=%d): %s",
			 ifp->dfd, strerror(errno));
		return (-1);
	}
	ifp->flags |= INTERFACE_FILE_VALID;
	return (0);
}

static int
drain_rcvbuf(evContext ctx, interface *ifp, int fd, int *mread, int *mstore) {
	int drop = 0;

	drop = 0;
	for (; *mread > 0; (*mread)--) {
		union {
			HEADER h;
			u_char buf[EDNS_MESSAGE_SZ+1];
		} u;
		struct sockaddr_in from;
		ISC_SOCKLEN_T from_len = sizeof from;
		savedg *dg;
		int n;

		n = recvfrom(fd, (char *)u.buf, sizeof u.buf, 0,
			     (struct sockaddr *)&from, &from_len);
		if (n <= 0)
			break;		/* Socket buffer assumed empty. */
		drop++;			/* Pessimistic assumption. */
		if (n > EDNS_MESSAGE_SZ)
			continue;	/* Oversize message - EDNS0 needed. */
		if (from.sin_family != AF_INET)
			continue;	/* Not IPv4 - IPv6 needed. */
		if (u.h.opcode == ns_o_query && u.h.qr == 0)
			continue;	/* Query - what we're here to axe. */
		if (*mstore <= 0)
			continue;	/* Reached storage quota, ignore. */
		if ((dg = memget(sizeof *dg)) == NULL)
			continue;	/* No memory - probably fatal. */
		if ((dg->buf = memget(n)) == NULL) {
			memput(dg, sizeof *dg);
			continue;	/* No memory - probably fatal. */
		}
		dg->from = from;
		dg->dfd = fd;
		dg->ifp = ifp;
		dg->gen = ifp->gen;
		dg->buflen = n;
		memcpy(dg->buf, u.buf, n);
		if (evWaitFor(ctx, (void *)drain_all_rcvbuf, savedg_waitfunc,
			      dg, NULL) < 0)
		{
			memput(dg->buf, dg->buflen);
			memput(dg, sizeof *dg);
			continue;	/* No memory - probably fatal. */
		}
		drop--;			/* Pessimism was inappropriate. */
		(*mstore)--;
	}
	return (drop);
}

static int
drain_all_rcvbuf(evContext ctx) {
	interface *ifp;
	int mread = MAX_SYNCDRAIN;
	int mstore = MAX_SYNCSTORE;
	int drop = 0;

	for (ifp = HEAD(iflist); ifp != NULL; ifp = NEXT(ifp, link))
		if (ifp->dfd != -1)
			drop += drain_rcvbuf(ctx, ifp, ifp->dfd,
					     &mread, &mstore);
	if (mstore < MAX_SYNCSTORE)
		INSIST_ERR(evDo(ctx, (void *)drain_all_rcvbuf) != -1);
	return (drop);
}

/* opensocket_s(ifp)
 *	Open stream (listener) socket bound to interface address.
 * Returns:
 *	0 on success.
 *	-1 on failure.
 */
static int
opensocket_s(interface *ifp) {
	struct sockaddr_in nsa;
	const int on = 1;
	int n;

	memset(&nsa, 0, sizeof nsa);
	nsa.sin_family = AF_INET;
	nsa.sin_addr = ifp->addr;
	nsa.sin_port = ifp->port;

	/*
	 * Open stream (listener) port.
	 */
	n = 0;
 again:
	if ((ifp->sfd = ns_socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		ns_error(ns_log_default, "socket(SOCK_STREAM): %s",
			 strerror(errno));
		return (-1);
	}
	if (ifp->sfd > evHighestFD(ev)) {
		ns_error(ns_log_default, "socket too high: %d", ifp->sfd);
		close(ifp->sfd);
		return (-1);
	}
	if (fcntl(ifp->sfd, F_SETFD, 1) < 0) {
		ns_error(ns_log_default, "F_SETFD: %s", strerror(errno));
		close(ifp->sfd);
		return (-1);
	}
	if (setsockopt(ifp->sfd, SOL_SOCKET, SO_REUSEADDR,
		       (const char *)&on, sizeof on) != 0) {
		ns_notice(ns_log_default, "setsockopt(REUSEADDR): %s",
			  strerror(errno));
		/* Consider that your first warning of trouble to come. */
	}
	if (bind(ifp->sfd, (struct sockaddr *)&nsa, sizeof nsa) < 0) {
		if (errno != EADDRINUSE || ++n > 4) {
			if (errno == EADDRINUSE)
				ns_error(ns_log_default,
			  "There may be a name server already running on %s",
					 sin_ntoa(nsa));
			else
				ns_error(ns_log_default,
					 "bind(sfd=%d, %s): %s", ifp->sfd,
					 sin_ntoa(nsa), strerror(errno));
			return (-1);
		}

		/* Retry opening the socket a few times */
		close(ifp->sfd);
		ifp->sfd = -1;
		sleep(30);
		goto again;
	}
	if (evListen(ev, ifp->sfd, listenmax, stream_accept, ifp, &ifp->evID_s)
	    == -1) {
		ns_error(ns_log_default, "evListen(sfd=%d): %s",
			 ifp->sfd, strerror(errno));
		return (-1);
	}
	ifp->flags |= INTERFACE_CONN_VALID;
	return (0);
}

/* opensocket_f()
 *	Open datagram socket bound to no particular interface; use for ns_forw
 *	and sysquery.
 */
void
opensocket_f() {
	static struct sockaddr_in prev_qsrc;
	static int been_here;
	static interface *prev_ifp;
	struct sockaddr_in nsa;
	const int on = 1;
	ISC_SOCKLEN_T n;
	int need_close;
	interface *ifp;

	need_close = 0;
	if (been_here) {
		if (prev_ifp != NULL)
			prev_ifp->flags &= ~INTERFACE_FORWARDING;
		else if (server_options->query_source.sin_port == htons(0) ||
			 prev_qsrc.sin_addr.s_addr !=
			 server_options->query_source.sin_addr.s_addr ||
			 prev_qsrc.sin_port !=
			 server_options->query_source.sin_port)
			need_close = 1;
	} else
		ds = -1;

	been_here = 1;
	INSIST(server_options != NULL);

	if (need_close) {
		evDeselectFD(ev, ds_evID);
		close(ds);
		ds = -1;
	}

	/*
	 * If we're already listening on the query_source address and port,
	 * we don't need to open another socket.  We mark the interface, so
	 * we'll notice we're in trouble if it goes away.
	 */
	ifp = if_find(server_options->query_source.sin_addr,
		      server_options->query_source.sin_port, 0);
	if (ifp != NULL) {
		ifp->flags |= INTERFACE_FORWARDING;
		prev_ifp = ifp;
		ds = ifp->dfd;
		ns_info(ns_log_default, "forwarding source address is %s",
			sin_ntoa(server_options->query_source));
		return;
	}

	/*
	 * If we're already using the correct query source, we're done.
	 */
	if (ds >= 0)
		return;

	prev_qsrc = server_options->query_source;
	prev_ifp = NULL;

	if ((ds = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		ns_panic(ns_log_default, 1, "socket(SOCK_DGRAM): %s",
			 strerror(errno));
	if (ds > evHighestFD(ev))
		ns_panic(ns_log_default, 1, "socket too high: %d", ds);
	if (fcntl(ds, F_SETFD, 1) < 0)
		ns_panic(ns_log_default, 1, "F_SETFD: %s", strerror(errno));
	if (setsockopt(ds, SOL_SOCKET, SO_REUSEADDR,
		       (const char *)&on, sizeof on) != 0) {
		ns_notice(ns_log_default, "setsockopt(REUSEADDR): %s",
			  strerror(errno));
		/* XXX press on regardless, this is not too serious. */
	}
#ifdef SO_BSDCOMPAT
	if (setsockopt(ds, SOL_SOCKET, SO_BSDCOMPAT,
	    (char *)&on, sizeof on) != 0) {
		ns_notice(ns_log_default, "setsockopt(BSDCOMPAT): %s",
			  strerror(errno));
		/* XXX press on regardless, this is not too serious. */
	}
#endif
	if (bind(ds, (struct sockaddr *)&server_options->query_source,
		 sizeof server_options->query_source) < 0)
		ns_panic(ns_log_default, 0, "opensocket_f: bind(%s): %s",
			 sin_ntoa(server_options->query_source),
			 strerror(errno));

	n = sizeof nsa;
	if (getsockname(ds, (struct sockaddr *)&nsa, &n) < 0)
		ns_panic(ns_log_default, 1, "opensocket_f: getsockaddr: %s",
			 strerror(errno));

	ns_debug(ns_log_default, 1, "fwd ds %d addr %s", ds, sin_ntoa(nsa));
	ns_info(ns_log_default, "Forwarding source address is %s",
		sin_ntoa(nsa));

	if (evSelectFD(ev, ds, EV_READ, datagram_read, NULL, &ds_evID) == -1)
		ns_panic(ns_log_default, 1, "evSelectFD(fd %d): %s",
			 ds, strerror(errno));
	/* XXX: should probably use a different FileFunc that only accepts
	 *	responses, since requests on this socket make no sense.
	 */
}

static void
setdebug(int new_debug) {
#ifdef DEBUG
	int old_debug;
	
	if (!new_debug)
		ns_debug(ns_log_default, 1, "Debug off");
	old_debug = debug;
	debug = new_debug;
	log_option(log_ctx, LOG_OPTION_DEBUG, debug);
	log_option(log_ctx, LOG_OPTION_LEVEL, debug);
	if (old_debug && !debug)
		log_close_debug_channels(log_ctx);
	evSetDebug(ev, debug, log_get_stream(eventlib_channel));
	if (debug) {
		if (!old_debug)
			open_special_channels();
		ns_debug(ns_log_default, 1, "Debug level %d", debug);
		if (!old_debug) {
			ns_debug(ns_log_default, 1, "Version = %s", Version);
			ns_debug(ns_log_default, 1, "conffile = %s", conffile);
		}
	}
#endif
}

/*
** Routines for managing stream queue
*/

static struct qstream *
sq_add() {
	struct qstream *sqp;

	if (!(sqp = (struct qstream *)memget(sizeof *sqp))) {
		ns_error(ns_log_default, "sq_add: memget: %s",
			 strerror(errno));
		return (NULL);
	}
	memset(sqp, 0, sizeof *sqp);
	ns_debug(ns_log_default, 3, "sq_add(%#lx)", (u_long)sqp);

	sqp->flags = 0;
	/* XXX should init other fields too? */
	sqp->s_next = streamq;
	streamq = sqp;
	return (sqp);
}

/* sq_remove(qp)
 *	remove stream queue structure `qp'.
 *	no current queries may refer to this stream when it is removed.
 * side effects:
 *	memory is deallocated.  sockets are closed.  lists are relinked.
 */
void
sq_remove(struct qstream *qp) {
	struct qstream *qsp;

	ns_debug(ns_log_default, 2, "sq_remove(%#lx, %d) rfcnt=%d",
		 (u_long)qp, qp->s_rfd, qp->s_refcnt);

	if (qp->s_wbuf != NULL) {
		memput(qp->s_wbuf, qp->s_wbuf_end - qp->s_wbuf);
		qp->s_wbuf_send = qp->s_wbuf_free = NULL;
		qp->s_wbuf_end = qp->s_wbuf = NULL;
	}
	if (qp->flags & STREAM_MALLOC)
		memput(qp->s_buf, qp->s_bufsize);
	if (qp->flags & STREAM_READ_EV)
		INSIST_ERR(evCancelRW(ev, qp->evID_r) != -1);
	if (qp->flags & STREAM_WRITE_EV)
		INSIST_ERR(evDeselectFD(ev, qp->evID_w) != -1);
	if (qp->flags & STREAM_CONNECT_EV)
		INSIST_ERR(evCancelConn(ev, qp->evID_c) != -1);
	if (qp->flags & STREAM_AXFR || qp->flags & STREAM_AXFRIXFR)
		ns_freexfr(qp);
	(void) close(qp->s_rfd);
	if (qp == streamq)
		streamq = qp->s_next;
	else {
		for (qsp = streamq;
		     qsp && (qsp->s_next != qp);
		     qsp = qsp->s_next)
			(void)NULL;
		if (qsp)
			qsp->s_next = qp->s_next;
	}
	memput(qp, sizeof *qp);
}

/* void
 * sq_flush(allbut)
 *	call sq_remove() on all open streams except `allbut'
 * side effects:
 *	global list `streamq' modified
 * idiocy:
 *	is N^2 due to the scan inside of sq_remove()
 */
void
sq_flush(struct qstream *allbut) {
	struct qstream *sp, *spnext;

	for (sp = streamq; sp != NULL; sp = spnext) {
		spnext = sp->s_next;
		if (sp != allbut)
			sq_remove(sp);
	}
}

/* int
 * sq_openw(qs, buflen)
 *	add a write buffer to a stream
 * return:
 *	0 = success
 *	-1 = failure (check errno)
 */
int
sq_openw(struct qstream *qs, int buflen) {
#ifdef DO_SO_LINGER	/* XXX */
	static const struct linger ll = { 1, 120 };
#endif

	INSIST(qs->s_wbuf == NULL);
	qs->s_wbuf = (u_char *)memget(buflen);
	if (qs->s_wbuf == NULL)
		return (-1);
	qs->s_wbuf_send = qs->s_wbuf;
	qs->s_wbuf_free = qs->s_wbuf;
	qs->s_wbuf_end = qs->s_wbuf + buflen;
#ifdef DO_SO_LINGER	/* XXX */
	/* kernels that map pages for IO end up failing if the pipe is full
	 * at exit and we take away the final buffer.  this is really a kernel
	 * bug but it's harmless on systems that are not broken, so...
	 */
	setsockopt(qs->s_rfd, SOL_SOCKET, SO_LINGER, (char *)&ll, sizeof ll);
#endif
	return (0);
}

/* static void
 * sq_dowrite(qs)
 *	try to submit data to the system, remove it from our queue.
 */
static int
sq_dowrite(struct qstream *qs) {
	if (qs->s_wbuf_free > qs->s_wbuf_send) {
		int n = write(qs->s_rfd, qs->s_wbuf_send,
			      qs->s_wbuf_free - qs->s_wbuf_send);
		INSIST(qs->s_wbuf != NULL);
		if (n < 0) {
			if (errno != EINTR && errno != EAGAIN
#if (EWOULDBLOCK != EAGAIN)
			    && errno != EWOULDBLOCK
#endif
			    )
				return (-1);
			return (0);
		}
		qs->s_wbuf_send += n;
		if (qs->s_wbuf_free > qs->s_wbuf_send) {
			/* XXX: need some kind of delay here during which the
			 *	socket will be deselected so we don't spin.
			 */
			n = qs->s_wbuf_free - qs->s_wbuf_send;
			memmove(qs->s_wbuf, qs->s_wbuf_send, n);
			qs->s_wbuf_send = qs->s_wbuf;
			qs->s_wbuf_free = qs->s_wbuf + n;
		}
	}
	if (qs->s_wbuf_free == qs->s_wbuf_send)
		qs->s_wbuf_free = qs->s_wbuf_send = qs->s_wbuf;
	return (0);
}

/* void
 * sq_flushw(qs)
 *	called when the socket becomes writable and we want to flush our
 *	buffers and the system's socket buffers.  use as a closure with
 *	sq_writeh().
 */
void
sq_flushw(struct qstream *qs) {
	if (qs->s_wbuf_free == qs->s_wbuf_send) {
		sq_writeh(qs, NULL);
		sq_done(qs);
	}
}

/* static void
 * sq_writable(ctx, uap, fd, evmask)
 *	glue between eventlib closures and qstream closures
 */
static void
sq_writable(evContext ctx, void *uap, int fd, int evmask) {
	struct qstream *qs = uap;

	UNUSED(ctx);

	INSIST(evmask & EV_WRITE);
	INSIST(fd == qs->s_rfd);
	if (sq_dowrite(qs) < 0) {
		sq_remove(qs);
		return;
	}
	if (qs->s_wbuf_closure
	    && qs->s_wbuf_end - qs->s_wbuf_free >= HFIXEDSZ+2)	/* XXX guess */
		(*qs->s_wbuf_closure)(qs);
	if (sq_dowrite(qs) < 0) {
		sq_remove(qs);
		return;
	}
}

/* int
 * sq_writeh(qs, closure)
 *	register a closure to be called when a stream becomes writable
 * return:
 *	0 = success
 *	-1 = failure (check errno)
 */
int
sq_writeh(struct qstream *qs, sq_closure c) {
	if (c) {
		if (!qs->s_wbuf_closure) {
			if (evSelectFD(ev, qs->s_rfd, EV_WRITE,
				       sq_writable, qs, &qs->evID_w) < 0) {
				return (-1);
			}
			qs->flags |= STREAM_WRITE_EV;
		}
	} else {
		(void) evDeselectFD(ev, qs->evID_w);
		qs->flags &= ~STREAM_WRITE_EV;
	}
	qs->s_wbuf_closure = c;
	return (0);
}

/* int
 * sq_write(qs, buf, len)
 *	queue a message onto the stream, prepended by a two byte length field
 * return:
 *	0 = success
 *	-1 = failure (check errno; E2BIG means we can't handle this right now)
 */
int
sq_write(struct qstream *qs, const u_char *buf, int len) {
	INSIST(qs->s_wbuf != NULL);
	if (NS_INT16SZ + len > qs->s_wbuf_end - qs->s_wbuf_free) {
		if (sq_dowrite(qs) < 0)
			return (-1);
		if (NS_INT16SZ + len > qs->s_wbuf_end - qs->s_wbuf_free) {
			errno = E2BIG;
			return (-1);
		}
	}
	ns_put16(len, qs->s_wbuf_free);
	qs->s_wbuf_free += NS_INT16SZ;
	memcpy(qs->s_wbuf_free, buf, len);
	qs->s_wbuf_free += len;
	return (0);
}

/*
 * Initiate query on stream;
 * mark as referenced and stop selecting for input.
 */
static void
sq_query(struct qstream *sp) {
	sp->s_refcnt++;
}

/*
 * Note that the current request on a stream has completed,
 * and that we should continue looking for requests on the stream.
 */
void
sq_done(struct qstream *sp) {
	struct iovec iov;

	if (sp->s_wbuf != NULL) {
		INSIST(sp->s_wbuf_send == sp->s_wbuf_free);
		memput(sp->s_wbuf, sp->s_wbuf_end - sp->s_wbuf);
		sp->s_wbuf_send = sp->s_wbuf_free = NULL;
		sp->s_wbuf_end = sp->s_wbuf = NULL;
	}
	if (sp->flags & STREAM_AXFR || sp->flags & STREAM_AXFRIXFR)
		ns_freexfr(sp);
	sp->s_refcnt = 0;
	sp->s_time = tt.tv_sec;
	if (sp->flags & STREAM_DONE_CLOSE) {
		/* XXX */
		sq_remove(sp);
		return;
	}
	iov = evConsIovec(sp->s_temp, INT16SZ);
	if (evRead(ev, sp->s_rfd, &iov, 1, stream_getlen, sp, &sp->evID_r) ==
	    -1) {
		ns_error(ns_log_default, "evRead(fd %d): %s",
			 sp->s_rfd, strerror(errno));
		sq_remove(sp);
		return;
	}
	sp->flags |= STREAM_READ_EV;
}

/* void
 * dq_remove_gen(gen)
 *	close/deallocate all the udp sockets (except 0.0.0.0) which are
 *	not from the current generation.
 * side effects:
 *	global list `iflist' is modified.
 */
void
dq_remove_gen(time_t gen) {
	interface *this, *next;

	for (this = HEAD(iflist); this != NULL; this = next) {
		next = NEXT(this, link);
		if (this->gen != gen && ina_hlong(this->addr) != INADDR_ANY)
			dq_remove(this);
	}
}

/* void
 * dq_remove_all()
 *	close/deallocate all interfaces.
 * side effects:
 *	global list `iflist' is modified.
 */
void
dq_remove_all() {
	interface *this, *next;

	for (this = HEAD(iflist); this != NULL; this = next) {
		next = NEXT(this, link);
		/* 
		 * Clear the forwarding flag so we don't panic the server.
		 */
		this->flags &= ~INTERFACE_FORWARDING;
		dq_remove(this);
	}
}

/* void
 * dq_remove(interface *this)
 *	close/deallocate an interface's sockets.  called on errors
 *	or if the interface disappears.
 * side effects:
 *	global list `iflist' is modified.
 */
static void
dq_remove(interface *this) {
	ns_notice(ns_log_default, "deleting interface [%s].%u",
		  inet_ntoa(this->addr), ntohs(this->port));

	if ((this->flags & INTERFACE_FORWARDING) != 0)
		ns_panic(ns_log_default, 0,
			 "forwarding interface [%s].%u gone",
			 inet_ntoa(this->addr),
			 ntohs(this->port));

	/* Deallocate fields. */
	if ((this->flags & INTERFACE_FILE_VALID) != 0)
		(void) evDeselectFD(ev, this->evID_d);
	if (this->dfd >= 0)
		(void) close(this->dfd);
	if ((this->flags & INTERFACE_CONN_VALID) != 0)
		(void) evCancelConn(ev, this->evID_s);
	if (this->sfd >= 0)
		(void) close(this->sfd);

	UNLINK(iflist, this, link);
	memput(this, sizeof *this);
}

/* struct in_addr
 * net_mask(ina)
 *	makes a classful assumption in a classless world, and returns it.
 */
struct in_addr
net_mask(struct in_addr ina) {
	u_long hl = ina_hlong(ina);
	struct in_addr ret;

	if (IN_CLASSA(hl))
		hl = IN_CLASSA_NET;
	else if (IN_CLASSB(hl))
		hl = IN_CLASSB_NET;
	else if (IN_CLASSC(hl))
		hl = IN_CLASSC_NET;
	else
		hl = INADDR_BROADCAST;
	ina_ulong(ret) = htonl(hl);
	return (ret);
}

/* aIsUs(addr)
 *	scan our list of interface addresses for "addr".
 * returns:
 *	0: address isn't one of our interfaces
 *	>0: address is one of our interfaces, or INADDR_ANY
 */
int
aIsUs(struct in_addr addr) {

	if (ina_hlong(addr) == INADDR_ANY || if_find(addr, 0, 1) != NULL)
		return (1);
	return (0);
}

/* interface *
 * if_find(addr, port, anyport)
 *	scan our list of interface addresses for "addr" and port.
 * returns:
 *	pointer to interface with this address/port, or NULL if there isn't
 *      one.
 */
static interface *
if_find(struct in_addr addr, u_int16_t port, int anyport) {
	interface *ifp;

	for (ifp = HEAD(iflist); ifp != NULL; ifp = NEXT(ifp, link))
		if (ina_equal(addr, ifp->addr))
			if (anyport || ifp->port == port)
				break;
	return (ifp);
}

/*
 * These are here in case we ever want to get more clever, like perhaps
 * using a bitmap to keep track of outstanding queries and a random
 * allocation scheme to make it a little harder to predict them.  Note
 * that the resolver will need the same protection so the cleverness
 * should be put there rather than here; this is just an interface layer.
 *
 * This is true but ... most clients only send out a few queries, they
 * use varying port numbers, and the queries aren't sent to the outside
 * world which we know is full of spoofers.  Doing a good job of randomizing
 * ids may also be to expensive for each client. Queries forwarded by the
 * server always come from the same port (unless you let 8.x pick a port
 * and restart it periodically - maybe it should open several and use
 * them randomly).  The server sends out lots more queries, and if it's
 * cache is corrupted, it has the potential to affect more clients.
 * NOTE: - randomizing the ID or source port doesn't help a bit if the
 * queries can be sniffed.
 *                             -- DL
 */

/*
 * Allow the user to pick one of two ID randomization algorithms.
 *
 * The first algorithm is an adaptation of the sequence shuffling
 * algorithm discovered by Carter Bays and S. D. Durham [ACM Trans. Math.
 * Software 2 (1976), 59-64], as documented as Algorithm B in Chapter
 * 3.2.2 in Volume 2 of Knuth's "The Art of Computer Programming".  We use
 * a randomly selected linear congruential random number generator with a
 * modulus of 2^16, whose increment is a randomly picked odd number, and
 * whose multiplier is picked from a set which meets the following
 * criteria:
 *     Is of the form 8*n+5, which ensures "high potency" according to
 *     principle iii in the summary chapter 3.6.  This form also has a
 *     gcd(a-1,m) of 4 which is good according to principle iv.
 *
 *     Is between 0.01 and 0.99 times the modulus as specified by
 *     principle iv.
 *
 *     Passes the spectral test "with flying colors" (ut >= 1) in
 *     dimensions 2 through 6 as calculated by Algorithm S in Chapter
 *     3.3.4 and the ratings calculated by formula 35 in section E.
 *
 *     Of the multipliers that pass this test, pick the set that is
 *     best according to the theoretical bounds of the serial
 *     correlation test.  This was calculated using a simplified
 *     version of Knuth's Theorem K in Chapter 3.3.3.
 *
 * These criteria may not be important for this use, but we might as well
 * pick from the best generators since there are so many possible ones and
 * we don't have that many random bits to do the picking.
 *
 * We use a modulus of 2^16 instead of something bigger so that we will
 * tend to cycle through all the possible IDs before repeating any,
 * however the shuffling will perturb this somewhat.  Theoretically there
 * is no minimimum interval between two uses of the same ID, but in
 * practice it seems to be >64000.
 *
 * Our adaptatation  of Algorithm B mixes the hash state which has
 * captured various random events into the shuffler to perturb the
 * sequence.
 *
 * One disadvantage of this algorithm is that if the generator parameters
 * were to be guessed, it would be possible to mount a limited brute force
 * attack on the ID space since the IDs are only shuffled within a limited
 * range.
 *
 * The second algorithm uses the same random number generator to populate
 * a pool of 65536 IDs.  The hash state is used to pick an ID from a window
 * of 4096 IDs in this pool, then the chosen ID is swapped with the ID
 * at the beginning of the window and the window position is advanced.
 * This means that the interval between uses of the ID will be no less
 * than 65536-4096.  The ID sequence in the pool will become more random
 * over time.
 *
 * For both algorithms, two more linear congruential random number generators
 * are selected.  The ID from the first part of algorithm is used to seed
 * the first of these generators, and its output is used to seed the second.
 * The strategy is use these generators as 1 to 1 hashes to obfuscate the
 * properties of the generator used in the first part of either algorithm.
 *
 * The first algorithm may be suitable for use in a client resolver since
 * its memory requirements are fairly low and it's pretty random out of
 * the box.  It is somewhat succeptible to a limited brute force attack,
 * so the second algorithm is probably preferable for a longer running
 * program that issues a large number of queries and has time to randomize
 * the pool.
 */

#define NSID_SHUFFLE_TABLE_SIZE 100 /* Suggested by Knuth */
/*
 * Pick one of the next 4096 IDs in the pool.
 * There is a tradeoff here between randomness and how often and ID is reused.
 */
#define NSID_LOOKAHEAD 4096	/* Must be a power of 2 */
#define NSID_SHUFFLE_ONLY 1	/* algorithm 1 */
#define NSID_USE_POOL 2		/* algorithm 2 */

/*
 * Keep a running hash of various bits of data that we'll use to
 * stir the ID pool or perturb the ID generator
 */
void
nsid_hash(u_char *data, size_t len) {
	/*
	 * Hash function similar to the one we use for hashing names.
	 * We don't fold case or toss the upper bit here, though.
	 * This hash doesn't do much interesting when fed binary zeros,
	 * so there may be a better hash function.
	 * This function doesn't need to be very strong since we're
	 * only using it to stir the pool, but it should be reasonably
	 * fast.
	 */
	while (len-- > 0) {
		nsid_hash_state = HASHROTATE(nsid_hash_state);
		nsid_hash_state += *data++;
	}
}

/*
 * Table of good linear congruential multipliers for modulus 2^16
 * in order of increasing serial correlation bounds (so trim from
 * the end).
 */
static const u_int16_t nsid_multiplier_table[] = {
	17565, 25013, 11733, 19877, 23989, 23997, 24997, 25421,
	26781, 27413, 35901, 35917, 35973, 36229, 38317, 38437,
	39941, 40493, 41853, 46317, 50581, 51429, 53453, 53805,
	11317, 11789, 12045, 12413, 14277, 14821, 14917, 18989,
	19821, 23005, 23533, 23573, 23693, 27549, 27709, 28461,
	29365, 35605, 37693, 37757, 38309, 41285, 45261, 47061,
	47269, 48133, 48597, 50277, 50717, 50757, 50805, 51341,
	51413, 51581, 51597, 53445, 11493, 14229, 20365, 20653,
	23485, 25541, 27429, 29421, 30173, 35445, 35653, 36789,
	36797, 37109, 37157, 37669, 38661, 39773, 40397, 41837,
	41877, 45293, 47277, 47845, 49853, 51085, 51349, 54085,
	56933,  8877,  8973,  9885, 11365, 11813, 13581, 13589,
	13613, 14109, 14317, 15765, 15789, 16925, 17069, 17205,
	17621, 17941, 19077, 19381, 20245, 22845, 23733, 24869,
	25453, 27213, 28381, 28965, 29245, 29997, 30733, 30901,
	34877, 35485, 35613, 36133, 36661, 36917, 38597, 40285,
	40693, 41413, 41541, 41637, 42053, 42349, 45245, 45469,
	46493, 48205, 48613, 50861, 51861, 52877, 53933, 54397,
	55669, 56453, 56965, 58021,  7757,  7781,  8333,  9661,
	12229, 14373, 14453, 17549, 18141, 19085, 20773, 23701,
	24205, 24333, 25261, 25317, 27181, 30117, 30477, 34757,
	34885, 35565, 35885, 36541, 37957, 39733, 39813, 41157,
	41893, 42317, 46621, 48117, 48181, 49525, 55261, 55389,
	56845,  7045,  7749,  7965,  8469,  9133,  9549,  9789,
	10173, 11181, 11285, 12253, 13453, 13533, 13757, 14477,
	15053, 16901, 17213, 17269, 17525, 17629, 18605, 19013,
	19829, 19933, 20069, 20093, 23261, 23333, 24949, 25309,
	27613, 28453, 28709, 29301, 29541, 34165, 34413, 37301,
	37773, 38045, 38405, 41077, 41781, 41925, 42717, 44437,
	44525, 44613, 45933, 45941, 47077, 50077, 50893, 52117,
	 5293, 55069, 55989, 58125, 59205,  6869, 14685, 15453,
	16821, 17045, 17613, 18437, 21029, 22773, 22909, 25445,
	25757, 26541, 30709, 30909, 31093, 31149, 37069, 37725,
	37925, 38949, 39637, 39701, 40765, 40861, 42965, 44813,
	45077, 45733, 47045, 50093, 52861, 52957, 54181, 56325,
	56365, 56381, 56877, 57013,  5741, 58101, 58669,  8613,
	10045, 10261, 10653, 10733, 11461, 12261, 14069, 15877,
	17757, 21165, 23885, 24701, 26429, 26645, 27925, 28765,
	29197, 30189, 31293, 39781, 39909, 40365, 41229, 41453,
	41653, 42165, 42365, 47421, 48029, 48085, 52773,  5573,
	57037, 57637, 58341, 58357, 58901,  6357,  7789,  9093,
	10125, 10709, 10765, 11957, 12469, 13437, 13509, 14773,
	15437, 15773, 17813, 18829, 19565, 20237, 23461, 23685,
	23725, 23941, 24877, 25461, 26405, 29509, 30285, 35181,
	37229, 37893, 38565, 40293, 44189, 44581, 45701, 47381,
	47589, 48557,  4941, 51069,  5165, 52797, 53149,  5341,
	56301, 56765, 58581, 59493, 59677,  6085,  6349,  8293,
	 8501,  8517, 11597, 11709, 12589, 12693, 13517, 14909,
	17397, 18085, 21101, 21269, 22717, 25237, 25661, 29189,
	30101, 31397, 33933, 34213, 34661, 35533, 36493, 37309,
	40037,  4189, 42909, 44309, 44357, 44389,  4541, 45461,
	46445, 48237, 54149, 55301, 55853, 56621, 56717, 56901,
	 5813, 58437, 12493, 15365, 15989, 17829, 18229, 19341,
	21013, 21357, 22925, 24885, 26053, 27581, 28221, 28485,
	30605, 30613, 30789, 35437, 36285, 37189,  3941, 41797,
	 4269, 42901, 43293, 44645, 45221, 46893,  4893, 50301,
	50325,  5189, 52109, 53517, 54053, 54485,  5525, 55949,
	56973, 59069, 59421, 60733, 61253,  6421,  6701,  6709,
	 7101,  8669, 15797, 19221, 19837, 20133, 20957, 21293,
	21461, 22461, 29085, 29861, 30869, 34973, 36469, 37565,
	38125, 38829, 39469, 40061, 40117, 44093, 47429, 48341,
	50597, 51757,  5541, 57629, 58405, 59621, 59693, 59701,
	61837,  7061, 10421, 11949, 15405, 20861, 25397, 25509,
	25893, 26037, 28629, 28869, 29605, 30213, 34205, 35637,
	36365, 37285,  3773, 39117,  4021, 41061, 42653, 44509,
	 4461, 44829,  4725,  5125, 52269, 56469, 59085,  5917,
	60973,  8349, 17725, 18637, 19773, 20293, 21453, 22533,
	24285, 26333, 26997, 31501, 34541, 34805, 37509, 38477,
	41333, 44125, 46285, 46997, 47637, 48173,  4925, 50253,
	50381, 50917, 51205, 51325, 52165, 52229,  5253,  5269,
	53509, 56253, 56341,  5821, 58373, 60301, 61653, 61973,
	62373,  8397, 11981, 14341, 14509, 15077, 22261, 22429,
	24261, 28165, 28685, 30661, 34021, 34445, 39149,  3917,
	43013, 43317, 44053, 44101,  4533, 49541, 49981,  5277,
	54477, 56357, 57261, 57765, 58573, 59061, 60197, 61197,
	62189,  7725,  8477,  9565, 10229, 11437, 14613, 14709,
	16813, 20029, 20677, 31445,  3165, 31957,  3229, 33541,
	36645,  3805, 38973,  3965,  4029, 44293, 44557, 46245,
	48917,  4909, 51749, 53709, 55733, 56445,  5925,  6093,
	61053, 62637,  8661,  9109, 10821, 11389, 13813, 14325,
	15501, 16149, 18845, 22669, 26437, 29869, 31837, 33709,
	33973, 34173,  3677,  3877,  3981, 39885, 42117,  4421,
	44221, 44245, 44693, 46157, 47309,  5005, 51461, 52037,
	55333, 55693, 56277, 58949,  6205, 62141, 62469,  6293,
	10101, 12509, 14029, 17997, 20469, 21149, 25221, 27109,
	 2773,  2877, 29405, 31493, 31645,  4077, 42005, 42077,
	42469, 42501, 44013, 48653, 49349,  4997, 50101, 55405,
	56957, 58037, 59429, 60749, 61797, 62381, 62837,  6605,
	10541, 23981, 24533,  2701, 27333, 27341, 31197, 33805,
	 3621, 37381,  3749,  3829, 38533, 42613, 44381, 45901,
	48517, 51269, 57725, 59461, 60045, 62029, 13805, 14013,
	15461, 16069, 16157, 18573,  2309, 23501, 28645,  3077,
	31541, 36357, 36877,  3789, 39429, 39805, 47685, 47949,
	49413,  5485, 56757, 57549, 57805, 58317, 59549, 62213,
	62613, 62853, 62933,  8909, 12941, 16677, 20333, 21541,
	24429, 26077, 26421,  2885, 31269, 33381,  3661, 40925,
	42925, 45173,  4525,  4709, 53133, 55941, 57413, 57797,
	62125, 62237, 62733,  6773, 12317, 13197, 16533, 16933,
	18245,  2213,  2477, 29757, 33293, 35517, 40133, 40749,
	 4661, 49941, 62757,  7853,  8149,  8573, 11029, 13421,
	21549, 22709, 22725, 24629,  2469, 26125,  2669, 34253,
	36709, 41013, 45597, 46637, 52285, 52333, 54685, 59013,
	60997, 61189, 61981, 62605, 62821,  7077,  7525,  8781,
	10861, 15277,  2205, 22077, 28517, 28949, 32109, 33493,
	 3685, 39197, 39869, 42621, 44997, 48565,  5221, 57381,
	61749, 62317, 63245, 63381, 23149,  2549, 28661, 31653,
	33885, 36341, 37053, 39517, 42805, 45853, 48997, 59349,
	60053, 62509, 63069,  6525,  1893, 20181,  2365, 24893,
	27397, 31357, 32277, 33357, 34437, 36677, 37661, 43469,
	43917, 50997, 53869,  5653, 13221, 16741, 17893,  2157,
	28653, 31789, 35301, 35821, 61613, 62245, 12405, 14517,
	17453, 18421,  3149,  3205, 40341,  4109, 43941, 46869,
	48837, 50621, 57405, 60509, 62877,  8157, 12933, 12957,
	16501, 19533,  3461, 36829, 52357, 58189, 58293, 63053,
	17109,  1933, 32157, 37701, 59005, 61621, 13029, 15085,
	16493, 32317, 35093,  5061, 51557, 62221, 20765, 24613,
	 2629, 30861, 33197, 33749, 35365, 37933, 40317, 48045,
	56229, 61157, 63797,  7917, 17965,  1917,  1973, 20301,
	 2253, 33157, 58629, 59861, 61085, 63909,  8141,  9221,
	14757,  1581, 21637, 26557, 33869, 34285, 35733, 40933,
	42517, 43501, 53653, 61885, 63805,  7141, 21653, 54973,
	31189, 60061, 60341, 63357, 16045,  2053, 26069, 33997,
	43901, 54565, 63837,  8949, 17909, 18693, 32349, 33125,
	37293, 48821, 49053, 51309, 64037,  7117,  1445, 20405,
	23085, 26269, 26293, 27349, 32381, 33141, 34525, 36461,
	37581, 43525,  4357, 43877,  5069, 55197, 63965,  9845,
	12093,  2197,  2229, 32165, 33469, 40981, 42397,  8749,
	10853,  1453, 18069, 21693, 30573, 36261, 37421, 42533
};
#define NSID_MULT_TABLE_SIZE \
	((sizeof nsid_multiplier_table)/(sizeof nsid_multiplier_table[0]))

void
nsid_init(void) {
	struct timeval now;
	pid_t mypid;
	u_int16_t a1ndx, a2ndx, a3ndx, c1ndx, c2ndx, c3ndx;
	int i;

	if (nsid_algorithm != 0)
		return;

	gettimeofday(&now, NULL);
	mypid = getpid();

	/* Initialize the state */
	nsid_hash_state = 0;
	nsid_hash((u_char *)&now, sizeof now);
	nsid_hash((u_char *)&mypid, sizeof mypid);

	/*
	 * Select our random number generators and initial seed.
	 * We could really use more random bits at this point,
	 * but we'll try to make a silk purse out of a sows ear ...
	 */
	/* generator 1 */
	a1ndx = ((u_long) NSID_MULT_TABLE_SIZE *
		 (nsid_hash_state & 0xFFFF)) >> 16;
	nsid_a1 = nsid_multiplier_table[a1ndx];
	c1ndx = (nsid_hash_state >> 9) & 0x7FFF;
	nsid_c1 = 2*c1ndx + 1;
	/* generator 2, distinct from 1 */
	a2ndx = ((u_long) (NSID_MULT_TABLE_SIZE - 1) *
		 ((nsid_hash_state >> 10) & 0xFFFF)) >> 16;
	if (a2ndx >= a1ndx)
		a2ndx++;
	nsid_a2 = nsid_multiplier_table[a2ndx];
	c2ndx = nsid_hash_state % 32767;
	if (c2ndx >= c1ndx)
		c2ndx++;
	nsid_c2 = 2*c2ndx + 1;
	/* generator 3, distinct from 1 and 2 */
	a3ndx = ((u_long) (NSID_MULT_TABLE_SIZE - 2) *
		 ((nsid_hash_state >> 20) & 0xFFFF)) >> 16;
	if (a3ndx >= a1ndx || a3ndx >= a2ndx)
		a3ndx++;
	if (a3ndx >= a1ndx && a3ndx >= a2ndx)
		a3ndx++;
	nsid_a3 = nsid_multiplier_table[a3ndx];
	c3ndx = nsid_hash_state % 32766;
	if (c3ndx >= c1ndx || c3ndx >= c2ndx)
		c3ndx++;
	if (c3ndx >= c1ndx && c3ndx >= c2ndx)
		c3ndx++;
	nsid_c3 = 2*c3ndx + 1;

	nsid_state = ((nsid_hash_state >> 16) ^ (nsid_hash_state)) & 0xFFFF;

	/* Do the algorithm specific initialization */
	INSIST(server_options != NULL);
	if (NS_OPTION_P(OPTION_USE_ID_POOL) == 0) {
		/* Algorithm 1 */
		nsid_algorithm = NSID_SHUFFLE_ONLY;
		nsid_vtable = memget(NSID_SHUFFLE_TABLE_SIZE *
				     (sizeof(u_int16_t)) );
		if (!nsid_vtable)
			ns_panic(ns_log_default, 1, "memget(nsid_vtable)");
		for (i = 0; i < NSID_SHUFFLE_TABLE_SIZE; i++) {
			nsid_vtable[i] = nsid_state;
			nsid_state = (((u_long) nsid_a1 * nsid_state) + nsid_c1)
					& 0xFFFF;
		}
		nsid_state2 = nsid_state;
	} else {
		/* Algorithm 2 */
		nsid_algorithm = NSID_USE_POOL;
		nsid_pool = memget(0x10000 * (sizeof(u_int16_t)));
		if (!nsid_pool)
			ns_panic(ns_log_default, 1, "memget(nsid_pool)");
		for (i = 0; ; i++) {
			nsid_pool[i] = nsid_state;
			nsid_state = (((u_long) nsid_a1 * nsid_state) + nsid_c1)                                     & 0xFFFF;
			if (i == 0xFFFF)
				break;
		}
	}
}

#define NSID_RANGE_MASK (NSID_LOOKAHEAD - 1)

#define NSID_POOL_MASK 0xFFFF /* used to wrap the pool index */

u_int16_t
nsid_next() {
	u_int16_t id, compressed_hash;

	compressed_hash = ((nsid_hash_state >> 16) ^ (nsid_hash_state)) &
			0xFFFF;
	if (nsid_algorithm == NSID_SHUFFLE_ONLY) {
		u_int16_t j;

		/*
		 * This is the original Algorithm B
		 * j = ((u_long) NSID_SHUFFLE_TABLE_SIZE * nsid_state2)
		 *     >> 16;
		 *
		 * We'll perturb it with some random stuff  ...
		 */
		j = ((u_long) NSID_SHUFFLE_TABLE_SIZE *
		     (nsid_state2 ^ compressed_hash)) >> 16;
		nsid_state2 = id = nsid_vtable[j];
		nsid_state = (((u_long) nsid_a1 * nsid_state) + nsid_c1) &
				0xFFFF;
		nsid_vtable[j] = nsid_state;
	} else if (nsid_algorithm == NSID_USE_POOL) {
		u_int16_t pick;

		pick = compressed_hash & NSID_RANGE_MASK;
		id = nsid_pool[(nsid_state + pick) & NSID_POOL_MASK];
		if (pick != 0) {
			/* Swap two IDs to stir the pool */
			nsid_pool[(nsid_state + pick) & NSID_POOL_MASK] =
				nsid_pool[nsid_state];
			nsid_pool[nsid_state] = id;
		}

		/* increment the base pointer into the pool */
		if (nsid_state == 65535)
			nsid_state = 0;
		else
			nsid_state++;
	} else {
		id = 0;	/* silence compiler */
		ns_panic(ns_log_default, 1, "Unknown ID algorithm");
	}

	/* Now lets obfuscate ... */
	id = (((u_long) nsid_a2 * id) + nsid_c2) & 0xFFFF;
	id = (((u_long) nsid_a3 * id) + nsid_c3) & 0xFFFF;

	return (id);
}

/* Note: this function CAN'T deallocate the saved_argv[]. */
static void
deallocate_everything(void) {
	FILE *f;

	f = write_open(server_options->memstats_filename);

	ns_freestats();
	qflush();
	sq_flush(NULL);
	free_addinfo();
	ns_shutdown();
	dq_remove_all();
	db_lame_destroy();
	if (local_addresses != NULL)
		free_ip_match_list(local_addresses);
	if (local_networks != NULL)
		free_ip_match_list(local_networks);
	destroyservicelist();
	destroyprotolist();
	shutdown_logging();
	evDestroy(ev);
	if (conffile != NULL)
		freestr(conffile);
	conffile = NULL;
	if (debugfile != NULL)
		freestr(debugfile);
	debugfile = NULL;
	if (user_name != NULL)
		freestr(user_name);
	user_name = NULL;
	if (group_name != NULL)
		freestr(group_name);
	group_name = NULL;
	if (chroot_dir != NULL)
		freestr(chroot_dir);
	chroot_dir = NULL;
	if (working_dir != NULL)
		freestr(working_dir);
	working_dir = NULL;
	if (nsid_pool != NULL)
		memput(nsid_pool, 0x10000 * (sizeof(u_int16_t)));
	nsid_pool = NULL;
	if (nsid_vtable != NULL)
		memput(nsid_vtable, NSID_SHUFFLE_TABLE_SIZE *
				     (sizeof(u_int16_t)));
	nsid_vtable = NULL;
	irs_destroy();
	if (f != NULL) {
		memstats(f);
		(void)fclose(f);
	}
	if (memactive())
		abort();
}
	
static void
ns_restart(void) {
	needs_restart = 1;
	needs_exit = 1;
}

static void
use_desired_debug(void) {
#ifdef DEBUG
	sigset_t set;

	/* Protect against race conditions by blocking debugging signals. */

	if (sigemptyset(&set) < 0) {
		ns_error(ns_log_os,
			 "sigemptyset failed in use_desired_debug: %s",
			 strerror(errno));
		return;
	}
	if (sigaddset(&set, SIGUSR1) < 0) {
		ns_error(ns_log_os,
			 "sigaddset SIGUSR1 failed in use_desired_debug: %s",
			 strerror(errno));
		return;
	}
	if (sigaddset(&set, SIGUSR2) < 0) {
		ns_error(ns_log_os,
			 "sigaddset SIGUSR2 failed in use_desired_debug: %s",
			 strerror(errno));
		return;
	}
	if (sigprocmask(SIG_BLOCK, &set, NULL) < 0) {
		ns_error(ns_log_os,
			 "sigprocmask to block USR1 and USR2 failed: %s",
			 strerror(errno));
		return;
	}
	setdebug(desired_debug);
	if (sigprocmask(SIG_UNBLOCK, &set, NULL) < 0)
		ns_error(ns_log_os,
			 "sigprocmask to unblock USR1 and USR2 failed: %s",
			 strerror(errno));
#endif
}

void
toggle_qrylog(void) {
	qrylog = !qrylog;
	ns_notice(ns_log_default, "query log %s\n", qrylog ?"on" :"off");
}

static void
wild(void) {
	ns_panic(ns_log_default, 1, "wild need");
}

/*
 * This is a functional interface to the global needs and options.
 */

static void
init_needs(void) {
	int need;

	for (need = 0; need < main_need_num; need++)
		handlers[need] = wild;
	handlers[main_need_zreload] = ns_zreload;
	handlers[main_need_reload] = ns_reload;
	handlers[main_need_reconfig] = ns_reconfig;
	handlers[main_need_endxfer] = endxfer;
	handlers[main_need_zoneload] = loadxfer;
	handlers[main_need_dump] = doadump;
	handlers[main_need_statsdump] = ns_stats;
	handlers[main_need_statsdumpandclear] = ns_stats_dumpandclear;
	handlers[main_need_exit] = exit_handler;
	handlers[main_need_qrylog] = toggle_qrylog;
	handlers[main_need_debug] = use_desired_debug;
	handlers[main_need_restart] = ns_restart;
	handlers[main_need_reap] = reapchild;
	handlers[main_need_noexpired] = ns_noexpired;
	handlers[main_need_tryxfer] = tryxfer;
}

static void
handle_needs(void) {
	int need, queued = 0;

	ns_debug(ns_log_default, 15, "handle_needs()");
	block_signals();
	for (need = 0; need < main_need_num; need++)
		if ((needs & (1 << need)) != 0) {
			INSIST_ERR(evWaitFor(ev, (void *)handle_needs,
					     need_waitfunc,
					     (void *)handlers[need],
					     NULL) != -1);
			queued++;
		}
	needs = 0;
	unblock_signals();
	ns_debug(ns_log_default, 15, "handle_needs(): queued %d", queued);
	if (queued != 0) {
		INSIST_ERR(evDo(ev, (void *)handle_needs) != -1);
		return;
	}
	ns_panic(ns_log_default, 1, "ns_handle_needs: queued == 0");
}

static void
need_waitfunc(evContext ctx, void *uap, const void *tag) {
	handler hand = (handler) uap;
	time_t begin;
	long syncdelay;

	UNUSED(tag);

	begin = time(NULL);
	(*hand)();
	syncdelay = time(NULL) - begin;
	
	if (syncdelay > MAX_SYNCDELAY)
		ns_notice(ns_log_default, "drained %d queries (delay %ld sec)",
			  drain_all_rcvbuf(ctx), syncdelay);
}

void
ns_need(enum need need) {
	block_signals();
	ns_need_unsafe(need);
	unblock_signals();
}

/* Note: this function should only be called with signals blocked. */
void
ns_need_unsafe(enum need need) {
	needs |= (1 << need);
}

static void
exit_handler(void) {
	needs_exit = 1;
}

void
ns_setoption(int option) {
	ns_warning(ns_log_default, "used obsolete ns_setoption(%d)", option);
}

void
writestream(struct qstream *sp, const u_char *msg, int msglen) {
	if (sq_openw(sp, msglen + INT16SZ) == -1) {
		sq_remove(sp);
		return;
	}
	if (sq_write(sp, msg, msglen) == -1) {
		sq_remove(sp);
		return;
	}
	sq_writeh(sp, sq_flushw);
}

static int
only_digits(const char *s) {
	if (*s == '\0')
		return (0);
	while (*s != '\0') {
		if (!isdigit(*s))
			return (0);
		s++;
	}
	return (1);
}
#if defined(__GNUC__) && defined(__BOUNDS_CHECKING_ON)
	/* Use bounds checking malloc, etc. */
void *
memget(size_t len) {
	return (malloc(len));
}

void
memput(void *addr, size_t len) {
	free(addr);
}

int
meminit(size_t init_max_size, size_t target_size) {
	return (0);
}

void *
memget_debug(size_t size, const char *file, int line) {
	void *ptr;
	ptr = __memget(size);
	fprintf(stderr, "%s:%d: memget(%lu) -> %p\n", file, line,
		(u_long)size, ptr);
	return (ptr);
}

void
memput_debug(void *ptr, size_t size, const char *file, int line) {
	fprintf(stderr, "%s:%d: memput(%p, %lu)\n", file, line, ptr,
		(u_long)size);
	__memput(ptr, size);
}

void
memstats(FILE *out) {
	fputs("No memstats\n", out);
}
#endif

#ifndef HAVE_CUSTOM
/* Standard implementation has nothing here */
static void
custom_init(void) {
	/* Noop. */
}

static void
custom_shutdown(void) {
	/* Noop. */
}
#endif
