/*	$NetBSD: rpcbind.c,v 1.4 2013/10/19 17:45:00 christos Exp $	*/

/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 * 
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 * 
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 * 
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 * 
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 * 
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */
/*
 * Copyright (c) 1984 - 1991 by Sun Microsystems, Inc.
 */

/* #ident	"@(#)rpcbind.c	1.19	94/04/25 SMI" */

#if 0
#ifndef lint
static	char sccsid[] = "@(#)rpcbind.c 1.35 89/04/21 Copyr 1984 Sun Micro";
#endif
#endif

/*
 * rpcbind.c
 * Implements the program, version to address mapping for rpc.
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <rpc/rpc.h>
#ifdef PORTMAP
#include <netinet/in.h>
#endif
#include <netdb.h>
#include <stdio.h>
#include <netconfig.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <err.h>
#include <util.h>
#include <pwd.h>
#include <semaphore.h>
#include <string.h>
#include <errno.h>
#include "rpcbind.h"

#include <rump/rump.h>
#include <rump/rump_syscalls.h>

/* Global variables */
int debugging = 1;	/* Tell me what's going on */
int doabort = 0;	/* When debugging, do an abort on errors */
rpcblist_ptr list_rbl;	/* A list of version 3/4 rpcbind services */

#include "svc_fdset.h"

/* who to suid to if -s is given */
#define RUN_AS  "daemon"

int runasdaemon = 0;
int insecure = 0;
int oldstyle_local = 0;
int verboselog = 0;

#ifdef WARMSTART
/* Local Variable */
static int warmstart = 0;	/* Grab a old copy of registrations */
#endif

#ifdef PORTMAP
struct pmaplist *list_pml;	/* A list of version 2 rpcbind services */
const char *udptrans;		/* Name of UDP transport */
const char *tcptrans;		/* Name of TCP transport */
const char *udp_uaddr;		/* Universal UDP address */
const char *tcp_uaddr;		/* Universal TCP address */
#endif
static const char servname[] = "sunrpc";

const char rpcbind_superuser[] = "superuser";
const char rpcbind_unknown[] = "unknown";

static int init_transport(struct netconfig *);
static void rbllist_add(rpcprog_t, rpcvers_t, struct netconfig *,
    struct netbuf *);
static void terminate(int);
#if 0
static void parseargs(int, char *[]);
#endif

int rpcbind_main(void *);
int
rpcbind_main(void *arg)
{
	struct netconfig *nconf;
	void *nc_handle;	/* Net config handle */
	struct rlimit rl;
	int maxrec = RPC_MAXDATASIZE;
	extern sem_t gensem;

#if 0
	parseargs(argc, argv);
#endif

	alloc_fdset();

	getrlimit(RLIMIT_NOFILE, &rl);
	if (rl.rlim_cur < 128) {
		if (rl.rlim_max <= 128)
			rl.rlim_cur = rl.rlim_max;
		else
			rl.rlim_cur = 128;
		setrlimit(RLIMIT_NOFILE, &rl);
	}
#if 0
	if (geteuid()) /* This command allowed only to root */
		errx(1, "Sorry. You are not superuser");
#endif
	nc_handle = setnetconfig(); 	/* open netconfig file */
	if (nc_handle == NULL)
		errx(1, "could not read /etc/netconfig");
#ifdef PORTMAP
	udptrans = "";
	tcptrans = "";
#endif

	nconf = getnetconfigent("local");
	if (nconf == NULL)
		errx(1, "can't find local transport");

	rpc_control(RPC_SVC_CONNMAXREC_SET, &maxrec);

	init_transport(nconf);

	while ((nconf = getnetconfig(nc_handle))) {
		if (nconf->nc_flag & NC_VISIBLE)
			init_transport(nconf);
	}
	endnetconfig(nc_handle);

	/* catch the usual termination signals for graceful exit */
	(void) signal(SIGCHLD, reap);
	(void) signal(SIGINT, terminate);
	(void) signal(SIGTERM, terminate);
	(void) signal(SIGQUIT, terminate);
	/* ignore others that could get sent */
	(void) signal(SIGPIPE, SIG_IGN);
	//(void) signal(SIGHUP, SIG_IGN); used by mountd
	(void) signal(SIGUSR1, SIG_IGN);
	(void) signal(SIGUSR2, SIG_IGN);
#ifdef WARMSTART
	if (warmstart) {
		read_warmstart();
	}
#endif
	if (debugging) {
		printf("rpcbind debugging enabled.");
		if (doabort) {
			printf("  Will abort on errors!\n");
		} else {
			printf("\n");
		}
	} else {
		if (daemon(0, 0))
			err(1, "fork failed");
	}

	openlog("rpcbind", 0, LOG_DAEMON);
	pidfile(NULL);

	if (runasdaemon) {
		struct passwd *p;

		if((p = getpwnam(RUN_AS)) == NULL) {
			syslog(LOG_ERR, "cannot get uid of daemon: %m");
			exit(1);
		}
		if (setuid(p->pw_uid) == -1) {
			syslog(LOG_ERR, "setuid to daemon failed: %m");
			exit(1);
		}
	}

	network_init();

	sem_post(&gensem);
	my_svc_run();
	syslog(LOG_ERR, "svc_run returned unexpectedly");
	rpcbind_abort();
	/* NOTREACHED */

	return 0;
}

