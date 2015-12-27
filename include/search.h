/*-
 * Written by J.T. Conklin <jtc@NetBSD.org>
 * Public domain.
 *
 *	$NetBSD: search.h,v 1.16 2005/02/03 04:39:32 perry Exp $
 * $FreeBSD$
 */

#ifndef _SEARCH_H_
#define _SEARCH_H_

#include <sys/cdefs.h>
#include <sys/_types.h>

#ifndef _SIZE_T_DECLARED
typedef	__size_t	size_t;
#define	_SIZE_T_DECLARED
#endif

typedef	struct entry {
	char	*key;
	void	*data;
} ENTRY;

typedef	enum {
	FIND, ENTER
} ACTION;

typedef	enum {
	preorder,
	postorder,
	endorder,
	leaf
} VISIT;

#ifdef _SEARCH_PRIVATE
typedef	struct node {
	void         *key;
	struct node  *llink, *rlink;
	signed char   balance;
} node_t;

struct que_elem {
	struct que_elem *next;
	struct que_elem *prev;
};
#endif

#if __BSD_VISIBLE
struct hsearch_data {
	struct __hsearch *__hsearch;
};
#endif

__BEGIN_DECLS
int	 hcreate(size_t);
void	 hdestroy(void);
ENTRY	*hsearch(ENTRY, ACTION);
void	 insque(void *, void *);
void	*lfind(const void *, const void *, size_t *, size_t,
	    int (*)(const void *, const void *));
void	*lsearch(const void *, void *, size_t *, size_t,
	    int (*)(const void *, const void *));
void	 remque(void *);
void	*tdelete(const void * __restrict, void ** __restrict,
	    int (*)(const void *, const void *));
void	*tfind(const void *, void * const *,
	    int (*)(const void *, const void *));
void	*tsearch(const void *, void **, int (*)(const void *, const void *));
void	 twalk(const void *, void (*)(const void *, VISIT, int));

#if __BSD_VISIBLE
int	 hcreate_r(size_t, struct hsearch_data *);
void	 hdestroy_r(struct hsearch_data *);
int	 hsearch_r(ENTRY, ACTION, ENTRY **, struct hsearch_data *);
#endif

__END_DECLS

#endif /* !_SEARCH_H_ */
