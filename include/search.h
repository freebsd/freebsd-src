/*	$NetBSD: search.h,v 1.12 1999/02/22 10:34:28 christos Exp $	*/
/* $FreeBSD: src/include/search.h,v 1.3.2.1 2000/08/17 07:38:34 jhb Exp $ */

/*
 * Written by J.T. Conklin <jtc@netbsd.org>
 * Public domain.
 */

#ifndef _SEARCH_H_
#define _SEARCH_H_

#include <sys/cdefs.h>
#include <machine/ansi.h>

#ifdef	_BSD_SIZE_T_
typedef	_BSD_SIZE_T_	size_t;
#undef	_BSD_SIZE_T_
#endif

typedef struct entry {
	char *key;
	void *data;
} ENTRY;

typedef enum {
	FIND, ENTER
} ACTION;

typedef enum {
	preorder,
	postorder,
	endorder,
	leaf
} VISIT;

#ifdef _SEARCH_PRIVATE
typedef struct node {
	char         *key;
	struct node  *llink, *rlink;
} node_t;
#endif

__BEGIN_DECLS
int	 hcreate __P((size_t));
void	 hdestroy __P((void));
ENTRY	*hsearch __P((ENTRY, ACTION));
void	*tdelete __P((const void *, void **,
		      int (*)(const void *, const void *)));
void	*tfind __P((const void *, void **,
		      int (*)(const void *, const void *)));
void	*tsearch __P((const void *, void **, 
		      int (*)(const void *, const void *)));
void      twalk __P((const void *, void (*)(const void *, VISIT, int)));
__END_DECLS

#endif /* !_SEARCH_H_ */
