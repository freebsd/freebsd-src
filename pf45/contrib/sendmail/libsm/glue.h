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
 * $Id: glue.h,v 1.6 2001/01/22 23:09:49 ca Exp $
 */

/*
**  The first few FILEs are statically allocated; others are dynamically
**  allocated and linked in via this glue structure.
*/

extern struct sm_glue
{
	struct sm_glue	*gl_next;
	int		gl_niobs;
	SM_FILE_T	*gl_iobs;
} smglue;
