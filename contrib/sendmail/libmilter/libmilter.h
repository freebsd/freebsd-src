/*
 * Copyright (c) 1999-2003 Sendmail, Inc. and its suppliers.
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

#include <sm/gen.h>

#ifdef _DEFINE
# define EXTERN
# define INIT(x)	= x
SM_IDSTR(MilterlId, "@(#)$Id: libmilter.h,v 8.33.2.13 2003/10/20 21:51:50 msk Exp $")
#else /* _DEFINE */
# define EXTERN extern
# define INIT(x)
#endif /* _DEFINE */


#define NOT_SENDMAIL	1
#define _SOCK_ADDR	union bigsockaddr
#include "sendmail.h"

#include "libmilter/milter.h"

# define ValidSocket(sd)	((sd) >= 0)
# define INVALID_SOCKET		(-1)
# define closesocket		close
# define MI_SOCK_READ(s, b, l)	read(s, b, l)
# define MI_SOCK_READ_FAIL(x)	((x) < 0)
# define MI_SOCK_WRITE(s, b, l)	write(s, b, l)

# define thread_create(ptid,wr,arg) pthread_create(ptid, NULL, wr, arg)
# define sthread_get_id()	pthread_self()

typedef pthread_mutex_t smutex_t;
# define smutex_init(mp)	(pthread_mutex_init(mp, NULL) == 0)
# define smutex_destroy(mp)	(pthread_mutex_destroy(mp) == 0)
# define smutex_lock(mp)	(pthread_mutex_lock(mp) == 0)
# define smutex_unlock(mp)	(pthread_mutex_unlock(mp) == 0)
# define smutex_trylock(mp)	(pthread_mutex_trylock(mp) == 0)

#if _FFR_USE_POLL

# include <poll.h>
# define MI_POLLSELECT  "poll"

# define MI_POLL_RD_FLAGS (POLLIN | POLLPRI)
# define MI_POLL_WR_FLAGS (POLLOUT)
# define MI_MS(timeout)	(((timeout)->tv_sec * 1000) + (timeout)->tv_usec)

# define FD_RD_VAR(rds, excs) struct pollfd rds
# define FD_WR_VAR(wrs) struct pollfd wrs

# define FD_RD_INIT(sd, rds, excs)			\
		(rds).fd = (sd);			\
		(rds).events = MI_POLL_RD_FLAGS;	\
		(rds).revents = 0

# define FD_WR_INIT(sd, wrs)				\
		(wrs).fd = (sd);			\
		(wrs).events = MI_POLL_WR_FLAGS;	\
		(wrs).revents = 0

# define FD_IS_RD_EXC(sd, rds, excs)	\
		(((rds).revents & (POLLERR | POLLHUP | POLLNVAL)) != 0)

# define FD_IS_WR_RDY(sd, wrs)		\
		(((wrs).revents & MI_POLL_WR_FLAGS) != 0)

# define FD_IS_RD_RDY(sd, rds, excs)			\
		(((rds).revents & MI_POLL_RD_FLAGS) != 0)

# define FD_WR_READY(sd, excs, timeout)	\
		poll(&(wrs), 1, MI_MS(timeout))

# define FD_RD_READY(sd, rds, excs, timeout)	\
		poll(&(rds), 1, MI_MS(timeout))

#else /* _FFR_USE_POLL */

# include <sm/fdset.h>
# define MI_POLLSELECT  "select"

# define FD_RD_VAR(rds, excs) fd_set rds, excs
# define FD_WR_VAR(wrs) fd_set wrs

# define FD_RD_INIT(sd, rds, excs)			\
		FD_ZERO(&(rds));			\
		FD_SET((unsigned int) (sd), &(rds));	\
		FD_ZERO(&(excs));			\
		FD_SET((unsigned int) (sd), &(excs))

# define FD_WR_INIT(sd, wrs)			\
		FD_ZERO(&(wrs));			\
		FD_SET((unsigned int) (sd), &(wrs));	\

# define FD_IS_RD_EXC(sd, rds, excs) FD_ISSET(sd, &(excs))
# define FD_IS_WR_RDY(sd, wrs) FD_ISSET((sd), &(wrs))
# define FD_IS_RD_RDY(sd, rds, excs) FD_ISSET((sd), &(rds))

# define FD_WR_READY(sd, wrs, timeout)	\
		select((sd) + 1, NULL, &(wrs), NULL, (timeout))
# define FD_RD_READY(sd, rds, excs, timeout)	\
		select((sd) + 1, &(rds), NULL, &(excs), (timeout))

#endif /* _FFR_USE_POLL */

#include <sys/time.h>

/* version info */
#define MILTER_PRODUCT_NAME	"libmilter"
#define MILTER_VERSION		100

/* some defaults */
#define MI_TIMEOUT	7210		/* default timeout for read/write */
#define MI_CHK_TIME	5		/* checking whether to terminate */

#ifndef MI_SOMAXCONN
# if SOMAXCONN > 20
#  define MI_SOMAXCONN	SOMAXCONN
# else /* SOMAXCONN */
#  define MI_SOMAXCONN	20
# endif /* SOMAXCONN */
#endif /* ! MI_SOMAXCONN */

/* maximum number of repeated failures in mi_listener() */
#define MAX_FAILS_M	16	/* malloc() */
#define MAX_FAILS_T	16	/* thread creation */
#define MAX_FAILS_A	16	/* accept() */
#define MAX_FAILS_S	16	/* select() */

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
#define sm_dprintf	(void) printf
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
extern int	mi_inet_pton __P((int, const char *, void *));
extern void	mi_closener __P((void));
extern int	mi_opensocket __P((char *, int, int, bool, smfiDesc_ptr));

/* communication functions */
extern char	*mi_rd_cmd __P((socket_t, struct timeval *, char *, size_t *, char *));
extern int	mi_wr_cmd __P((socket_t, struct timeval *, int, char *, size_t));
extern bool	mi_sendok __P((SMFICTX_PTR, int));


#endif /* ! _LIBMILTER_H */
