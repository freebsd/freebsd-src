/*
 * Copyright (c) 2006 Sendmail, Inc. and its suppliers.
 *      All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 *	$Id: misc.h,v 1.1 2006/06/28 23:57:59 ca Exp $
 */

#ifndef SM_MISC_H
# define SM_MISC_H 1

int sm_memstat_open __P((void));
int sm_memstat_close __P((void));
int sm_memstat_get __P((char *, long *));

#endif /* ! SM_MISC_H */
