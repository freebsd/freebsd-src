#ifndef LINT
static const char rcsid[] = "$Id: tree.c,v 1.3.18.1 2005-04-27 05:01:08 sra Exp $";
#endif

/*%
 * tree - balanced binary tree library
 *
 * vix 05apr94 [removed vixie.h dependencies; cleaned up formatting, names]
 * vix 22jan93 [revisited; uses RCS, ANSI, POSIX; has bug fixes]
 * vix 23jun86 [added delete uar to add for replaced nodes]
 * vix 20jun86 [added tree_delete per wirth a+ds (mod2 v.) p. 224]
 * vix 06feb86 [added tree_mung()]
 * vix 02feb86 [added tree balancing from wirth "a+ds=p" p. 220-221]
 * vix 14dec85 [written]
 */

/*%
 * This program text was created by Paul Vixie using examples from the book:
 * "Algorithms & Data Structures," Niklaus Wirth, Prentice-Hall, 1986, ISBN
 * 0-13-022005-1.  Any errors in the conversion from Modula-2 to C are Paul
 * Vixie's.
 */

/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Portions Copyright (c) 1996-1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*#define		DEBUG	"tree"*/

#include "port_before.h"

#include <stdio.h>
#include <stdlib.h>

#include "port_after.h"

#include <isc/memcluster.h>
#include <isc/tree.h>

#ifdef DEBUG
static int	debugDepth = 0;
static char	*debugFuncs[256];
# define ENTER(proc) { \
			debugFuncs[debugDepth] = proc; \
			fprintf(stderr, "ENTER(%d:%s.%s)\n", \
				debugDepth, DEBUG, \
				debugFuncs[debugDepth]); \
			debugDepth++; \
		}
# define RET(value) { \
			debugDepth--; \
			fprintf(stderr, "RET(%d:%s.%s)\n", \
				debugDepth, DEBUG, \
				debugFuncs[debugDepth]); \
			return (value); \
		}
# define RETV { \
			debugDepth--; \
			fprintf(stderr, "RETV(%d:%s.%s)\n", \
				debugDepth, DEBUG, \
				debugFuncs[debugDepth]); \
			return; \
		}
# define MSG(msg)	fprintf(stderr, "MSG(%s)\n", msg);
#else
# define ENTER(proc)	;
# define RET(value)	return (value);
# define RETV		return;
# define MSG(msg)	;
#endif

#ifndef TRUE
# define TRUE		1
# define FALSE		0
#endif

static tree *	sprout(tree **, tree_t, int *, int (*)(), void (*)());
static int	delete(tree **, int (*)(), tree_t, void (*)(), int *, int *);
static void	del(tree **, int *, tree **, void (*)(), int *);
static void	bal_L(tree **, int *);
static void	bal_R(tree **, int *);

void
tree_init(tree **ppr_tree) {
	ENTER("tree_init")
	*ppr_tree = NULL;
	RETV
}
	
tree_t
tree_srch(tree **ppr_tree, int (*pfi_compare)(tree_t, tree_t), tree_t	p_user) {
	ENTER("tree_srch")

	if (*ppr_tree) {
		int i_comp = (*pfi_compare)(p_user, (**ppr_tree).data);

		if (i_comp > 0)
			RET(tree_srch(&(**ppr_tree).right,
				      pfi_compare,
				      p_user))

		if (i_comp < 0)
			RET(tree_srch(&(**ppr_tree).left,
				      pfi_compare,
				      p_user))

		/* not higher, not lower... this must be the one.
		 */
		RET((**ppr_tree).data)
	}

	/* grounded. NOT found.
	 */
	RET(NULL)
}

tree_t
tree_add(tree **ppr_tree, int (*pfi_compare)(tree_t, tree_t),
	 tree_t p_user, void (*pfv_uar)())
{
	int i_balance = FALSE;

	ENTER("tree_add")
	if (!sprout(ppr_tree, p_user, &i_balance, pfi_compare, pfv_uar))
		RET(NULL)
	RET(p_user)
}

