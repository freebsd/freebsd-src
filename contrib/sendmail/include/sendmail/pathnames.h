/*-
 * Copyright (c) 1998-2000 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 *
 *	$Id: pathnames.h,v 8.16.8.8 2000/09/28 21:26:39 gshapiro Exp $
 */


# ifndef _PATH_SENDMAILCF
#  if defined(USE_VENDOR_CF_PATH) && defined(_PATH_VENDOR_CF)
#   define _PATH_SENDMAILCF	_PATH_VENDOR_CF
#  else /* defined(USE_VENDOR_CF_PATH) && defined(_PATH_VENDOR_CF) */
#   define _PATH_SENDMAILCF	"/etc/mail/sendmail.cf"
#  endif /* defined(USE_VENDOR_CF_PATH) && defined(_PATH_VENDOR_CF) */
# endif /* ! _PATH_SENDMAILCF */

# ifndef _PATH_SENDMAILPID
#  ifdef BSD4_4
#   define _PATH_SENDMAILPID	"/var/run/sendmail.pid"
#  else /* BSD4_4 */
#   define _PATH_SENDMAILPID	"/etc/mail/sendmail.pid"
#  endif /* BSD4_4 */
# endif /* ! _PATH_SENDMAILPID */

# ifndef _PATH_HOSTS
#  define _PATH_HOSTS		"/etc/hosts"
# endif /* ! _PATH_HOSTS */


