/*
 * Copyright (c) 1988, 1989  Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 *	from: @(#)radix.c	7.9 (Berkeley) 2/4/91
 *	$Id: radix.c,v 1.4 1993/11/25 01:34:07 wollman Exp $
 */

/*
 * Routines to build and maintain radix trees for routing lookups.
 */
#ifndef RNF_NORMAL
#include "param.h"
#include "systm.h"
#include "radix.h"
#include "malloc.h"
#define	M_DONTWAIT M_NOWAIT
#endif
struct radix_node_head *mask_rnhead;
#define rn_maskhead mask_rnhead->rnh_treetop
struct radix_mask *rn_mkfreelist;
struct radix_node_head *radix_node_head;
#undef Bcmp
#define Bcmp(a, b, l) (l == 0 ? 0 : bcmp((caddr_t)(a), (caddr_t)(b), (u_long)l))
/*
 * The data structure for the keys is a radix tree with one way
 * branching removed.  The index rn_b at an internal node n represents a bit
 * position to be tested.  The tree is arranged so that all descendants
 * of a node n have keys whose bits all agree up to position rn_b - 1.
 * (We say the index of n is rn_b.)
 *
 * There is at least one descendant which has a one bit at position rn_b,
 * and at least one with a zero there.
 *
 * A route is determined by a pair of key and mask.  We require that the
 * bit-wise logical and of the key and mask to be the key.
 * We define the index of a route to associated with the mask to be
 * the first bit number in the mask where 0 occurs (with bit number 0
 * representing the highest order bit).
 * 
 * We say a mask is normal if every bit is 0, past the index of the mask.
 * If a node n has a descendant (k, m) with index(m) == index(n) == rn_b,
 * and m is a normal mask, then the route applies to every descendant of n.
 * If the index(m) < rn_b, this implies the trailing last few bits of k
 * before bit b are all 0, (and hence consequently true of every descendant
 * of n), so the route applies to all descendants of the node as well.
 *
 * The present version of the code makes no use of normal routes,
 * but similar logic shows that a non-normal mask m such that
 * index(m) <= index(n) could potentially apply to many children of n.
 * Thus, for each non-host route, we attach its mask to a list at an internal
 * node as high in the tree as we can go. 
 */

struct radix_node *
rn_search(v, head)
	struct radix_node *head;
	register caddr_t v;
{
	register struct radix_node *x;

	for (x = head; x->rn_b >= 0;) {
		if (x->rn_bmask & v[x->rn_off])
			x = x->rn_r;
		else
			x = x->rn_l;
	}
	return x;
};

struct radix_node *
rn_search_m(v, head, m)
	struct radix_node *head;
	register caddr_t v, m;
{
	register struct radix_node *x;

	for (x = head; x->rn_b >= 0;) {
		if ((x->rn_bmask & m[x->rn_off]) &&
		    (x->rn_bmask & v[x->rn_off]))
			x = x->rn_r;
		else
			x = x->rn_l;
	}
	return x;
};


static int gotOddMasks;
static char maskedKey[MAXKEYLEN];

