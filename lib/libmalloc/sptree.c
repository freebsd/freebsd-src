/*
 *  This file contains a few splay tree routines snarfed from David
 *  Brower's package, with globals renamed to keep them internal to the
 *  malloc, and not clash with similar routines that the application may
 *  use. The comments have been left with the original names - most of
 *  the renaming just involved prepending an __ before the name -
 *  spinstall got remapped to __spadd. Function prototypes added for
 *  external declarations. - Mark Moraes.
 */
/*
 * spdaveb.c -- daveb's new splay tree functions.
 *
 * The functions in this file provide an interface that is nearly
 * the same as the hash library I swiped from mkmf, allowing
 * replacement of one by the other.  Hey, it worked for me!
 *
 * splookup() -- given a key, find a node in a tree.
 * spinstall() -- install an item in the tree, overwriting existing value.
 * spfhead() -- fast (non-splay) find the first node in a tree.
 * spscan() -- forward scan tree from the head.
 * spfnext() -- non-splaying next.
 * spstats() -- make char string of stats for a tree.
 *
 * Written by David Brower, daveb@rtech.uucp 1/88.
 */
/*LINTLIBRARY*/

#if defined(__STDC__) && defined(ANSI_TYPES)
#include <stddef.h>
#endif
#include <stdio.h>

#include "defs.h"

# define COMPARE(a,b)	(((char *) (a)) - ((char *) (b)))

# include	"sptree.h"

/* insert item into the tree */
static SPBLK * spenq proto((SPBLK *, SPTREE *));
/* return and remove lowest item in subtree */
static SPBLK * spdeq proto((SPBLK **));
/* reorganize tree */
static void splay proto((SPBLK *, SPTREE *));
/* fast non-splaying head */
static SPBLK * spfhead proto((SPTREE *));
/* fast non-splaying next */
static SPBLK * spfnext proto((SPBLK *));

/* USER SUPPLIED! */

extern univptr_t emalloc proto((size_t));


/*----------------
 *
 * splookup() -- given key, find a node in a tree.
 *
 *	Splays the found node to the root.
 */
SPBLK *
__splookup( key, q )
REGISTER univptr_t key;
REGISTER SPTREE *q;

{
    REGISTER SPBLK * n;
    REGISTER int Sct;
    REGISTER int c;

    /* find node in the tree */
    n = q->root;
    c = ++(q->lkpcmps);
    q->lookups++;
    while( n && (Sct = COMPARE( key, n->key ) ) )
    {
	c++;
	n = ( Sct < 0 ) ? n->leftlink : n->rightlink;
    }
    q->lkpcmps = c;

    /* reorganize tree around this node */
    if( n != NULL )
	splay( n, q );

    return( n );
}



/*----------------
 *
 * spinstall() -- install an entry in a tree, overwriting any existing node.
 *
 *	If the node already exists, replace its contents.
 *	If it does not exist, then allocate a new node and fill it in.
 */

SPBLK *
__spadd( key, data, datb, q )

REGISTER univptr_t key;
REGISTER univptr_t data;
REGISTER univptr_t datb;
REGISTER SPTREE *q;

{
    REGISTER SPBLK *n;

    if( NULL == ( n = __splookup( key, q ) ) )
    {
	n = (SPBLK *) emalloc( sizeof( *n ) );
	n->key = (univptr_t) key;
	n->leftlink = NULL;
	n->rightlink = NULL;
	n->uplink = NULL;
	(void) spenq( n, q );
    }

    n->data = data;
    n->datb = datb;

    return( n );
}




/*----------------
 *
 * spfhead() --	return the "lowest" element in the tree.
 *
 *	returns a reference to the head event in the event-set q.
 *	avoids splaying but just searches for and returns a pointer to
 *	the bottom of the left branch.
 */
static SPBLK *
spfhead( q )

REGISTER SPTREE * q;

{
    REGISTER SPBLK * x;

    if( NULL != ( x = q->root ) )
	while( x->leftlink != NULL )
	    x = x->leftlink;

    return( x );

} /* spfhead */



/*----------------
 *
 * spscan() -- apply a function to nodes in ascending order.
 *
 *	if n is given, start at that node, otherwise start from
 *	the head.
 */
