/* 
 * $FreeBSD$
 * $Source: /home/ncvs/src/eBones/lib/librkinit/rk_util.c,v $
 * $Author: gibbs $
 *
 * This file contains internal routines for general use by the rkinit
 * library and server.  
 *
 * See the comment at the top of rk_lib.c for a description of the naming
 * conventions used within the rkinit library.
 */

#if !defined(lint) && !defined(SABER) && !defined(LOCORE) && defined(RCS_HDRS)
static char *rcsid = "$FreeBSD$";
#endif /* lint || SABER || LOCORE || RCS_HDRS */

#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <setjmp.h>
#include <signal.h>
#include <sys/time.h>

#ifdef DEBUG
#include <syslog.h>
#endif /* DEBUG */

#include <rkinit.h>
#include <rkinit_private.h>
#include <rkinit_err.h>

#define RKINIT_TIMEOUTVAL 60

static char errbuf[BUFSIZ];
static jmp_buf timeout_env;

#ifdef DEBUG
static int _rkinit_server_ = FALSE;

#ifdef __STDC__
void rki_dmsg(char *string)
#else
void rki_dmsg(string)
  char *string;
#endif /* __STDC__ */
{
    if (_rkinit_server_)
	syslog(LOG_NOTICE, string);
    else
	printf("%s\n", string);
}	

#ifdef __STDC__
void rki_i_am_server(void)
#else
void rki_i_am_server()
#endif /* __STDC__ */
{
    _rkinit_server_ = TRUE;
}
#else /* DEBUG */
#ifdef __STDC__
void rki_dmsg(char *string)
#else
void rki_dmsg(string)
  char *string;
#endif /* __STDC__ */
{
    return;
}

#endif /* DEBUG */

#ifdef __STDC__
const char *rki_mt_to_string(int mt)
#else
const char *rki_mt_to_string(mt)
  int mt;
#endif /* __STDC__ */
{
    char *string = 0;

    switch(mt) {
      case MT_STATUS:
	string = "Status message";
	break;
      case MT_CVERSION:
	string = "Client version";
	break;
      case MT_SVERSION:
	string = "Server version";
	break;
      case MT_RKINIT_INFO:
	string = "Rkinit information";
	break;
      case MT_SKDC:
	string = "Server kdc packet";
	break;
      case MT_CKDC:
	string = "Client kdc packet";
	break;
      case MT_AUTH:
	string = "Authenticator";
	break;
      case MT_DROP:
	string = "Drop server";
	break;
      default:
	string = "Unknown message type";
	break;
    }

    return(string);
}      
	      
#ifdef __STDC__
int rki_choose_version(int *version)
#else
int rki_choose_version(version)
  int *version;
#endif /* __STDC__ */
{
    int s_lversion;		/* lowest version number server supports */
    int s_hversion;		/* highest version number server supports */
    int status = RKINIT_SUCCESS;
    
    if ((status = 
	 rki_rpc_exchange_version_info(RKINIT_LVERSION, RKINIT_HVERSION, 
				       &s_lversion, 
				       &s_hversion)) != RKINIT_SUCCESS)
	return(status);
    
    *version = min(RKINIT_HVERSION, s_hversion);
    if (*version < max(RKINIT_LVERSION, s_lversion)) {
	sprintf(errbuf, 
		"Can't run version %d client against version %d server.",
		RKINIT_HVERSION, s_hversion);
	rkinit_errmsg(errbuf);
	status = RKINIT_VERSION;
    }

    return(status);
}

#ifdef __STDC__
int rki_send_rkinit_info(int version, rkinit_info *info)
#else
int rki_send_rkinit_info(version, info)
  int version;
  rkinit_info *info;
#endif /* __STDC__ */
{
    int status = 0;

    if ((status = rki_rpc_send_rkinit_info(info)) != RKINIT_SUCCESS)
	return(status);

    return(rki_rpc_get_status());
}

#ifdef __STDC__
static void rki_timeout(int signal)
#else
static void rki_timeout(signal)
	int signal;
#endif /* __STDC__ */
{
    sprintf(errbuf, "%d seconds exceeded.", RKINIT_TIMEOUTVAL);
    rkinit_errmsg(errbuf);
    longjmp(timeout_env, RKINIT_TIMEOUT);
    return;
}

#ifdef __STDC__
static void set_timer(int secs)
#else
static void set_timer(secs)
  int secs;
#endif /* __STDC__ */
{
    struct itimerval timer;	/* Time structure for timeout */

    /* Set up an itimer structure to send an alarm signal after TIMEOUT
       seconds. */
    timer.it_interval.tv_sec = secs;
    timer.it_interval.tv_usec = 0;
    timer.it_value = timer.it_interval;
    
    (void) setitimer (ITIMER_REAL, &timer, (struct itimerval *)0);
}
    

#ifdef __STDC__ 
void (*rki_setup_timer(jmp_buf env))(int)
#else
void (*rki_setup_timer(env))(int)
  jmp_buf env;
#endif /* __STDC__ */
{
    bcopy((char *)env, (char *)timeout_env, sizeof(jmp_buf));
    set_timer(RKINIT_TIMEOUTVAL);
    return(signal(SIGALRM, rki_timeout));
}

#ifdef __STDC__
void rki_restore_timer(void (*old_alrm)(int))
#else
void rki_restore_timer(old_alrm)
  void (*old_alrm)(int);
#endif /* __STDC__ */
{
    set_timer(0);
    (void) signal(SIGALRM, old_alrm);
}