struct radix_node *
rn_match(v, head)
	struct radix_node *head;
	caddr_t v;
{
	register struct radix_node *t = head, *x;
	register caddr_t cp = v, cp2, cp3;
	caddr_t cplim, mstart;
	struct radix_node *saved_t;
	int off = t->rn_off, vlen = *(u_char *)cp, matched_off;

	/*
	 * Open code rn_search(v, head) to avoid overhead of extra
	 * subroutine call.
	 */
	for (; t->rn_b >= 0; ) {
		if (t->rn_bmask & cp[t->rn_off])
			t = t->rn_r;
		else
			t = t->rn_l;
	}
	/*
	 * See if we match exactly as a host destination
	 */
	cp += off; cp2 = t->rn_key + off; cplim = v + vlen;
	for (; cp < cplim; cp++, cp2++)
		if (*cp != *cp2)
			goto on1;
	/*
	 * This extra grot is in case we are explicitly asked
	 * to look up the default.  Ugh!
	 */
	if ((t->rn_flags & RNF_ROOT) && t->rn_dupedkey)
		t = t->rn_dupedkey;
	return t;
on1:
	matched_off = cp - v;
	saved_t = t;
	do {
	    if (t->rn_mask) {
		/*
		 * Even if we don't match exactly as a hosts;
		 * we may match if the leaf we wound up at is
		 * a route to a net.
		 */
		cp3 = matched_off + t->rn_mask;
		cp2 = matched_off + t->rn_key;
		for (; cp < cplim; cp++)
			if ((*cp2++ ^ *cp) & *cp3++)
				break;
		if (cp == cplim)
			return t;
		cp = matched_off + v;
	    }
	} while (t = t->rn_dupedkey);
	t = saved_t;
	/* start searching up the tree */
	do {
		register struct radix_mask *m;
		t = t->rn_p;
		if (m = t->rn_mklist) {
			/*
			 * After doing measurements here, it may
			 * turn out to be faster to open code
			 * rn_search_m here instead of always
			 * copying and masking.
			 */
			off = min(t->rn_off, matched_off);
			mstart = maskedKey + off;
			do {
				cp2 = mstart;
				cp3 = m->rm_mask + off;
				for (cp = v + off; cp < cplim;)
					*cp2++ =  *cp++ & *cp3++;
				x = rn_search(maskedKey, t);
				while (x && x->rn_mask != m->rm_mask)
					x = x->rn_dupedkey;
				if (x &&
				    (Bcmp(mstart, x->rn_key + off,
					vlen - off) == 0))
					    return x;
			} while (m = m->rm_mklist);
		}
	} while (t != head);
	return 0;
};
		
#ifdef RN_DEBUG
int	rn_nodenum;
struct	radix_node *rn_clist;
int	rn_saveinfo;
#endif

struct radix_node *
rn_newpair(v, b, nodes)
	caddr_t v;
	int b;
	struct radix_node nodes[2];
{
	register struct radix_node *tt = nodes, *t = tt + 1;
	t->rn_b = b; t->rn_bmask = 0x80 >> (b & 7);
	t->rn_l = tt; t->rn_off = b >> 3;
	tt->rn_b = -1; tt->rn_key = v; tt->rn_p = t;
	tt->rn_flags = t->rn_flags = RNF_ACTIVE;
#ifdef RN_DEBUG
	tt->rn_info = rn_nodenum++; t->rn_info = rn_nodenum++;
	tt->rn_twin = t; tt->rn_ybro = rn_clist; rn_clist = tt;
#endif
	return t;
}

int rn_debug =  1;
struct radix_node *
rn_insert(v, head, dupentry, nodes)
	caddr_t v;
	struct radix_node *head;
	int *dupentry;
	struct radix_node nodes[2];
{
	int head_off = head->rn_off, vlen = (int)*((u_char *)v);
	register struct radix_node *t = rn_search(v, head);
	register caddr_t cp = v + head_off;
	register int b;
	struct radix_node *tt;
    	/*
	 *find first bit at which v and t->rn_key differ
	 */
    {
	register caddr_t cp2 = t->rn_key + head_off;
	register int cmp_res;
	caddr_t cplim = v + vlen;

	while (cp < cplim)
		if (*cp2++ != *cp++)
			goto on1;
	*dupentry = 1;
	return t;
on1:
	*dupentry = 0;
	cmp_res = (cp[-1] ^ cp2[-1]) & 0xff;
	for (b = (cp - v) << 3; cmp_res; b--)
		cmp_res >>= 1;
    }
    {
	register struct radix_node *p, *x = head;
	cp = v;
	do {
		p = x;
		if (cp[x->rn_off] & x->rn_bmask) 
			x = x->rn_r;
		else x = x->rn_l;
	} while (b > (unsigned) x->rn_b); /* x->rn_b < b && x->rn_b >= 0 */
#ifdef RN_DEBUG
	if (rn_debug)
		printf("Going In:\n"), traverse(p);
#endif
	t = rn_newpair(v, b, nodes); tt = t->rn_l;
	if ((cp[p->rn_off] & p->rn_bmask) == 0)
		p->rn_l = t;
	else
		p->rn_r = t;
	x->rn_p = t; t->rn_p = p; /* frees x, p as temp vars below */
	if ((cp[t->rn_off] & t->rn_bmask) == 0) {
		t->rn_r = x;
	} else {
		t->rn_r = tt; t->rn_l = x;
	}
#ifdef RN_DEBUG
	if (rn_debug)
		printf("Coming out:\n"), traverse(p);
#endif
    }
	return (tt);
}

