/*	$FreeBSD$	*/

/*
 * Copyright (c) 1988, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)radix.h	8.2 (Berkeley) 10/31/94
 */

#if !defined(_NET_RADIX_H_) && !defined(_RADIX_H_)
#define	_NET_RADIX_H_
#ifndef _RADIX_H_
#define	_RADIX_H_
#endif /* _RADIX_H_ */

#ifndef __P
# ifdef __STDC__
#  define	__P(x)  x
# else
#  define	__P(x)  ()
# endif
#endif

#if defined(__sgi) || defined(__osf__) || defined(sun)
# define	radix_mask	ipf_radix_mask
# define	radix_node	ipf_radix_node
# define	radix_node_head	ipf_radix_node_head
#endif

/*
 * Radix search tree node layout.
 */

struct radix_node {
	struct	radix_mask *rn_mklist;	/* list of masks contained in subtree */
	struct	radix_node *rn_p;	/* parent */
	short	rn_b;			/* bit offset; -1-index(netmask) */
	char	rn_bmask;		/* node: mask for bit test*/
	u_char	rn_flags;		/* enumerated next */
#define RNF_NORMAL	1		/* leaf contains normal route */
#define RNF_ROOT	2		/* leaf is root leaf for tree */
#define RNF_ACTIVE	4		/* This node is alive (for rtfree) */
	union {
		struct {			/* leaf only data: */
			caddr_t rn_Key;		/* object of search */
			caddr_t rn_Mask;	/* netmask, if present */
			struct	radix_node *rn_Dupedkey;
		} rn_leaf;
		struct {			/* node only data: */
			int	rn_Off;		/* where to start compare */
			struct	radix_node *rn_L;/* progeny */
			struct	radix_node *rn_R;/* progeny */
		} rn_node;
	} rn_u;
#ifdef RN_DEBUG
	int rn_info;
	struct radix_node *rn_twin;
	struct radix_node *rn_ybro;
#endif
};

#define rn_dupedkey rn_u.rn_leaf.rn_Dupedkey
#define rn_key rn_u.rn_leaf.rn_Key
#define rn_mask rn_u.rn_leaf.rn_Mask
#define rn_off rn_u.rn_node.rn_Off
#define rn_l rn_u.rn_node.rn_L
#define rn_r rn_u.rn_node.rn_R

/*
 * Annotations to tree concerning potential routes applying to subtrees.
 */

struct radix_mask {
	short	rm_b;			/* bit offset; -1-index(netmask) */
	char	rm_unused;		/* cf. rn_bmask */
	u_char	rm_flags;		/* cf. rn_flags */
	struct	radix_mask *rm_mklist;	/* more masks to try */
	union	{
		caddr_t	rmu_mask;		/* the mask */
		struct	radix_node *rmu_leaf;	/* for normal routes */
	}	rm_rmu;
	int	rm_refs;		/* # of references to this struct */
};

#define rm_mask rm_rmu.rmu_mask
#define rm_leaf rm_rmu.rmu_leaf		/* extra field would make 32 bytes */

#define MKGet(m) {\
	if (rn_mkfreelist) {\
		m = rn_mkfreelist; \
		rn_mkfreelist = (m)->rm_mklist; \
	} else \
		R_Malloc(m, struct radix_mask *, sizeof (*(m))); }\

#define MKFree(m) { (m)->rm_mklist = rn_mkfreelist; rn_mkfreelist = (m);}

