/*-
 * Copyright (c) 1998 Sendmail, Inc.  All rights reserved.
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 *
 *	@(#)pathnames.h	8.8 (Berkeley) 5/19/98
 */

#ifndef _PATH_SENDMAILCF
# if defined(USE_VENDOR_CF_PATH) && defined(_PATH_VENDOR_CF)
#  define _PATH_SENDMAILCF	_PATH_VENDOR_CF
# else
#  define _PATH_SENDMAILCF	"/etc/sendmail.cf"
# endif
#endif

#ifndef _PATH_SENDMAILPID
# ifdef BSD4_4
#  define _PATH_SENDMAILPID	"/var/run/sendmail.pid"
# else
#  define _PATH_SENDMAILPID	"/etc/sendmail.pid"
# endif
#endif

#ifndef _PATH_HOSTS
# define _PATH_HOSTS		"/etc/hosts"
#endif