struct radix_node *
rn_addmask(netmask, search, skip)
	caddr_t netmask;
	int search;
	int skip;
{
	register struct radix_node *x;
	register caddr_t cp, cplim;
	register int b, mlen, j;
	int maskduplicated;

	mlen = *(u_char *)netmask;
	if (search) {
		x = rn_search(netmask, rn_maskhead);
		mlen = *(u_char *)netmask;
		if (Bcmp(netmask, x->rn_key, mlen) == 0)
			return (x);
	}
	R_Malloc(x, struct radix_node *, MAXKEYLEN + 2 * sizeof (*x));
	if (x == 0)
		return (0);
	Bzero(x, MAXKEYLEN + 2 * sizeof (*x));
	cp = (caddr_t)(x + 2);
	Bcopy(netmask, cp, mlen);
	netmask = cp;
	x = rn_insert(netmask, rn_maskhead, &maskduplicated, x);
	/*
	 * Calculate index of mask.
	 */
	cplim = netmask + mlen;
	for (cp = netmask + skip; cp < cplim; cp++)
		if (*(u_char *)cp != 0xff)
			break;
	b = (cp - netmask) << 3;
	if (cp != cplim) {
		if (*cp != 0) {
			gotOddMasks = 1;
			for (j = 0x80; j; b++, j >>= 1)  
				if ((j & *cp) == 0)
					break;
		}
	}
	x->rn_b = -1 - b;
	return (x);
}

struct radix_node *
rn_addroute(v, netmask, head, treenodes)
struct radix_node *head;
	caddr_t netmask, v;
	struct radix_node treenodes[2];
{
	register int j;
	register caddr_t cp;
	register struct radix_node *t, *x = 0, *tt;
	short b = 0, b_leaf;
	int vlen = *(u_char *)v, mlen = 0, keyduplicated;
	caddr_t cplim; unsigned char *maskp;
	struct radix_mask *m, **mp;
	struct radix_node *saved_tt;

