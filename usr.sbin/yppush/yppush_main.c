/*
 * Copyright (c) 1995
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
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
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: yppush_main.c,v 1.4 1996/04/29 05:24:26 wpaul Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include <sys/wait.h>
#include <sys/param.h>
#include <rpc/rpc.h>
#include <rpc/clnt.h>
#include <rpc/pmap_clnt.h>
#include <rpcsvc/yp.h>
struct dom_binding {};
#include <rpcsvc/ypclnt.h>
#include "ypxfr_extern.h"
#include "yppush_extern.h"

#ifndef lint
static const char rcsid[] = "$Id: yppush_main.c,v 1.4 1996/04/29 05:24:26 wpaul Exp $";
#endif

char *progname = "yppush";
int debug = 1;
int _rpcpmstart = 0;
char *yp_dir = _PATH_YP;

char *yppush_mapname = NULL;	/* Map to transfer. */
char *yppush_domain = NULL;	/* Domain in which map resides. */
char *yppush_master = NULL;	/* Master NIS server for said domain. */
int verbose = 0;		/* Toggle verbose mode. */
unsigned long yppush_transid = 0;
int yppush_timeout = 80;	/* Default timeout. */
int yppush_jobs = 0;		/* Number of allowed concurrent jobs. */
int yppush_running_jobs = 0;	/* Number of currently running jobs. */
int yppush_alarm_tripped = 0;

/* Structure for holding information about a running job. */
struct jobs {
	unsigned long tid;
	int sock;
	int port;
	ypxfrstat stat;
	unsigned long prognum;
	char *server;
	char *map;
	int polled;
	struct jobs *next;
};

struct jobs *yppush_joblist;	/* Linked list of running jobs. */

/*
 * Local error messages.
 */
static char *yppusherr_string(err)
	int err;
{
	switch(err) {
	case YPPUSH_TIMEDOUT: return("transfer or callback timed out");
	case YPPUSH_YPSERV:   return("failed to contact ypserv");
	case YPPUSH_NOHOST:   return("no such host");
	case YPPUSH_PMAP:     return("portmapper failure");
	default:              return("unknown error code");
	}
}

/*
 * Report state of a job.
 */
static int yppush_show_status(status, tid)
	ypxfrstat status;
	unsigned long tid;
{
	struct jobs *job;

	job = yppush_joblist;

	while(job) {
		if (job->tid == tid)
			break;
		job = job->next;
	}

	if (job->polled) {
		return(0);
	}

	if (verbose > 1)
		yp_error("Checking return status: Transaction ID: %lu",
								job->tid);
	if (status != YPPUSH_SUCC || verbose) {
		yp_error("Transfer of map %s to server %s %s.",
		 	job->map, job->server, status == YPPUSH_SUCC ?
		 	"succeeded" : "failed");
		yp_error("status returned by ypxfr: %s", status > YPPUSH_AGE ?
			yppusherr_string(status) : 
			ypxfrerr_string(status));
	}

	job->polled = 1;

	svc_unregister(job->prognum, 1);
	
	yppush_running_jobs--;
	return(0);
}

/* Exit routine. */
static void yppush_exit(now)
	int now;
{
	struct jobs *jptr;
	int still_pending = 1;

	/* Let all the information trickle in. */
	while(!now && still_pending) {
		jptr = yppush_joblist;
		still_pending = 0;
		while (jptr) {
			if (jptr->polled == 0) {
				still_pending++;
				if (verbose > 1)
					yp_error("%s has not responded",
						  jptr->server);
			} else {
				if (verbose > 1)
					yp_error("%s has responded",
						  jptr->server);
			}
			jptr = jptr->next;
		}
		if (still_pending) {
			if (verbose > 1)
				yp_error("%d transfer%sstill pending",
					still_pending,
					still_pending > 1 ? "s " : " ");
			yppush_alarm_tripped = 0;
			alarm(YPPUSH_RESPONSE_TIMEOUT);
			pause();
			alarm(0);
			if (yppush_alarm_tripped == 1) {
				yp_error("timed out");
				now = 1;
			}
		} else {
			if (verbose)
				yp_error("all transfers complete");
			break;
		}
	}


	/* All stats collected and reported -- kill all the stragglers. */
	jptr = yppush_joblist;
	while(jptr) {
		if (!jptr->polled)
			yp_error("warning: exiting with transfer \
to %s (transid = %lu) still pending.", jptr->server, jptr->tid);
		svc_unregister(jptr->prognum, 1);
		jptr = jptr->next;
	}

	exit(0);
}

/*
 * Handler for 'normal' signals.
 */