void
__spscan( f, n, q )
REGISTER void (*f)();
REGISTER SPBLK * n;
REGISTER SPTREE * q;
{
    REGISTER SPBLK * x;

    for( x = n != NULL ? n : spfhead( q ); x != NULL ; x = spfnext( x ) )
        (*f)( x );
}


/*----------------
 *
 * spfnext() -- fast return next higer item in the tree, or NULL.
 *
 *	return the successor of n in q, represented as a splay tree.
 *	This is a fast (on average) version that does not splay.
 */
static SPBLK *
spfnext( n )

REGISTER SPBLK * n;

{
    REGISTER SPBLK * next;
    REGISTER SPBLK * x;

    /* a long version, avoids splaying for fast average,
     * poor amortized bound
     */

    if( n == NULL )
        return( n );

    x = n->rightlink;
    if( x != NULL )
    {
        while( x->leftlink != NULL )
	    x = x->leftlink;
        next = x;
    }
    else	/* x == NULL */
    {
        x = n->uplink;
        next = NULL;
        while( x != NULL )
	{
            if( x->leftlink == n )
	    {
                next = x;
                x = NULL;
            }
	    else
	    {
                n = x;
                x = n->uplink;
            }
        }
    }

    return( next );

} /* spfnext */


char *
__spstats( q )
SPTREE *q;
{
    static char buf[ 128 ];
    float llen;
    float elen;
    float sloops;

    if( q == NULL )
	return("");

    llen = q->lookups ? (float)q->lkpcmps / q->lookups : 0;
    elen = q->enqs ? (float)q->enqcmps/q->enqs : 0;
    sloops = q->splays ? (float)q->splayloops/q->splays : 0;

    (void) sprintf(buf, "f(%d %4.2f) i(%d %4.2f) s(%d %4.2f)",
	q->lookups, llen, q->enqs, elen, q->splays, sloops );

    return buf;
}

/*
  spaux.c:  This code implements the following operations on an event-set
  or priority-queue implemented using splay trees:
  
  spdelete( n, q )		n is removed from q.
  
  In the above, n and np are pointers to single items (type
  SPBLK *); q is an event-set (type SPTREE *),
  The type definitions for these are taken
  from file sptree.h.  All of these operations rest on basic
  splay tree operations from file sptree.c.
  
  The basic splay tree algorithms were originally presented in:
  
  Self Adjusting Binary Trees,
  by D. D. Sleator and R. E. Tarjan,
  Proc. ACM SIGACT Symposium on Theory
  of Computing (Boston, Apr 1983) 235-245.
  
  The operations in this package supplement the operations from
  file splay.h to provide support for operations typically needed
  on the pending event set in discrete event simulation.  See, for
  example,
  
  Introduction to Simula 67,
  by Gunther Lamprecht, Vieweg & Sohn, Braucschweig, Wiesbaden, 1981.
  (Chapter 14 contains the relevant discussion.)
  
  Simula Begin,
  by Graham M. Birtwistle, et al, Studentlitteratur, Lund, 1979.
  (Chapter 9 contains the relevant discussion.)
  
  Many of the routines in this package use the splay procedure,
  for bottom-up splaying of the queue.  Consequently, item n in
  delete and item np in all operations listed above must be in the
  event-set prior to the call or the results will be
  unpredictable (eg:  chaos will ensue).
  
  Note that, in all cases, these operations can be replaced with
  the corresponding operations formulated for a conventional
  lexicographically ordered tree.  The versions here all use the
  splay operation to ensure the amortized bounds; this usually
  leads to a very compact formulation of the operations
  themselves, but it may slow the average performance.
  
  Alternative versions based on simple binary tree operations are
  provided (commented out) for head, next, and prev, since these
  are frequently used to traverse the entire data structure, and
  the cost of traversal is independent of the shape of the
  structure, so the extra time taken by splay in this context is
  wasted.
  
  This code was written by:
  Douglas W. Jones with assistance from Srinivas R. Sataluri
  
  Translated to C by David Brower, daveb@rtech.uucp
  
  Thu Oct  6 12:11:33 PDT 1988 (daveb) Fixed spdeq, which was broken
 	handling one-node trees.  I botched the pascal translation of
 	a VAR parameter.  Changed interface, so callers must also be
	corrected to pass the node by address rather than value.
  Mon Apr  3 15:18:32 PDT 1989 (daveb)
  	Apply fix supplied by Mark Moraes <moraes@csri.toronto.edu> to
	spdelete(), which dropped core when taking out the last element
	in a subtree -- that is, when the right subtree was empty and
	the leftlink was also null, it tried to take out the leftlink's
	uplink anyway.
 */
