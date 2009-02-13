/*
 * Copyright (c) 2000-2001 Sendmail, Inc. and its suppliers.
 *      All rights reserved.
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 * $Id: fvwrite.h,v 1.7 2001/03/02 00:18:19 ca Exp $
 */

/* I/O descriptors for sm_fvwrite() */
struct sm_iov
{
	void	*iov_base;
	size_t	iov_len;
};
struct sm_uio
{
	struct sm_iov	*uio_iov;
	int		uio_iovcnt;
	int		uio_resid;
};

extern int sm_fvwrite __P((SM_FILE_T *, int, struct sm_uio *));
