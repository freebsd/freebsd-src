/* 
 * $FreeBSD$
 * $Source: /home/ncvs/src/eBones/libexec/rkinitd/rpc.c,v $
 * $Author: gibbs $
 *
 * This file contains the network parts of the rkinit server.
 */

#if !defined(lint) && !defined(SABER) && !defined(LOCORE) && defined(RCS_HDRS)
static char *rcsid = "$FreeBSD$";
#endif /* lint || SABER || LOCORE || RCS_HDRS */

#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <syslog.h>
#include <signal.h>
#include <errno.h>
#include <string.h>

#include <rkinit.h>
#include <rkinit_err.h>
#include <rkinit_private.h>

#include "rkinitd.h"

#define RKINITD_TIMEOUT 60

extern int errno;

static int in;			/* sockets */
static int out;

static char errbuf[BUFSIZ];

void error();

#ifdef __STDC__
static void timeout(int signal)
#else
static void timeout(signal)
	int signal;
#endif /* __STDC__ */
{
    syslog(LOG_WARNING, "rkinitd timed out.\n");
    exit(1);

    return;
}

/*
 * This function does all the network setup for rkinitd.
 * It returns true if we were started from inetd, or false if 
 * we were started from the commandline.
 * It causes the program to exit if there is an error. 
 */
#ifdef __STDC__
int setup_rpc(int notimeout)		
#else
int setup_rpc(notimeout)
  int notimeout; /* True if we should not timeout */
#endif /* __STDC__ */
{
    struct itimerval timer;	/* Time structure for timeout */

    /* For now, support only inetd. */
    in = 0;
    out = 1;

    if (! notimeout) {
	SBCLEAR(timer);

	/* Set up an itimer structure to send an alarm signal after timeout
	   seconds. */
	timer.it_interval.tv_sec = RKINITD_TIMEOUT;
	timer.it_interval.tv_usec = 0;
	timer.it_value = timer.it_interval;
	
	/* Start the timer. */
	if (setitimer (ITIMER_REAL, &timer, (struct itimerval *)0) < 0) {
	    sprintf(errbuf, "setitimer: %s", sys_errlist[errno]);
	    rkinit_errmsg(errbuf);
	    error();
	    exit(1);
	}

	signal(SIGALRM, timeout);
    }

    return(TRUE);
}

#ifdef __STDC__
void rpc_exchange_version_info(int *c_lversion, int *c_hversion, 
			       int s_lversion, int s_hversion)
#else
void rpc_exchange_version_info(c_lversion, c_hversion, s_lversion, s_hversion)
  int *c_lversion;
  int *c_hversion;
  int s_lversion;
  int s_hversion;
#endif /* __STDC__ */
{
    u_char version_info[VERSION_INFO_SIZE];
    u_int32_t length = sizeof(version_info);
    
    if (rki_get_packet(in, MT_CVERSION, &length, (char *)version_info) !=
	RKINIT_SUCCESS) {
	error();
	exit(1);
    }

    *c_lversion = version_info[0];
    *c_hversion = version_info[1];

    version_info[0] = s_lversion;
    version_info[1] = s_hversion;

    if (rki_send_packet(out, MT_SVERSION, length, (char *)version_info) != 
	RKINIT_SUCCESS) {
	error();
	exit(1);
    }
}
    
#ifdef __STDC__
void rpc_get_rkinit_info(rkinit_info *info)
#else
void rpc_get_rkinit_info(info)
  rkinit_info *info;
#endif /* __STDC__ */
{
    u_int32_t length = sizeof(rkinit_info);
    
    if (rki_get_packet(in, MT_RKINIT_INFO, &length, (char *)info)) {
	error();
	exit(1);
    }
    
    info->lifetime = ntohl(info->lifetime);
}

#ifdef __STDC__
void rpc_send_error(char *errmsg)
#else
void rpc_send_error(errmsg)
  char *errmsg;
#endif /* __STDC__ */
{
    if (rki_send_packet(out, MT_STATUS, strlen(errmsg), errmsg)) {
	error();
	exit(1);
    }
}

#ifdef __STDC__
void rpc_send_success(void)
#else
void rpc_send_success()
#endif /* __STDC__ */
{
    if (rki_send_packet(out, MT_STATUS, 0, "")) {
	error();
	exit(1);
    }
}

#ifdef __STDC__
void rpc_exchange_tkt(KTEXT cip, MSG_DAT *scip)
#else
void rpc_exchange_tkt(cip, scip)
  KTEXT cip;
  MSG_DAT *scip;
#endif /* __STDC__ */
{
    u_int32_t length = MAX_KTXT_LEN;

    if (rki_send_packet(out, MT_SKDC, cip->length, (char *)cip->dat)) {
	error();
	exit(1);
    }
    
    if (rki_get_packet(in, MT_CKDC, &length, (char *)scip->app_data)) {
	error();
	exit(1);
    }
    scip->app_length = length;
}

#ifdef __STDC__
void rpc_getauth(KTEXT auth, struct sockaddr_in *caddr, 
		 struct sockaddr_in *saddr)
#else
void rpc_getauth(auth, caddr, saddr)
  KTEXT auth;
  struct sockaddr_in *caddr;
  struct sockaddr_in *saddr;
#endif /* __STDC__ */
{
    int addrlen = sizeof(struct sockaddr_in);

    if (rki_rpc_get_ktext(in, auth, MT_AUTH)) {
	error();
	exit(1);
    }

    if (getpeername(in, (struct sockaddr *)caddr, &addrlen) < 0) {
	sprintf(errbuf, "getpeername: %s", sys_errlist[errno]);
	rkinit_errmsg(errbuf);
	error();
	exit(1);
    }

    if (getsockname(out, (struct sockaddr *)saddr, &addrlen) < 0) {
	sprintf(errbuf, "getsockname: %s", sys_errlist[errno]);
	rkinit_errmsg(errbuf);
	error();
	exit(1);
    }
}
