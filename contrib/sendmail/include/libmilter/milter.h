/*
 * Copyright (c) 1999-2003 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 *
 *	$Id: milter.h,v 8.37.2.3 2003/12/02 00:19:51 msk Exp $
 */

/*
**  MILTER.H -- Global definitions for mail filter.
*/

#ifndef _LIBMILTER_MILTER_H
# define _LIBMILTER_MILTER_H	1

#include "sendmail.h"
#include "libmilter/mfapi.h"

/* socket and thread portability */
# include <pthread.h>
typedef pthread_t	sthread_t;
typedef int		socket_t;

#if _FFR_MILTER_MACROS_EOM
# define MAX_MACROS_ENTRIES	5	/* max size of macro pointer array */
#else /* _FFR_MILTER_MACROS_EOM */
# define MAX_MACROS_ENTRIES	4	/* max size of macro pointer array */
#endif /* _FFR_MILTER_MACROS_EOM */

/*
**  context for milter
**  implementation hint:
**  macros are stored in mac_buf[] as sequence of:
**  macro_name \0 macro_value
**  (just as read from the MTA)
**  mac_ptr is a list of pointers into mac_buf to the beginning of each
**  entry, i.e., macro_name, macro_value, ...
*/

struct smfi_str
{
	sthread_t	ctx_id;		/* thread id */
	socket_t	ctx_sd;		/* socket descriptor */
	int		ctx_dbg;	/* debug level */
	time_t		ctx_timeout;	/* timeout */
	int		ctx_state;	/* state */
	smfiDesc_ptr	ctx_smfi;	/* filter description */
	unsigned long	ctx_pflags;	/* protocol flags */
	char		**ctx_mac_ptr[MAX_MACROS_ENTRIES];
	char		*ctx_mac_buf[MAX_MACROS_ENTRIES];
	char		*ctx_reply;	/* reply code */
	void		*ctx_privdata;	/* private data */
};

#endif /* ! _LIBMILTER_MILTER_H */