static void handler(sig)
	int sig;
{
	if (sig == SIGTERM || sig == SIGINT || sig == SIGABRT) {
		yppush_jobs = 0;
		yppush_exit(1);
	}

	if (sig == SIGALRM) {
		alarm(0);
		yppush_alarm_tripped++;
	}

	return;
}

/*
 * Dispatch loop for callback RPC services.
 */
static void yppush_svc_run()
{
#ifdef FD_SETSIZE
	fd_set readfds;
#else 
	int readfds; 
#endif /* def FD_SETSIZE */
	struct timeval timeout;

	timeout.tv_usec = 0;
	timeout.tv_sec = 5;

retry:
#ifdef FD_SETSIZE
	readfds = svc_fdset;
#else
	readfds = svc_fds;
#endif /* def FD_SETSIZE */
	switch (select(_rpc_dtablesize(), &readfds, NULL, NULL, &timeout)) {
	case -1:
		if (errno == EINTR)
			goto retry;
		yp_error("select failed: %s", strerror(errno));
		break;
	case 0:
		yp_error("select() timed out");
		break;
	default:
		svc_getreqset(&readfds);
		break;
	}
	return;
}

/*
 * Special handler for asynchronous socket I/O. We mark the
 * sockets of the callback handlers as O_ASYNC and handle SIGIO
 * events here, which will occur when the callback handler has
 * something interesting to tell us.
 */
static void async_handler(sig)
	int sig;
{
	yppush_svc_run();

	/* reset any pending alarms. */
	alarm(0);
	yppush_alarm_tripped++;
	kill(getpid(), SIGALRM);
	return;
}

/*
 * RPC service routines for callbacks.
 */
void *
yppushproc_null_1_svc(void *argp, struct svc_req *rqstp)
{
	static char * result;
	/* Do nothing -- RPC conventions call for all a null proc. */
	return((void *) &result);
}

void *
yppushproc_xfrresp_1_svc(yppushresp_xfr *argp, struct svc_req *rqstp)
{
	static char * result;
	yppush_show_status(argp->status, argp->transid);
	return((void *) &result);
}

/*
 * Transmit a YPPROC_XFR request to ypserv.
 */   
static int yppush_send_xfr(job)
	struct jobs *job;
{
	ypreq_xfr req;
/*	ypresp_xfr *resp; */
	DBT key, data;
	CLIENT *clnt;
	struct rpc_err err;
	struct timeval timeout;

	timeout.tv_usec = 0;
	timeout.tv_sec = 0;

	/*
	 * The ypreq_xfr structure has a member of type map_parms,
	 * which seems to require the order number of the map.
	 * It isn't actually used at the other end (at least the
	 * FreeBSD ypserv doesn't use it) but we fill it in here
	 * for the sake of completeness.
	 */
	key.data = "YP_LAST_MODIFIED";
	key.size = sizeof ("YP_LAST_MODIFIED") - 1;

	if (yp_get_record(yppush_domain, yppush_mapname, &key, &data,
			  1) != YP_TRUE) {
		yp_error("failed to read order number from %s: %s: %s",
			  yppush_mapname, yperr_string(yp_errno),
			  strerror(errno));
		return(1);
	}

	/* Fill in the request arguments */
	req.map_parms.ordernum = atoi(data.data);
	req.map_parms.domain = yppush_domain;
	req.map_parms.peer = yppush_master;
	req.map_parms.map = job->map;
	req.transid = job->tid;
	req.prog = job->prognum;
	req.port = job->port;

	/* Get a handle to the remote ypserv. */
	if ((clnt = clnt_create(job->server, YPPROG, YPVERS, "udp")) == NULL) {
		yp_error("%s: %s",job->server,clnt_spcreateerror("couldn't \
create udp handle to NIS server"));
		switch(rpc_createerr.cf_stat) {
			case RPC_UNKNOWNHOST:
				job->stat = YPPUSH_NOHOST;
				break;
			case RPC_PMAPFAILURE:
				job->stat = YPPUSH_PMAP;
				break;
			default:
				job->stat = YPPUSH_RPC;
				break;
			}
		return(1);
	}

	/*
	 * Reduce timeout to nothing since we may not
	 * get a response from ypserv and we don't want to block.
	 */
	if (clnt_control(clnt, CLSET_TIMEOUT, (char *)&timeout) == FALSE)
		yp_error("failed to set timeout on ypproc_xfr call");

	/* Invoke the ypproc_xfr service. */
	if (ypproc_xfr_2(&req, clnt) == NULL) {
		clnt_geterr(clnt, &err);
		if (err.re_status != RPC_SUCCESS &&
		    err.re_status != RPC_TIMEDOUT) {
			yp_error("%s: %s", job->server, clnt_sperror(clnt,
							"yp_xfr failed"));
			job->stat = YPPUSH_YPSERV;
			clnt_destroy(clnt);
			return(1);
		}
	}