	/*
	 * In dealing with non-contiguous masks, there may be
	 * many different routes which have the same mask.
	 * We will find it useful to have a unique pointer to
	 * the mask to speed avoiding duplicate references at
	 * nodes and possibly save time in calculating indices.
	 */
	if (netmask)  {
		x = rn_search(netmask, rn_maskhead);
		mlen = *(u_char *)netmask;
		if (Bcmp(netmask, x->rn_key, mlen) != 0) {
			x = rn_addmask(netmask, 0, head->rn_off);
			if (x == 0)
				return (0);
		}
		netmask = x->rn_key;
		b = -1 - x->rn_b;
	}
	/*
	 * Deal with duplicated keys: attach node to previous instance
	 */
	saved_tt = tt = rn_insert(v, head, &keyduplicated, treenodes);
	if (keyduplicated) {
		do {
			if (tt->rn_mask == netmask)
				return (0);
			t = tt;
		} while (tt = tt->rn_dupedkey);
		/*
		 * If the mask is not duplicated, we wouldn't
		 * find it among possible duplicate key entries
		 * anyway, so the above test doesn't hurt.
		 *
		 * XXX: we really ought to sort the masks
		 * for a duplicated key the same way as in a masklist.
		 * It is an unfortunate pain having to relocate
		 * the head of the list.
		 */
		t->rn_dupedkey = tt = treenodes;
#ifdef RN_DEBUG
		t=tt+1; tt->rn_info = rn_nodenum++; t->rn_info = rn_nodenum++;
		tt->rn_twin = t; tt->rn_ybro = rn_clist; rn_clist = tt;
#endif
		t = saved_tt;
		tt->rn_key = (caddr_t) v;
		tt->rn_b = -1;
		tt->rn_flags = t->rn_flags & ~RNF_ROOT;
	}
	/*
	 * Put mask in tree.
	 */
	if (netmask) {
		tt->rn_mask = netmask;
		tt->rn_b = x->rn_b;
	}
	t = saved_tt->rn_p;
	b_leaf = -1 - t->rn_b;
	if (t->rn_r == saved_tt) x = t->rn_l; else x = t->rn_r;
	/* Promote general routes from below */
	if (x->rn_b < 0) { 
		if (x->rn_mask && (x->rn_b >= b_leaf) && x->rn_mklist == 0) {
			MKGet(m);
			if (m) {
				Bzero(m, sizeof *m);
				m->rm_b = x->rn_b;
				m->rm_mask = x->rn_mask;
				x->rn_mklist = t->rn_mklist = m;
			}
		}
	} else if (x->rn_mklist) {
		/*
		 * Skip over masks whose index is > that of new node
		 */
		for (mp = &x->rn_mklist; m = *mp; mp = &m->rm_mklist)
			if (m->rm_b >= b_leaf)
				break;
		t->rn_mklist = m; *mp = 0;
	}
	/* Add new route to highest possible ancestor's list */
	if ((netmask == 0) || (b > t->rn_b ))
		return tt; /* can't lift at all */
	b_leaf = tt->rn_b;
	do {
		x = t;
		t = t->rn_p;
	} while (b <= t->rn_b && x != head);
	/*
	 * Search through routes associated with node to
	 * insert new route according to index.
	 * For nodes of equal index, place more specific
	 * masks first.
	 */
	cplim = netmask + mlen;
	for (mp = &x->rn_mklist; m = *mp; mp = &m->rm_mklist) {
		if (m->rm_b < b_leaf)
			continue;
		if (m->rm_b > b_leaf)
			break;
		if (m->rm_mask == netmask) {
			m->rm_refs++;
			tt->rn_mklist = m;
			return tt;
		}
		maskp = (u_char *)m->rm_mask;
		for (cp = netmask; cp < cplim; cp++)
			if (*(u_char *)cp > *maskp++)
				goto on2;
	}
on2:
	MKGet(m);
	if (m == 0) {
		printf("Mask for route not entered\n");
		return (tt);
	}
	Bzero(m, sizeof *m);
	m->rm_b = b_leaf;
	m->rm_mask = netmask;
	m->rm_mklist = *mp;
	*mp = m;
	tt->rn_mklist = m;
	return tt;
}

struct radix_node *
rn_delete(v, netmask, head)
	caddr_t v, netmask;
	struct radix_node *head;
{
	register struct radix_node *t, *p, *x = head;
	register struct radix_node *tt = rn_search(v, x);
	int b, head_off = x->rn_off, vlen =  * (u_char *) v;
	struct radix_mask *m, *saved_m, **mp;
	struct radix_node *dupedkey, *saved_tt = tt;