int
tree_delete(tree **ppr_p, int (*pfi_compare)(tree_t, tree_t),
	    tree_t p_user, void	(*pfv_uar)())
{
	int i_balance = FALSE, i_uar_called = FALSE;

	ENTER("tree_delete");
	RET(delete(ppr_p, pfi_compare, p_user, pfv_uar,
		   &i_balance, &i_uar_called))
}

int
tree_trav(tree **ppr_tree, int (*pfi_uar)(tree_t)) {
	ENTER("tree_trav")

	if (!*ppr_tree)
		RET(TRUE)

	if (!tree_trav(&(**ppr_tree).left, pfi_uar))
		RET(FALSE)
	if (!(*pfi_uar)((**ppr_tree).data))
		RET(FALSE)
	if (!tree_trav(&(**ppr_tree).right, pfi_uar))
		RET(FALSE)
	RET(TRUE)
}

void
tree_mung(tree **ppr_tree, void	(*pfv_uar)(tree_t)) {
	ENTER("tree_mung")
	if (*ppr_tree) {
		tree_mung(&(**ppr_tree).left, pfv_uar);
		tree_mung(&(**ppr_tree).right, pfv_uar);
		if (pfv_uar)
			(*pfv_uar)((**ppr_tree).data);
		memput(*ppr_tree, sizeof(tree));
		*ppr_tree = NULL;
	}
	RETV
}

static tree *
sprout(tree **ppr, tree_t p_data, int *pi_balance,
       int (*pfi_compare)(tree_t, tree_t), void (*pfv_delete)(tree_t))
{
	tree *p1, *p2, *sub;
	int cmp;

	ENTER("sprout")

	/* are we grounded?  if so, add the node "here" and set the rebalance
	 * flag, then exit.
	 */
	if (!*ppr) {
		MSG("grounded. adding new node, setting h=true")
		*ppr = (tree *) memget(sizeof(tree));
		if (*ppr) {
			(*ppr)->left = NULL;
			(*ppr)->right = NULL;
			(*ppr)->bal = 0;
			(*ppr)->data = p_data;
			*pi_balance = TRUE;
		}
		RET(*ppr);
	}

	/* compare the data using routine passed by caller.
	 */
	cmp = (*pfi_compare)(p_data, (*ppr)->data);

	/* if LESS, prepare to move to the left.
	 */
	if (cmp < 0) {
		MSG("LESS. sprouting left.")
		sub = sprout(&(*ppr)->left, p_data, pi_balance,
			     pfi_compare, pfv_delete);
		if (sub && *pi_balance) {	/*%< left branch has grown */
			MSG("LESS: left branch has grown")
			switch ((*ppr)->bal) {
			case 1:
				/* right branch WAS longer; bal is ok now */
				MSG("LESS: case 1.. bal restored implicitly")
				(*ppr)->bal = 0;
				*pi_balance = FALSE;
				break;
			case 0:
				/* balance WAS okay; now left branch longer */
				MSG("LESS: case 0.. balnce bad but still ok")
				(*ppr)->bal = -1;
				break;
			case -1:
				/* left branch was already too long. rebal */
				MSG("LESS: case -1: rebalancing")
				p1 = (*ppr)->left;
				if (p1->bal == -1) {		/*%< LL */
					MSG("LESS: single LL")
					(*ppr)->left = p1->right;
					p1->right = *ppr;
					(*ppr)->bal = 0;
					*ppr = p1;
				} else {			/*%< double LR */
					MSG("LESS: double LR")

					p2 = p1->right;
					p1->right = p2->left;
					p2->left = p1;

					(*ppr)->left = p2->right;
					p2->right = *ppr;

					if (p2->bal == -1)
						(*ppr)->bal = 1;
					else
						(*ppr)->bal = 0;

					if (p2->bal == 1)
						p1->bal = -1;
					else
						p1->bal = 0;
					*ppr = p2;
				} /*else*/
				(*ppr)->bal = 0;
				*pi_balance = FALSE;
			} /*switch*/
		} /*if*/
		RET(sub)
	} /*if*/

	/* if MORE, prepare to move to the right.
	 */
	if (cmp > 0) {
		MSG("MORE: sprouting to the right")
		sub = sprout(&(*ppr)->right, p_data, pi_balance,
			     pfi_compare, pfv_delete);
		if (sub && *pi_balance) {
			MSG("MORE: right branch has grown")

			switch ((*ppr)->bal) {
			case -1:
				MSG("MORE: balance was off, fixed implicitly")
				(*ppr)->bal = 0;
				*pi_balance = FALSE;
				break;
			case 0:
				MSG("MORE: balance was okay, now off but ok")
				(*ppr)->bal = 1;
				break;
			case 1:
				MSG("MORE: balance was off, need to rebalance")
				p1 = (*ppr)->right;
				if (p1->bal == 1) {		/*%< RR */
					MSG("MORE: single RR")
					(*ppr)->right = p1->left;
					p1->left = *ppr;
					(*ppr)->bal = 0;
					*ppr = p1;
				} else {			/*%< double RL */
					MSG("MORE: double RL")

					p2 = p1->left;
					p1->left = p2->right;
					p2->right = p1;

					(*ppr)->right = p2->left;
					p2->left = *ppr;

					if (p2->bal == 1)
						(*ppr)->bal = -1;
					else
						(*ppr)->bal = 0;

					if (p2->bal == -1)
						p1->bal = 1;
					else
						p1->bal = 0;

					*ppr = p2;
				} /*else*/
				(*ppr)->bal = 0;
				*pi_balance = FALSE;
			} /*switch*/
		} /*if*/
		RET(sub)
	} /*if*/

	/* not less, not more: this is the same key!  replace...
	 */
	MSG("FOUND: Replacing data value")
	*pi_balance = FALSE;
	if (pfv_delete)
		(*pfv_delete)((*ppr)->data);
	(*ppr)->data = p_data;
	RET(*ppr)
}