	clnt_destroy(clnt);

	return(0);
}

/*
 * Main driver function. Register the callback service, add the transfer
 * request to the internal list, send the YPPROC_XFR request to ypserv
 * do other magic things.
 */
int yp_push(server, map, tid)
	char *server;
	char *map;
	unsigned long tid;
{
	unsigned long prognum;
	int sock = RPC_ANYSOCK;
	SVCXPRT *xprt;
	struct jobs *job;

	/*
	 * Register the callback service on the first free
	 * transient program number.
	 */
	xprt = svcudp_create(sock);
	for (prognum = 0x40000000; prognum < 0x5FFFFFFF; prognum++) {
		if (svc_register(xprt, prognum, 1,
		    yppush_xfrrespprog_1, IPPROTO_UDP) == TRUE)
			break;
	}

	/* Register the job in our linked list of jobs. */
	if ((job = (struct jobs *)malloc(sizeof (struct jobs))) == NULL) {
		yp_error("malloc failed: %s", strerror(errno));
		yppush_exit(1);
	}

	/* Initialize the info for this job. */
	job->stat = 0;
	job->tid = tid;
	job->port = xprt->xp_port;
	job->sock = xprt->xp_sock; /*XXX: Evil!! EEEEEEEVIL!!! */
	job->server = strdup(server);
	job->map = strdup(map);
	job->prognum = prognum;
	job->polled = 0;
	job->next = yppush_joblist;
	yppush_joblist = job;

	/*
	 * Set the RPC sockets to asynchronous mode. This will
	 * cause the system to smack us with a SIGIO when an RPC
	 * callback is delivered. This in turn allows us to handle
	 * the callback even though we may be in the middle of doing
	 * something else at the time.
	 *
	 * XXX This is a horrible thing to do for two reasons,
	 * both of which have to do with portability:
	 * 1) We really ought not to be sticking our grubby mits
	 *    into the RPC service transport handle like this.
	 * 2) Even in this day and age, there are still some *NIXes
	 *    that don't support async socket I/O.
	 */
	if (fcntl(xprt->xp_sock, F_SETOWN, getpid()) == -1 ||
	    fcntl(xprt->xp_sock, F_SETFL, O_ASYNC) == -1) {
		yp_error("failed to set async I/O mode: %s",
			 strerror(errno));
		yppush_exit(1);
	}

	if (verbose) {
		yp_error("initiating transfer: %s -> %s (transid = %lu)",
			yppush_mapname, server, tid);
	}

	/*
	 * Send the XFR request to ypserv. We don't have to wait for
	 * a response here since we can handle them asynchronously.
	 */

	if (yppush_send_xfr(job)){
		/* Transfer request blew up. */
		yppush_show_status(job->stat ? job->stat :
			YPPUSH_YPSERV,job->tid);
	} else {
		if (verbose > 1)
			yp_error("%s has been called", server);
	}

	return(0);
}

/*
 * Called for each entry in the ypservers map from yp_get_map(), which
 * is our private yp_all() routine.
 */
int yppush_foreach(status, key, keylen, val, vallen, data)
	int status;
	char *key;
	int keylen;
	char *val;
	int vallen;
	char *data;
{
	char server[YPMAXRECORD + 2];

	if (status != YP_TRUE)
		return (status);

	snprintf(server, sizeof(server), "%.*s", vallen, val);

	/*
	 * Restrict the number of concurrent jobs. If yppush_jobs number
	 * of jobs have already been dispatched and are still pending,
	 * wait for one of them to finish so we can reuse its slot.
	 */
	if (yppush_jobs <= 1) {
		yppush_alarm_tripped = 0;
		while (!yppush_alarm_tripped && yppush_running_jobs) {
			alarm(yppush_timeout);
			yppush_alarm_tripped = 0;
			pause();
			alarm(0);
		}
	} else {
		yppush_alarm_tripped = 0;
		while (!yppush_alarm_tripped && yppush_running_jobs >= yppush_jobs) {
			alarm(yppush_timeout);
			yppush_alarm_tripped = 0;
			pause();
			alarm(0);
		}
	}

	/* Cleared for takeoff: set everything in motion. */
	if (yp_push(&server, yppush_mapname, yppush_transid))
		return(yp_errno);

	/* Bump the job counter and transaction ID. */
	yppush_running_jobs++;
	yppush_transid++;
	return (0);
}

