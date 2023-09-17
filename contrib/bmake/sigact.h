/* NAME:
 *      sigact.h - sigaction et al
 *
 * SYNOPSIS:
 *      #include "sigact.h"
 *
 * DESCRIPTION:
 *      This header is the interface to a fake sigaction(2) 
 *      implementation. It provides a POSIX compliant interface 
 *      to whatever signal handling mechanisms are available.
 *      It also provides a Signal() function that is implemented 
 *      in terms of sigaction().
 *      If not using signal(2) as part of the underlying 
 *      implementation (USE_SIGNAL or USE_SIGMASK), and 
 *      NO_SIGNAL is not defined, it also provides a signal() 
 *      function that calls Signal(). 
 *      
 * SEE ALSO:
 *      sigact.c
 */
/*
 * RCSid:
 *      $Id: sigact.h,v 1.4 2021/10/14 19:39:17 sjg Exp $
 */
#ifndef _SIGACT_H
#define _SIGACT_H

#include <sys/cdefs.h>

/*
 * most modern systems use void for signal handlers but
 * not all.
 */
#ifndef SIG_HDLR
# define SIG_HDLR void
#endif

/*
 * if you want to install this header as signal.h,
 * modify this to pick up the original signal.h
 */
#ifndef SIGKILL
# include <signal.h>
#endif
#ifndef SIGKILL
# include <sys/signal.h>
#endif
  
#ifndef SIG_ERR
# define SIG_ERR  (SIG_HDLR (*)())-1
#endif
#ifndef BADSIG
# define BADSIG  SIG_ERR
#endif
    
#ifndef SA_NOCLDSTOP
/* we assume we need the fake sigaction */
/* sa_flags */
#define	SA_NOCLDSTOP	1		/* don't send SIGCHLD on child stop */
#define SA_RESTART	2		/* re-start I/O */

/* sigprocmask flags */
#define	SIG_BLOCK	1
#define	SIG_UNBLOCK	2
#define	SIG_SETMASK	4

/*
 * this is a bit untidy
 */
#ifdef _SIGSET_T_
typedef _SIGSET_T_ sigset_t;
#endif
  
/*
 * POSIX sa_handler should return void, but since we are
 * implementing in terms of something else, it may
 * be appropriate to use the normal SIG_HDLR return type
 */
struct sigaction
{
  SIG_HDLR	(*sa_handler)();
  sigset_t	sa_mask;
  int		sa_flags;
};


int	sigaction	( int /*sig*/, const struct sigaction */*act*/, struct sigaction */*oact*/ );
int	sigaddset	( sigset_t */*mask*/, int /*sig*/ );
int	sigdelset	( sigset_t */*mask*/, int /*sig*/ );
int	sigemptyset	( sigset_t */*mask*/ );
int	sigfillset	( sigset_t */*mask*/ );
int	sigismember	( const sigset_t */*mask*/, int /*sig*/ );
int	sigpending	( sigset_t */*set*/ );
int	sigprocmask	( int how, const sigset_t */*set*/, sigset_t */*oset*/ );
int	sigsuspend	( sigset_t */*mask*/ );
	
#ifndef sigmask
# define sigmask(s)	(1<<((s)-1) & (32 - 1))	/* convert SIGnum to mask */
#endif
#if !defined(NSIG) && defined(_NSIG)
# define NSIG _NSIG
#endif
#endif /* ! SA_NOCLDSTOP */
#endif /* _SIGACT_H */