/*----------------
 *
 * spdelete() -- Delete node from a tree.
 *
 *	n is deleted from q; the resulting splay tree has been splayed
 *	around its new root, which is the successor of n
 *
 */
void
__spdelete( n, q )

REGISTER SPBLK * n;
REGISTER SPTREE * q;

{
    REGISTER SPBLK * x;
    
    splay( n, q );
    x = spdeq( &q->root->rightlink );
    if( x == NULL )		/* empty right subtree */
    {
        q->root = q->root->leftlink;
        if (q->root) q->root->uplink = NULL;
    }
    else			/* non-empty right subtree */
    {
        x->uplink = NULL;
        x->leftlink = q->root->leftlink;
        x->rightlink = q->root->rightlink;
        if( x->leftlink != NULL )
	    x->leftlink->uplink = x;
        if( x->rightlink != NULL )
	    x->rightlink->uplink = x;
        q->root = x;
    }
    
} /* spdelete */


/*
 *
 *  sptree.c:  The following code implements the basic operations on
 *  an event-set or priority-queue implemented using splay trees:
 *
 *  SPTREE *spinit( compare )	Make a new tree
 *  SPBLK *spenq( n, q )	Insert n in q after all equal keys.
 *  SPBLK *spdeq( np )		Return first key under *np, removing it.
 *  void splay( n, q )		n (already in q) becomes the root.
 *
 *  In the above, n points to an SPBLK type, while q points to an
 *  SPTREE.
 *
 *  The implementation used here is based on the implementation
 *  which was used in the tests of splay trees reported in:
 *
 *    An Empirical Comparison of Priority-Queue and Event-Set Implementations,
 *	by Douglas W. Jones, Comm. ACM 29, 4 (Apr. 1986) 300-311.
 *
 *  The changes made include the addition of the enqprior
 *  operation and the addition of up-links to allow for the splay
 *  operation.  The basic splay tree algorithms were originally
 *  presented in:
 *
 *	Self Adjusting Binary Trees,
 *		by D. D. Sleator and R. E. Tarjan,
 *			Proc. ACM SIGACT Symposium on Theory
 *			of Computing (Boston, Apr 1983) 235-245.
 *
 *  The enq and enqprior routines use variations on the
 *  top-down splay operation, while the splay routine is bottom-up.
 *  All are coded for speed.
 *
 *  Written by:
 *    Douglas W. Jones
 *
 *  Translated to C by:
 *    David Brower, daveb@rtech.uucp
 *
 * Thu Oct  6 12:11:33 PDT 1988 (daveb) Fixed spdeq, which was broken
 *	handling one-node trees.  I botched the pascal translation of
 *	a VAR parameter.
 */
/*----------------
 *
 * spinit() -- initialize an empty splay tree
 *
 */
SPTREE *
__spinit()
{
    REGISTER SPTREE * q;

    q = (SPTREE *) emalloc( sizeof( *q ) );

    q->lookups = 0;
    q->lkpcmps = 0;
    q->enqs = 0;
    q->enqcmps = 0;
    q->splays = 0;
    q->splayloops = 0;
    q->root = NULL;
    return( q );
}

/*----------------
 *
 *  spenq() -- insert item in a tree.
 *
 *  put n in q after all other nodes with the same key; when this is
 *  done, n will be the root of the splay tree representing q, all nodes
 *  in q with keys less than or equal to that of n will be in the
 *  left subtree, all with greater keys will be in the right subtree;
 *  the tree is split into these subtrees from the top down, with rotations
 *  performed along the way to shorten the left branch of the right subtree
 *  and the right branch of the left subtree
 */
