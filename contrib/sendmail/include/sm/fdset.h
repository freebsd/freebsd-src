/*
 * Copyright (c) 2001 Sendmail, Inc. and its suppliers.
 *      All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 *	$Id: fdset.h,v 1.3 2001/03/30 23:45:31 geir Exp $
 */

#ifndef	SM_FDSET_H
# define SM_FDSET_H

#  define SM_FD_SET(fd, pfdset)		FD_SET(fd, pfdset)
#  define SM_FD_ISSET(fd, pfdset)	FD_ISSET(fd, pfdset)
#  define SM_FD_SETSIZE	FD_SETSIZE

#endif /* SM_FDSET_H */