static int
delete(tree **ppr_p, int (*pfi_compare)(tree_t, tree_t), tree_t p_user,
       void (*pfv_uar)(tree_t), int *pi_balance, int *pi_uar_called)
{
	tree *pr_q;
	int i_comp, i_ret;

	ENTER("delete")

	if (*ppr_p == NULL) {
		MSG("key not in tree")
		RET(FALSE)
	}

	i_comp = (*pfi_compare)((*ppr_p)->data, p_user);
	if (i_comp > 0) {
		MSG("too high - scan left")
		i_ret = delete(&(*ppr_p)->left, pfi_compare, p_user, pfv_uar,
			       pi_balance, pi_uar_called);
		if (*pi_balance)
			bal_L(ppr_p, pi_balance);
	} else if (i_comp < 0) {
		MSG("too low - scan right")
		i_ret = delete(&(*ppr_p)->right, pfi_compare, p_user, pfv_uar,
			       pi_balance, pi_uar_called);
		if (*pi_balance)
			bal_R(ppr_p, pi_balance);
	} else {
		MSG("equal")
		pr_q = *ppr_p;
		if (pr_q->right == NULL) {
			MSG("right subtree null")
			*ppr_p = pr_q->left;
			*pi_balance = TRUE;
		} else if (pr_q->left == NULL) {
			MSG("right subtree non-null, left subtree null")
			*ppr_p = pr_q->right;
			*pi_balance = TRUE;
		} else {
			MSG("neither subtree null")
			del(&pr_q->left, pi_balance, &pr_q,
			    pfv_uar, pi_uar_called);
			if (*pi_balance)
				bal_L(ppr_p, pi_balance);
		}
		if (!*pi_uar_called && pfv_uar)
			(*pfv_uar)(pr_q->data);
		/* Thanks to wuth@castrov.cuc.ab.ca for the following stmt. */
		memput(pr_q, sizeof(tree));
		i_ret = TRUE;
	}
	RET(i_ret)
}