static SPBLK *
spenq( n, q )
REGISTER SPBLK * n;
REGISTER SPTREE * q;
{
    REGISTER SPBLK * left;	/* the rightmost node in the left tree */
    REGISTER SPBLK * right;	/* the leftmost node in the right tree */
    REGISTER SPBLK * next;	/* the root of the unsplit part */
    REGISTER SPBLK * temp;

    REGISTER univptr_t key;

    q->enqs++;
    n->uplink = NULL;
    next = q->root;
    q->root = n;
    if( next == NULL )	/* trivial enq */
    {
        n->leftlink = NULL;
        n->rightlink = NULL;
    }
    else		/* difficult enq */
    {
        key = n->key;
        left = n;
        right = n;

        /* n's left and right children will hold the right and left
	   splayed trees resulting from splitting on n->key;
	   note that the children will be reversed! */

	q->enqcmps++;
        if ( COMPARE( next->key, key ) > 0 )
	    goto two;

    one:	/* assert next->key <= key */

	do	/* walk to the right in the left tree */
	{
            temp = next->rightlink;
            if( temp == NULL )
	    {
                left->rightlink = next;
                next->uplink = left;
                right->leftlink = NULL;
                goto done;	/* job done, entire tree split */
            }

	    q->enqcmps++;
            if( COMPARE( temp->key, key ) > 0 )
	    {
                left->rightlink = next;
                next->uplink = left;
                left = next;
                next = temp;
                goto two;	/* change sides */
            }

            next->rightlink = temp->leftlink;
            if( temp->leftlink != NULL )
	    	temp->leftlink->uplink = next;
            left->rightlink = temp;
            temp->uplink = left;
            temp->leftlink = next;
            next->uplink = temp;
            left = temp;
            next = temp->rightlink;
            if( next == NULL )
	    {
                right->leftlink = NULL;
                goto done;	/* job done, entire tree split */
            }

	    q->enqcmps++;

	} while( COMPARE( next->key, key ) <= 0 );	/* change sides */

    two:	/* assert next->key > key */

	do	/* walk to the left in the right tree */
	{
            temp = next->leftlink;
            if( temp == NULL )
	    {
                right->leftlink = next;
                next->uplink = right;
                left->rightlink = NULL;
                goto done;	/* job done, entire tree split */
            }

	    q->enqcmps++;
            if( COMPARE( temp->key, key ) <= 0 )
	    {
                right->leftlink = next;
                next->uplink = right;
                right = next;
                next = temp;
                goto one;	/* change sides */
            }
            next->leftlink = temp->rightlink;
            if( temp->rightlink != NULL )
	    	temp->rightlink->uplink = next;
            right->leftlink = temp;
            temp->uplink = right;
            temp->rightlink = next;
            next->uplink = temp;
            right = temp;
            next = temp->leftlink;
            if( next == NULL )
	    {
                left->rightlink = NULL;
                goto done;	/* job done, entire tree split */
            }

	    q->enqcmps++;

	} while( COMPARE( next->key, key ) > 0 );	/* change sides */

        goto one;

    done:	/* split is done, branches of n need reversal */

        temp = n->leftlink;
        n->leftlink = n->rightlink;
        n->rightlink = temp;
    }

    return( n );

} /* spenq */


/*----------------
 *
 *  spdeq() -- return and remove head node from a subtree.
 *
 *  remove and return the head node from the node set; this deletes
 *  (and returns) the leftmost node from q, replacing it with its right
 *  subtree (if there is one); on the way to the leftmost node, rotations
 *  are performed to shorten the left branch of the tree
 */
static SPBLK *
spdeq( np )

SPBLK **np;		/* pointer to a node pointer */