/*
 * Adds the entry into the rpcbind database.
 * If PORTMAP, then for UDP and TCP, it adds the entries for version 2 also
 * Returns 0 if succeeds, else fails
 */
static int
init_transport(struct netconfig *nconf)
{
	int fd;
	struct t_bind taddr;
	struct addrinfo hints, *res = NULL;
	struct __rpc_sockinfo si;
	SVCXPRT	*my_xprt;
	int aicode, status, addrlen;
	struct sockaddr *sa;
	struct sockaddr_un sun;
	const int one = 1;

	if ((nconf->nc_semantics != NC_TPI_CLTS) &&
		(nconf->nc_semantics != NC_TPI_COTS) &&
		(nconf->nc_semantics != NC_TPI_COTS_ORD))
		return 1;	/* not my type */
#ifdef RPCBIND_DEBUG
	if (debugging) {
		int i;
		char **s;

		(void)fprintf(stderr, "%s: %ld lookup routines :\n",
		    nconf->nc_netid, nconf->nc_nlookups);
		for (i = 0, s = nconf->nc_lookups; i < nconf->nc_nlookups;
		     i++, s++)
			(void)fprintf(stderr, "[%d] - %s\n", i, *s);
	}
#endif

	/*
	 * XXX - using RPC library internal functions.
	 */
	if ((fd = __rpc_nconf2fd(nconf)) < 0) {
		if (errno == EAFNOSUPPORT)
			return 1;
		warn("Cannot create socket for `%s'", nconf->nc_netid);
		return 1;
	}

	if (!__rpc_nconf2sockinfo(nconf, &si)) {
		warnx("Cannot get information for `%s'", nconf->nc_netid);
		return 1;
	}

	if (si.si_af == AF_INET6) {
		/*
		 * We're doing host-based access checks here, so don't allow
		 * v4-in-v6 to confuse things.
		 */
		if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &one,
		    sizeof one) < 0) {
			warn("Can't make socket ipv6 only");
			return 1;
		}
	}


	if (!strcmp(nconf->nc_netid, "local")) {
		(void)memset(&sun, 0, sizeof sun);
		sun.sun_family = AF_LOCAL;
		(void)rump_sys_unlink(_PATH_RPCBINDSOCK);
		(void)strlcpy(sun.sun_path, _PATH_RPCBINDSOCK,
		    sizeof(sun.sun_path));
		sun.sun_len = SUN_LEN(&sun);
		addrlen = sizeof(struct sockaddr_un);
		sa = (struct sockaddr *)&sun;
	} else {
		/* Get rpcbind's address on this transport */

		(void)memset(&hints, 0, sizeof hints);
		hints.ai_flags = AI_PASSIVE;
		hints.ai_family = si.si_af;
		hints.ai_socktype = si.si_socktype;
		hints.ai_protocol = si.si_proto;
		if ((aicode = getaddrinfo(NULL, servname, &hints, &res)) != 0) {
			warnx("Cannot get local address for `%s' (%s)",
			    nconf->nc_netid, gai_strerror(aicode));
			return 1;
		}
		addrlen = res->ai_addrlen;
		sa = (struct sockaddr *)res->ai_addr;
	}

	if (bind(fd, sa, addrlen) < 0) {
		warn("Cannot bind `%s'", nconf->nc_netid);
		if (res != NULL)
			freeaddrinfo(res);
		return 1;
	}
#if 0
	if (sa->sa_family == AF_LOCAL)
		if (rump_sys_chmod(sun.sun_path, S_IRWXU|S_IRWXG|S_IRWXO) == -1)
			warn("Cannot chmod `%s'", sun.sun_path);
