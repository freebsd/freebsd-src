/*
 * Copyright (c) 1999-2000 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 */

/*
**  LIBMILTER.H -- include file for mail filter library functions
*/

#ifndef _LIBMILTER_H
# define _LIBMILTER_H	1
#ifdef _DEFINE
# define EXTERN
# define INIT(x)	= x
# ifndef lint
static char MilterlId[] = "@(#)$Id: libmilter.h,v 8.3.6.9 2000/09/01 00:49:04 ca Exp $";
# endif /* ! lint */
#else /* _DEFINE */
# define EXTERN extern
# define INIT(x)
#endif /* _DEFINE */


#define NOT_SENDMAIL	1
#define _SOCK_ADDR	union bigsockaddr
#include "sendmail.h"

#include "libmilter/milter.h"

#ifndef __P
# include "sendmail/cdefs.h"
#endif /* ! __P */
#include "sendmail/useful.h"

# define ValidSocket(sd)	((sd) >= 0)
# define INVALID_SOCKET		-1
# define MI_SOCK_READ(s, b, l)	(read(s, b, l))
# define MI_SOCK_WRITE(s, b, l)	(write(s, b, l))

# define thread_create(ptid,wr,arg) pthread_create(ptid, NULL, wr, arg)
# define sthread_get_id()	pthread_self()

#include <sys/time.h>

/* version info */
#define MILTER_PRODUCT_NAME	"libmilter"
#define MILTER_VERSION		100

/* some defaults */
#define MI_TIMEOUT	1800		/* default timeout for read/write */
#define MI_CHK_TIME	5		/* checking whether to terminate */

#if SOMAXCONN > 20
# define MI_SOMAXCONN	SOMAXCONN
#else /* SOMAXCONN */
# define MI_SOMAXCONN	20
#endif /* SOMAXCONN */

/* maximum number of repeated failures in mi_listener() */
#define MAX_FAILS_M	16	/* malloc() */
#define MAX_FAILS_T	16	/* thread creation */

/* internal "commands", i.e., error codes */
#define SMFIC_TIMEOUT	((char) 1)	/* timeout */
#define SMFIC_SELECT	((char) 2)	/* select error */
#define SMFIC_MALLOC	((char) 3)	/* malloc error */
#define SMFIC_RECVERR	((char) 4)	/* recv() error */
#define SMFIC_EOF	((char) 5)	/* eof */
#define SMFIC_UNKNERR	((char) 6)	/* unknown error */
#define SMFIC_TOOBIG	((char) 7)	/* body chunk too big */
#define SMFIC_VALIDCMD	' '		/* first valid command */

/* hack */
#define smi_log		syslog
#define milter_ret	int
#define SMI_LOG_ERR	LOG_ERR
#define SMI_LOG_FATAL	LOG_ERR
#define SMI_LOG_WARN	LOG_WARNING
#define SMI_LOG_INFO	LOG_INFO
#define SMI_LOG_DEBUG	LOG_DEBUG

/* stop? */
#define MILTER_CONT	0
#define MILTER_STOP	1
#define MILTER_ABRT	2

/* functions */
extern int	mi_handle_session __P((SMFICTX_PTR));
extern int	mi_engine __P((SMFICTX_PTR));
extern int	mi_listener __P((char *, int, smfiDesc_ptr, time_t, int));
extern void	mi_clr_macros __P((SMFICTX_PTR, int));
extern int	mi_stop __P((void));
extern int	mi_control_startup __P((char *));
extern void	mi_stop_milters __P((int));
extern void	mi_clean_signals __P((void));
extern struct hostent *mi_gethostbyname __P((char *, int));
extern void	mi_closener __P((void));

/* communication functions */
extern char	*mi_rd_cmd __P((socket_t, struct timeval *, char *, size_t *, char *));
extern int	mi_wr_cmd __P((socket_t, struct timeval *, int, char *, size_t));
extern bool	mi_sendok __P((SMFICTX_PTR, int));

#endif /* !_LIBMILTER_H */
