/*
 * Copyright (c) 1992 William Jolitz. All rights reserved.
 * Written by William Jolitz 1/92
 *
 * Redistribution and use in source and binary forms are freely permitted
 * provided that the above copyright notice and attribution and date of work
 * and this paragraph are duplicated in all such forms.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Resource lists.
 *
 *	Usage:
 *		rlist_free(&swapmap, 100, 200);	add space to swapmap
 *		rlist_alloc(&swapmap, 100, &loc); obtain 100 sectors from swap
 *
 *	from: unknown?
 *	$Id: rlist.h,v 1.4 1993/10/16 17:17:32 rgrimes Exp $
 */

#ifndef _SYS_RLIST_H_
#define _SYS_RLIST_H_

/* A resource list element. */
struct rlist {
	unsigned	rl_start;	/* boundaries of extent - inclusive */
	unsigned	rl_end;		/* boundaries of extent - inclusive */
	struct rlist	*rl_next;	/* next list entry, if present */
};

/* Functions to manipulate resource lists.  */
extern rlist_free __P((struct rlist **, unsigned, unsigned));
int rlist_alloc __P((struct rlist **, unsigned, unsigned *));
extern rlist_destroy __P((struct rlist **));


/* heads of lists */
struct rlist *swapmap;

#endif	/* _SYS_RLIST_H_ */