#endif

	/* Copy the address */
	taddr.addr.len = taddr.addr.maxlen = addrlen;
	taddr.addr.buf = malloc(addrlen);
	if (taddr.addr.buf == NULL) {
		warn("Cannot allocate memory for `%s' address",
		    nconf->nc_netid);
		if (res != NULL)
			freeaddrinfo(res);
		return 1;
	}
	(void)memcpy(taddr.addr.buf, sa, addrlen);
#ifdef RPCBIND_DEBUG
	if (debugging) {
		/* for debugging print out our universal address */
		char *uaddr;
		struct netbuf nb;

		nb.buf = sa;
		nb.len = nb.maxlen = sa->sa_len;
		uaddr = taddr2uaddr(nconf, &nb);
		(void)fprintf(stderr, "rpcbind: my address is %s\n", uaddr);
		(void)free(uaddr);
	}
#endif

	if (res != NULL)
		freeaddrinfo(res);

	if (nconf->nc_semantics != NC_TPI_CLTS)
		listen(fd, SOMAXCONN);
		
	my_xprt = (SVCXPRT *)svc_tli_create(fd, nconf, &taddr, RPC_MAXDATASIZE,
	    RPC_MAXDATASIZE);
	if (my_xprt == NULL) {
		warnx("Could not create service for `%s'", nconf->nc_netid);
		goto error;
	}

#ifdef PORTMAP
	/*
	 * Register both the versions for tcp/ip, udp/ip and local.
	 */
	if ((strcmp(nconf->nc_protofmly, NC_INET) == 0 &&
		(strcmp(nconf->nc_proto, NC_TCP) == 0 ||
		strcmp(nconf->nc_proto, NC_UDP) == 0)) ||
		strcmp(nconf->nc_netid, "local") == 0) {
		struct pmaplist *pml;

		if (!svc_register(my_xprt, PMAPPROG, PMAPVERS,
			pmap_service, 0)) {
			warn("Could not register on `%s'", nconf->nc_netid);
			goto error;
		}
		pml = malloc(sizeof (struct pmaplist));
		if (pml == NULL) {
			warn("Cannot allocate memory");
			goto error;
		}
		pml->pml_map.pm_prog = PMAPPROG;
		pml->pml_map.pm_vers = PMAPVERS;
		pml->pml_map.pm_port = PMAPPORT;
		if (strcmp(nconf->nc_proto, NC_TCP) == 0) {
			if (tcptrans[0]) {
				warnx(
				    "Cannot have more than one TCP transport");
				free(pml);
				goto error;
			}
			tcptrans = strdup(nconf->nc_netid);
			if (tcptrans == NULL) {
				free(pml);
				warn("Cannot allocate memory");
				goto error;
			}
			pml->pml_map.pm_prot = IPPROTO_TCP;

			/* Let's snarf the universal address */
			/* "h1.h2.h3.h4.p1.p2" */
			tcp_uaddr = taddr2uaddr(nconf, &taddr.addr);
		} else if (strcmp(nconf->nc_proto, NC_UDP) == 0) {
			if (udptrans[0]) {
				free(pml);
				warnx(
				"Cannot have more than one UDP transport");
				goto error;
			}
			udptrans = strdup(nconf->nc_netid);
			if (udptrans == NULL) {
				free(pml);
				warn("Cannot allocate memory");
				goto error;
			}
			pml->pml_map.pm_prot = IPPROTO_UDP;

			/* Let's snarf the universal address */
			/* "h1.h2.h3.h4.p1.p2" */
			udp_uaddr = taddr2uaddr(nconf, &taddr.addr);
		}
		pml->pml_next = list_pml;
		list_pml = pml;

		/* Add version 3 information */
		pml = malloc(sizeof (struct pmaplist));
		if (pml == NULL) {
			warn("Cannot allocate memory");
			goto error;
		}
		pml->pml_map = list_pml->pml_map;
		pml->pml_map.pm_vers = RPCBVERS;
		pml->pml_next = list_pml;
		list_pml = pml;

		/* Add version 4 information */
		pml = malloc(sizeof (struct pmaplist));
		if (pml == NULL) {
			warn("Cannot allocate memory");
			goto error;
		}
		pml->pml_map = list_pml->pml_map;
		pml->pml_map.pm_vers = RPCBVERS4;
		pml->pml_next = list_pml;
		list_pml = pml;

		/* Also add version 2 stuff to rpcbind list */
		rbllist_add(PMAPPROG, PMAPVERS, nconf, &taddr.addr);
	}