	if (tt == 0 ||
	    Bcmp(v + head_off, tt->rn_key + head_off, vlen - head_off))
		return (0);
	/*
	 * Delete our route from mask lists.
	 */
	if (dupedkey = tt->rn_dupedkey) {
		if (netmask) 
			netmask = rn_search(netmask, rn_maskhead)->rn_key;
		while (tt->rn_mask != netmask)
			if ((tt = tt->rn_dupedkey) == 0)
				return (0);
	}
	if (tt->rn_mask == 0 || (saved_m = m = tt->rn_mklist) == 0)
		goto on1;
	if (m->rm_mask != tt->rn_mask) {
		printf("rn_delete: inconsistent annotation\n");
		goto on1;
	}
	if (--m->rm_refs >= 0)
		goto on1;
	b = -1 - tt->rn_b;
	t = saved_tt->rn_p;
	if (b > t->rn_b)
		goto on1; /* Wasn't lifted at all */
	do {
		x = t;
		t = t->rn_p;
	} while (b <= t->rn_b && x != head);
	for (mp = &x->rn_mklist; m = *mp; mp = &m->rm_mklist)
		if (m == saved_m) {
			*mp = m->rm_mklist;
			MKFree(m);
			break;
		}
	if (m == 0)
		printf("rn_delete: couldn't find our annotation\n");
on1:
	/*
	 * Eliminate us from tree
	 */
	if (tt->rn_flags & RNF_ROOT)
		return (0);
#ifdef RN_DEBUG
	/* Get us out of the creation list */
	for (t = rn_clist; t && t->rn_ybro != tt; t = t->rn_ybro) {}
	if (t) t->rn_ybro = tt->rn_ybro;
#endif RN_DEBUG
	t = tt->rn_p;
	if (dupedkey) {
		if (tt == saved_tt) {
			x = dupedkey; x->rn_p = t;
			if (t->rn_l == tt) t->rn_l = x; else t->rn_r = x;
#ifndef RN_DEBUG
			x++; t = tt + 1; *x = *t; p = t->rn_p;
#else
			x++; b = x->rn_info; t = tt + 1; *x = *t; p = t->rn_p;
			x->rn_info = b;
#endif
			if (p->rn_l == t) p->rn_l = x; else p->rn_r = x;
			x->rn_l->rn_p = x; x->rn_r->rn_p = x;
		} else {
			for (p = saved_tt; p && p->rn_dupedkey != tt;)
				p = p->rn_dupedkey;
			if (p) p->rn_dupedkey = tt->rn_dupedkey;
			else printf("rn_delete: couldn't find us\n");
		}
		goto out;
	}
	if (t->rn_l == tt) x = t->rn_r; else x = t->rn_l;
	p = t->rn_p;
	if (p->rn_r == t) p->rn_r = x; else p->rn_l = x;
	x->rn_p = p;
	/*
	 * Demote routes attached to us.
	 */
	if (t->rn_mklist) {
		if (x->rn_b >= 0) {
			for (mp = &x->rn_mklist; m = *mp;)
				mp = &m->rm_mklist;
			*mp = t->rn_mklist;
		} else {
			for (m = t->rn_mklist; m;) {
				struct radix_mask *mm = m->rm_mklist;
				if (m == x->rn_mklist && (--(m->rm_refs) < 0)) {
					x->rn_mklist = 0;
					MKFree(m);
				} else 
					printf("%s %x at %x\n",
					    "rn_delete: Orphaned Mask", m, x);
				m = mm;
			}
		}
	}
	/*
	 * We may be holding an active internal node in the tree.
	 */
	x = tt + 1;
	if (t != x) {
#ifndef RN_DEBUG
		*t = *x;
#else
		b = t->rn_info; *t = *x; t->rn_info = b;
#endif
		t->rn_l->rn_p = t; t->rn_r->rn_p = t;
		p = x->rn_p;
		if (p->rn_l == x) p->rn_l = t; else p->rn_r = t;
	}
out:
	tt->rn_flags &= ~RNF_ACTIVE;
	tt[1].rn_flags &= ~RNF_ACTIVE;
	return (tt);
}
char rn_zeros[MAXKEYLEN], rn_ones[MAXKEYLEN];

int
rn_inithead(head, off, af)
	struct radix_node_head **head;
	int off;
	int af;
{
	register struct radix_node_head *rnh;
	register struct radix_node *t, *tt, *ttt;
	if (*head)
		return (1);
	R_Malloc(rnh, struct radix_node_head *, sizeof (*rnh));
	if (rnh == 0)
		return (0);
	Bzero(rnh, sizeof (*rnh));
	*head = rnh;
	t = rn_newpair(rn_zeros, off, rnh->rnh_nodes);
	ttt = rnh->rnh_nodes + 2;
	t->rn_r = ttt;
	t->rn_p = t;
	tt = t->rn_l;
	tt->rn_flags = t->rn_flags = RNF_ROOT | RNF_ACTIVE;
	tt->rn_b = -1 - off;
	*ttt = *tt;
	ttt->rn_key = rn_ones;
	rnh->rnh_af = af;
	rnh->rnh_treetop = t;
	if (radix_node_head == 0) {
		caddr_t cp = rn_ones, cplim = rn_ones + MAXKEYLEN;
		while (cp < cplim)
			*cp++ = -1;
		if (rn_inithead(&radix_node_head, 0, 0) == 0) {
			Free(rnh);
			*head = 0;
			return (0);
		}
		mask_rnhead = radix_node_head;
	}
	rnh->rnh_next = radix_node_head->rnh_next;
	if (radix_node_head != rnh)
		radix_node_head->rnh_next = rnh;
	return (1);
}