static void
del(tree **ppr_r, int *pi_balance, tree **ppr_q,
    void (*pfv_uar)(tree_t), int *pi_uar_called)
{
	ENTER("del")

	if ((*ppr_r)->right != NULL) {
		del(&(*ppr_r)->right, pi_balance, ppr_q,
		    pfv_uar, pi_uar_called);
		if (*pi_balance)
			bal_R(ppr_r, pi_balance);
	} else {
		if (pfv_uar)
			(*pfv_uar)((*ppr_q)->data);
		*pi_uar_called = TRUE;
		(*ppr_q)->data = (*ppr_r)->data;
		*ppr_q = *ppr_r;
		*ppr_r = (*ppr_r)->left;
		*pi_balance = TRUE;
	}

	RETV
}

static void
bal_L(tree **ppr_p, int *pi_balance) {
	tree *p1, *p2;
	int b1, b2;

	ENTER("bal_L")
	MSG("left branch has shrunk")

	switch ((*ppr_p)->bal) {
	case -1:
		MSG("was imbalanced, fixed implicitly")
		(*ppr_p)->bal = 0;
		break;
	case 0:
		MSG("was okay, is now one off")
		(*ppr_p)->bal = 1;
		*pi_balance = FALSE;
		break;
	case 1:
		MSG("was already off, this is too much")
		p1 = (*ppr_p)->right;
		b1 = p1->bal;
		if (b1 >= 0) {
			MSG("single RR")
			(*ppr_p)->right = p1->left;
			p1->left = *ppr_p;
			if (b1 == 0) {
				MSG("b1 == 0")
				(*ppr_p)->bal = 1;
				p1->bal = -1;
				*pi_balance = FALSE;
			} else {
				MSG("b1 != 0")
				(*ppr_p)->bal = 0;
				p1->bal = 0;
			}
			*ppr_p = p1;
		} else {
			MSG("double RL")
			p2 = p1->left;
			b2 = p2->bal;
			p1->left = p2->right;
			p2->right = p1;
			(*ppr_p)->right = p2->left;
			p2->left = *ppr_p;
			if (b2 == 1)
				(*ppr_p)->bal = -1;
			else
				(*ppr_p)->bal = 0;
			if (b2 == -1)
				p1->bal = 1;
			else
				p1->bal = 0;
			*ppr_p = p2;
			p2->bal = 0;
		}
	}
	RETV
}

static void
bal_R(tree **ppr_p, int *pi_balance) {
	tree *p1, *p2;
	int b1, b2;

	ENTER("bal_R")
	MSG("right branch has shrunk")
	switch ((*ppr_p)->bal) {
	case 1:
		MSG("was imbalanced, fixed implicitly")
		(*ppr_p)->bal = 0;
		break;
	case 0:
		MSG("was okay, is now one off")
		(*ppr_p)->bal = -1;
		*pi_balance = FALSE;
		break;
	case -1:
		MSG("was already off, this is too much")
		p1 = (*ppr_p)->left;
		b1 = p1->bal;
		if (b1 <= 0) {
			MSG("single LL")
			(*ppr_p)->left = p1->right;
			p1->right = *ppr_p;
			if (b1 == 0) {
				MSG("b1 == 0")
				(*ppr_p)->bal = -1;
				p1->bal = 1;
				*pi_balance = FALSE;
			} else {
				MSG("b1 != 0")
				(*ppr_p)->bal = 0;
				p1->bal = 0;
			}
			*ppr_p = p1;
		} else {
			MSG("double LR")
			p2 = p1->right;
			b2 = p2->bal;
			p1->right = p2->left;
			p2->left = p1;
			(*ppr_p)->left = p2->right;
			p2->right = *ppr_p;
			if (b2 == -1)
				(*ppr_p)->bal = 1;
			else
				(*ppr_p)->bal = 0;
			if (b2 == 1)
				p1->bal = -1;
			else
				p1->bal = 0;
			*ppr_p = p2;
			p2->bal = 0;
		}
	}
	RETV
}

/*! \file */