#endif

	/* version 3 registration */
	if (!svc_reg(my_xprt, RPCBPROG, RPCBVERS, rpcb_service_3, NULL)) {
		warn("Could not register %s version 3", nconf->nc_netid);
		goto error;
	}
	rbllist_add(RPCBPROG, RPCBVERS, nconf, &taddr.addr);

	/* version 4 registration */
	if (!svc_reg(my_xprt, RPCBPROG, RPCBVERS4, rpcb_service_4, NULL)) {
		warn("Could not register %s version 4", nconf->nc_netid);
		goto error;
	}
	rbllist_add(RPCBPROG, RPCBVERS4, nconf, &taddr.addr);

	/* decide if bound checking works for this transport */
	status = add_bndlist(nconf, &taddr.addr);
#ifdef RPCBIND_DEBUG
	if (debugging) {
		if (status < 0) {
			fprintf(stderr, "Error in finding bind status for %s\n",
				nconf->nc_netid);
		} else if (status == 0) {
			fprintf(stderr, "check binding for %s\n",
				nconf->nc_netid);
		} else if (status > 0) {
			fprintf(stderr, "No check binding for %s\n",
				nconf->nc_netid);
		}
	}
#else
	__USE(status);
#endif
	/*
	 * rmtcall only supported on CLTS transports for now.
	 */
	if (nconf->nc_semantics == NC_TPI_CLTS) {
		status = create_rmtcall_fd(nconf);

#ifdef RPCBIND_DEBUG
		if (debugging) {
			if (status < 0) {
				fprintf(stderr,
				    "Could not create rmtcall fd for %s\n",
					nconf->nc_netid);
			} else {
				fprintf(stderr, "rmtcall fd for %s is %d\n",
					nconf->nc_netid, status);
			}
		}
#endif
	}
	return (0);
error:
	(void)rump_sys_close(fd);
	return (1);
}

static void
rbllist_add(rpcprog_t prog, rpcvers_t vers, struct netconfig *nconf,
	    struct netbuf *addr)
{
	rpcblist_ptr rbl;

	rbl = malloc(sizeof(rpcblist));
	if (rbl == NULL) {
		warn("Out of memory");
		return;
	}

	rbl->rpcb_map.r_prog = prog;
	rbl->rpcb_map.r_vers = vers;
	rbl->rpcb_map.r_netid = strdup(nconf->nc_netid);
	rbl->rpcb_map.r_addr = taddr2uaddr(nconf, addr);
	rbl->rpcb_map.r_owner = strdup(rpcbind_superuser);
	rbl->rpcb_next = list_rbl;	/* Attach to global list */
	list_rbl = rbl;
}

/*
 * Catch the signal and die
 */
static void
terminate(int dummy)
{
#ifdef WARMSTART
	syslog(LOG_ERR,
		"rpcbind terminating on signal. Restart with \"rpcbind -w\"");
	write_warmstart();	/* Dump yourself */
#endif
	exit(2);
}

void
rpcbind_abort()
{
#ifdef WARMSTART
	write_warmstart();	/* Dump yourself */
#endif
	abort();
}

#if 0
/* get command line options */
static void
parseargs(int argc, char *argv[])
{
	int c;

	while ((c = getopt(argc, argv, "dwailLs")) != -1) {
		switch (c) {
		case 'a':
			doabort = 1;	/* when debugging, do an abort on */
			break;		/* errors; for rpcbind developers */
					/* only! */
		case 'd':
			debugging = 1;
			break;
		case 'i':
			insecure = 1;
			break;
		case 'L':
			oldstyle_local = 1;
			break;
		case 'l':
			verboselog = 1;
			break;
		case 's':
			runasdaemon = 1;
			break;
#ifdef WARMSTART
		case 'w':
			warmstart = 1;
			break;
#endif
		default:	/* error */
			fprintf(stderr,	"usage: rpcbind [-Idwils]\n");
			exit (1);
		}
	}
	if (doabort && !debugging) {
	    fprintf(stderr,
		"-a (abort) specified without -d (debugging) -- ignored.\n");
	    doabort = 0;
	}
}
#endif

void
reap(int dummy)
{
	int save_errno = errno;
 
	while (wait3(NULL, WNOHANG, NULL) > 0)
		;       
	errno = save_errno;
}

void
toggle_verboselog(int dummy)
{
	verboselog = !verboselog;
}