struct radix_node_head {
	struct	radix_node *rnh_treetop;
	struct	radix_node *rnh_leaflist;
	u_long	rnh_hits;
	u_int	rnh_number;
	u_int	rnh_ref;
	int	rnh_addrsize;		/* permit, but not require fixed keys */
	int	rnh_pktsize;		/* permit, but not require fixed keys */
	struct	radix_node *(*rnh_addaddr)	/* add based on sockaddr */
		__P((void *v, void *mask,
		     struct radix_node_head *head, struct radix_node nodes[]));
	struct	radix_node *(*rnh_addpkt)	/* add based on packet hdr */
		__P((void *v, void *mask,
		     struct radix_node_head *head, struct radix_node nodes[]));
	struct	radix_node *(*rnh_deladdr)	/* remove based on sockaddr */
		__P((void *v, void *mask, struct radix_node_head *head));
	struct	radix_node *(*rnh_delpkt)	/* remove based on packet hdr */
		__P((void *v, void *mask, struct radix_node_head *head));
	struct	radix_node *(*rnh_matchaddr)	/* locate based on sockaddr */
		__P((void *v, struct radix_node_head *head));
	struct	radix_node *(*rnh_lookup)	/* locate based on sockaddr */
		__P((void *v, void *mask, struct radix_node_head *head));
	struct	radix_node *(*rnh_matchpkt)	/* locate based on packet hdr */
		__P((void *v, struct radix_node_head *head));
	int	(*rnh_walktree)			/* traverse tree */
		__P((struct radix_node_head *,
		     int (*)(struct radix_node *, void *), void *));
	struct	radix_node rnh_nodes[3];	/* empty tree for common case */
};


#if defined(AIX)
# undef Bcmp
# undef Bzero
# undef R_Malloc
# undef Free
#endif
#define Bcmp(a, b, n)	bcmp(((caddr_t)(a)), ((caddr_t)(b)), (unsigned)(n))
#if defined(linux) && defined(_KERNEL)
# define Bcopy(a, b, n)	memmove(((caddr_t)(b)), ((caddr_t)(a)), (unsigned)(n))
#else
# define Bcopy(a, b, n)	bcopy(((caddr_t)(a)), ((caddr_t)(b)), (unsigned)(n))
#endif
#define Bzero(p, n)		bzero((caddr_t)(p), (unsigned)(n));
#define R_Malloc(p, t, n)	KMALLOCS(p, t, n)
#define FreeS(p, z)		KFREES(p, z)
#define Free(p)			KFREE(p)

#if (defined(__osf__) || defined(AIX) || (IRIX >= 60516) || defined(sun)) && defined(_KERNEL)
# define	rn_init		ipf_rn_init
# define	rn_fini		ipf_rn_fini
# define	rn_inithead	ipf_rn_inithead
# define	rn_freehead	ipf_rn_freehead
# define	rn_inithead0	ipf_rn_inithead0
# define	rn_refines	ipf_rn_refines
# define	rn_walktree	ipf_rn_walktree
# define	rn_addmask	ipf_rn_addmask
# define	rn_addroute	ipf_rn_addroute
# define	rn_delete	ipf_rn_delete
# define	rn_insert	ipf_rn_insert
# define	rn_lookup	ipf_rn_lookup
# define	rn_match	ipf_rn_match
# define	rn_newpair	ipf_rn_newpair
# define	rn_search	ipf_rn_search
# define	rn_search_m	ipf_rn_search_m
# define	max_keylen	ipf_maxkeylen
# define	rn_mkfreelist	ipf_rn_mkfreelist
# define	rn_zeros	ipf_rn_zeros
# define	rn_ones		ipf_rn_ones
# define	rn_satisfies_leaf	ipf_rn_satisfies_leaf
# define	rn_lexobetter	ipf_rn_lexobetter
# define	rn_new_radix_mask	ipf_rn_new_radix_mask
# define	rn_freenode	ipf_rn_freenode
#endif

void	 rn_init __P((void));
void	 rn_fini __P((void));
int	 rn_inithead __P((void **, int));
void	 rn_freehead __P((struct radix_node_head *));
int	 rn_inithead0 __P((struct radix_node_head *, int));
int	 rn_refines __P((void *, void *));
int	 rn_walktree __P((struct radix_node_head *,
			  int (*)(struct radix_node *, void *), void *));
struct radix_node
	 *rn_addmask __P((void *, int, int)),
	 *rn_addroute __P((void *, void *, struct radix_node_head *,
			struct radix_node [2])),
	 *rn_delete __P((void *, void *, struct radix_node_head *)),
	 *rn_insert __P((void *, struct radix_node_head *, int *,
			struct radix_node [2])),
	 *rn_lookup __P((void *, void *, struct radix_node_head *)),
	 *rn_match __P((void *, struct radix_node_head *)),
	 *rn_newpair __P((void *, int, struct radix_node[2])),
	 *rn_search __P((void *, struct radix_node *)),
	 *rn_search_m __P((void *, struct radix_node *, void *));

#endif /* _NET_RADIX_H_ */
