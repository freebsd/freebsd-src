/*
 * Copyright (c) 1999 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 *	$Id: bf_portable.h,v 8.6 1999/11/04 19:31:25 ca Exp $
 *
 * Contributed by Exactis.com, Inc.
 *
 */

#ifndef BF_PORTABLE_H
#define BF_PORTABLE_H 1
/*
**  This implementation will behave differently from the Torek-based code in
**  the following major ways:
**   - The buffer size argument to bfopen() will be sent in, sent back,
**     queried, lost, found, subjected to public inquiry, lost again, and
**     finally buried in soft peat and recycled as firelighters.
**   - Errors in creating the file (but not necessarily writing to it) will
**     always be detected and reported synchronously with the bfopen()
*/

/* Linked structure for storing information about each buffered file */
struct bf
{
	FILE		*bf_key;	/* Unused except as a key for lookup */
	bool		bf_committed;	/* buffered file is on disk */
	char		*bf_filename;	/* Name of disk file */
	int		bf_refcount;	/* Reference count */
	struct bf	*bf_cdr;
};

/*
**  Access routines for looking up bf structures
**
**	maybe replace with a faster data structure later
*/

extern void		bfinsert __P((struct bf *));
extern struct bf	*bflookup __P((FILE *));
extern struct bf	*bfdelete __P((FILE *));
#endif /* BF_PORTABLE_H */