{
    REGISTER SPBLK * deq;		/* one to return */
    REGISTER SPBLK * next;       	/* the next thing to deal with */
    REGISTER SPBLK * left;      	/* the left child of next */
    REGISTER SPBLK * farleft;		/* the left child of left */
    REGISTER SPBLK * farfarleft;	/* the left child of farleft */

    if( np == NULL || *np == NULL )
    {
        deq = NULL;
    }
    else
    {
        next = *np;
        left = next->leftlink;
        if( left == NULL )
	{
            deq = next;
            *np = next->rightlink;

            if( *np != NULL )
		(*np)->uplink = NULL;

        }
	else for(;;)	/* left is not null */
	{
            /* next is not it, left is not NULL, might be it */
            farleft = left->leftlink;
            if( farleft == NULL )
	    {
                deq = left;
                next->leftlink = left->rightlink;
                if( left->rightlink != NULL )
		    left->rightlink->uplink = next;
		break;
            }

            /* next, left are not it, farleft is not NULL, might be it */
            farfarleft = farleft->leftlink;
            if( farfarleft == NULL )
	    {
                deq = farleft;
                left->leftlink = farleft->rightlink;
                if( farleft->rightlink != NULL )
		    farleft->rightlink->uplink = left;
		break;
            }

            /* next, left, farleft are not it, rotate */
            next->leftlink = farleft;
            farleft->uplink = next;
            left->leftlink = farleft->rightlink;
            if( farleft->rightlink != NULL )
		farleft->rightlink->uplink = left;
            farleft->rightlink = left;
            left->uplink = farleft;
            next = farleft;
            left = farfarleft;
	}
    }

    return( deq );

} /* spdeq */


/*----------------
 *
 *  splay() -- reorganize the tree.
 *
 *  the tree is reorganized so that n is the root of the
 *  splay tree representing q; results are unpredictable if n is not
 *  in q to start with; q is split from n up to the old root, with all
 *  nodes to the left of n ending up in the left subtree, and all nodes
 *  to the right of n ending up in the right subtree; the left branch of
 *  the right subtree and the right branch of the left subtree are
 *  shortened in the process
 *
 *  this code assumes that n is not NULL and is in q; it can sometimes
 *  detect n not in q and complain
 */

static void
splay( n, q )

REGISTER SPBLK * n;
SPTREE * q;

{
    REGISTER SPBLK * up;	/* points to the node being dealt with */
    REGISTER SPBLK * prev;	/* a descendent of up, already dealt with */
    REGISTER SPBLK * upup;	/* the parent of up */
    REGISTER SPBLK * upupup;	/* the grandparent of up */
    REGISTER SPBLK * left;	/* the top of left subtree being built */
    REGISTER SPBLK * right;	/* the top of right subtree being built */

    left = n->leftlink;
    right = n->rightlink;
    prev = n;
    up = prev->uplink;

    q->splays++;

    while( up != NULL )
    {
	q->splayloops++;

        /* walk up the tree towards the root, splaying all to the left of
	   n into the left subtree, all to right into the right subtree */

        upup = up->uplink;
        if( up->leftlink == prev )	/* up is to the right of n */
	{
            if( upup != NULL && upup->leftlink == up )  /* rotate */
	    {
                upupup = upup->uplink;
                upup->leftlink = up->rightlink;
                if( upup->leftlink != NULL )
		    upup->leftlink->uplink = upup;
                up->rightlink = upup;
                upup->uplink = up;
                if( upupup == NULL )
		    q->root = up;
		else if( upupup->leftlink == upup )
		    upupup->leftlink = up;
		else
		    upupup->rightlink = up;
                up->uplink = upupup;
                upup = upupup;
            }
            up->leftlink = right;
            if( right != NULL )
		right->uplink = up;
            right = up;

        }
	else				/* up is to the left of n */
	{
            if( upup != NULL && upup->rightlink == up )	/* rotate */
	    {
                upupup = upup->uplink;
                upup->rightlink = up->leftlink;
                if( upup->rightlink != NULL )
		    upup->rightlink->uplink = upup;
                up->leftlink = upup;
                upup->uplink = up;
                if( upupup == NULL )
		    q->root = up;
		else if( upupup->rightlink == upup )
		    upupup->rightlink = up;
		else
		    upupup->leftlink = up;
                up->uplink = upupup;
                upup = upupup;
            }
            up->rightlink = left;
            if( left != NULL )
		left->uplink = up;
            left = up;
        }
        prev = up;
        up = upup;
    }

# ifdef SPLAYDEBUG
    if( q->root != prev )
    {
/*	fprintf(stderr, " *** bug in splay: n not in q *** " ); */
	abort();
    }
# endif

    n->leftlink = left;
    n->rightlink = right;
    if( left != NULL )
	left->uplink = n;
    if( right != NULL )
	right->uplink = n;
    q->root = n;
    n->uplink = NULL;

} /* splay */

