/*
** sptree.h:  The following type declarations provide the binary tree
**  representation of event-sets or priority queues needed by splay trees
**
**  assumes that data and datb will be provided by the application
**  to hold all application specific information
**
**  assumes that key will be provided by the application, comparable
**  with the compare function applied to the addresses of two keys.
*/

# ifndef SPTREE_H
# define SPTREE_H

typedef struct _spblk
{
    struct _spblk	* leftlink;
    struct _spblk	* rightlink;
    struct _spblk	* uplink;

    univptr_t	key;		/* formerly time/timetyp */
    univptr_t data;		/* formerly aux/auxtype */
    univptr_t datb;
} SPBLK;

typedef struct
{
    SPBLK	* root;		/* root node */

    /* Statistics, not strictly necessary, but handy for tuning  */

    int		lookups;	/* number of splookup()s */
    int		lkpcmps;	/* number of lookup comparisons */
    
    int		enqs;		/* number of spenq()s */
    int		enqcmps;	/* compares in spenq */
    
    int		splays;
    int		splayloops;

} SPTREE;

#if defined(__STDC__)
#define __proto(x)	x
#else
#define __proto(x)	()
#endif

/* sptree.c */
/* init tree */
extern SPTREE * __spinit __proto((void));
/* find key in a tree */
extern SPBLK * __splookup __proto((univptr_t, SPTREE *));
/* enter an item, allocating or replacing */
extern SPBLK * __spadd __proto((univptr_t, univptr_t, univptr_t, SPTREE *));
/* scan forward through tree */
extern void __spscan __proto((void (*) __proto((SPBLK *)), SPBLK *, SPTREE *));
/* return tree statistics */
extern char *__spstats __proto((SPTREE *));
/* delete node from tree */
extern void __spdelete __proto((SPBLK *, SPTREE *));

#undef __proto

# endif /* SPTREE_H */