static void usage()
{
	fprintf (stderr, "%s: [-d domain] [-t timeout] [-j #parallel jobs] \
[-h host] [-p path] mapname\n", progname);
	exit(1);
}

/*
 * Entry point. (About time!)
 */
main(argc,argv)
	int argc;
	char *argv[];
{
	int ch;
	DBT key, data;
	char myname[MAXHOSTNAMELEN];
	struct hostlist {
		char *name;
		struct hostlist *next;
	};
	struct hostlist *yppush_hostlist = NULL;
	struct hostlist *tmp;
	struct sigaction sa;

	while ((ch = getopt(argc, argv, "d:j:p:h:t:v")) != EOF) {
		switch(ch) {
		case 'd':
			yppush_domain = optarg;
			break;
		case 'j':
			yppush_jobs = atoi(optarg);
			if (yppush_jobs <= 0)
				yppush_jobs = 1;
			break;
		case 'p':
			yp_dir = optarg;
			break;
		case 'h': /* we can handle multiple hosts */
			if ((tmp = (struct hostlist *)malloc(sizeof(struct hostlist))) == NULL) {
				yp_error("malloc() failed: %s", strerror(errno));
				yppush_exit(1);
			}
			tmp->name = strdup(optarg);
			tmp->next = yppush_hostlist;
			yppush_hostlist = tmp;
			break;
		case 't':
			yppush_timeout = atoi(optarg);
			break;
		case 'v':
			verbose++;
			break;
		default:
			usage();
			break;
		}
	}

	argc -= optind;
	argv += optind;

	yppush_mapname = argv[0];

	if (yppush_mapname == NULL) {
	/* "No guts, no glory." */
		usage();
	}

	/*
	 * If no domain was specified, try to find the default
	 * domain. If we can't find that, we're doomed and must bail.
	 */
	if (yppush_domain == NULL) {
		char *yppush_check_domain;
		if (!yp_get_default_domain(&yppush_check_domain) &&
			!_yp_check(&yppush_check_domain)) {
			yp_error("no domain specified and NIS not running");
			usage();
		} else
			yp_get_default_domain(&yppush_domain);
	}

	/* Check to see that we are the master for this map. */

	if (gethostname ((char *)&myname, sizeof(myname))) {
		yp_error("failed to get name of local host: %s",
			strerror(errno));
		yppush_exit(1);
	}

	key.data = "YP_MASTER_NAME";
	key.size = sizeof("YP_MASTER_NAME") - 1;

	if (yp_get_record(yppush_domain, yppush_mapname,
			  &key, &data, 1) != YP_TRUE) {
		yp_error("couldn't open %s map: %s", yppush_mapname,
			 strerror(errno));
		yppush_exit(1);
	}

	if (strncmp(myname, data.data, data.size)) {
		yp_error("warning: this host is not the master for %s",
							yppush_mapname);
#ifdef NITPICKY
		yppush_exit(1);
#endif
	}

	yppush_master = malloc(data.size + 1);
	strncpy(yppush_master, data.data, data.size);
	yppush_master[data.size] = '\0';

	/* Install some handy handlers. */
	signal(SIGALRM, handler);
	signal(SIGTERM, handler);
	signal(SIGINT, handler);
	signal(SIGABRT, handler);

	/*
	 * Set up the SIGIO handler. Make sure that some of the
	 * other signals are blocked while the handler is running so
	 * select() doesn't get interrupted.
	 */
	sigemptyset(&sa.sa_mask);
	sigaddset(&sa.sa_mask, SIGIO); /* Goes without saying. */
	sigaddset(&sa.sa_mask, SIGPIPE);
	sigaddset(&sa.sa_mask, SIGCHLD);
	sigaddset(&sa.sa_mask, SIGALRM);
	sigaddset(&sa.sa_mask, SIGINT);
	sa.sa_handler = async_handler;

	sigaction(SIGIO, &sa, NULL);

	/* set initial transaction ID */
	time(&yppush_transid);

	if (yppush_hostlist) {
	/*
	 * Host list was specified on the command line:
	 * kick off the transfers by hand.
	 */
		tmp = yppush_hostlist;
		while(tmp) {
			yppush_foreach(YP_TRUE, NULL, 0, tmp->name,
							strlen(tmp->name));
			tmp = tmp->next;
		}
	} else {
	/*
	 * Do a yp_all() on the ypservers map and initiate a ypxfr
	 * for each one.
	 */
		ypxfr_get_map("ypservers", yppush_domain,
			      "localhost", yppush_foreach);
	}

	if (verbose > 1)
		yp_error("all jobs dispatched");

	/* All done -- normal exit. */
	yppush_exit(0);

	/* Just in case. */
	exit(0);
}
