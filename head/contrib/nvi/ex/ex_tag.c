/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1992, 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * David Hitz of Auspex Systems, Inc.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)ex_tag.c	10.36 (Berkeley) 9/15/96";
#endif /* not lint */

#include <sys/param.h>
#include <sys/types.h>		/* XXX: param.h may not have included types.h */

#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif

#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../common/common.h"
#include "../vi/vi.h"
#include "tag.h"

static char	*binary_search __P((char *, char *, char *));
static int	 compare __P((char *, char *, char *));
static void	 ctag_file __P((SCR *, TAGF *, char *, char **, size_t *));
static int	 ctag_search __P((SCR *, char *, size_t, char *));
#ifdef GTAGS
static int	 getentry __P((char *, char **, char **, char **));
static TAGQ	*gtag_slist __P((SCR *, char *, int));
#endif
static int	 ctag_sfile __P((SCR *, TAGF *, TAGQ *, char *));
static TAGQ	*ctag_slist __P((SCR *, char *));
static char	*linear_search __P((char *, char *, char *));
static int	 tag_copy __P((SCR *, TAG *, TAG **));
static int	 tag_pop __P((SCR *, TAGQ *, int));
static int	 tagf_copy __P((SCR *, TAGF *, TAGF **));
static int	 tagf_free __P((SCR *, TAGF *));
static int	 tagq_copy __P((SCR *, TAGQ *, TAGQ **));

/*
 * ex_tag_first --
 *	The tag code can be entered from main, e.g., "vi -t tag".
 *
 * PUBLIC: int ex_tag_first __P((SCR *, char *));
 */
int
ex_tag_first(sp, tagarg)
	SCR *sp;
	char *tagarg;
{
	ARGS *ap[2], a;
	EXCMD cmd;

	/* Build an argument for the ex :tag command. */
	ex_cinit(&cmd, C_TAG, 0, OOBLNO, OOBLNO, 0, ap);
	ex_cadd(&cmd, &a, tagarg, strlen(tagarg));

	/*
	 * XXX
	 * Historic vi went ahead and created a temporary file when it failed
	 * to find the tag.  We match historic practice, but don't distinguish
	 * between real error and failure to find the tag.
	 */
	if (ex_tag_push(sp, &cmd))
		return (0);

	/* Display tags in the center of the screen. */
	F_CLR(sp, SC_SCR_TOP);
	F_SET(sp, SC_SCR_CENTER);

	return (0);
}

#ifdef GTAGS
/*
 * ex_rtag_push -- ^]
 *		  :rtag[!] [string]
 *
 * Enter a new TAGQ context based on a ctag string.
 *
 * PUBLIC: int ex_rtag_push __P((SCR *, EXCMD *));
 */
int
ex_rtag_push(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
{
	F_SET(cmdp, E_REFERENCE);
	return ex_tag_push(sp, cmdp);
}
#endif

/*
 * ex_tag_push -- ^]
 *		  :tag[!] [string]
 *
 * Enter a new TAGQ context based on a ctag string.
 *
 * PUBLIC: int ex_tag_push __P((SCR *, EXCMD *));
 */
int
ex_tag_push(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
{
	EX_PRIVATE *exp;
	FREF *frp;
	TAG *rtp;
	TAGQ *rtqp, *tqp;
	recno_t lno;
	size_t cno;
	long tl;
	int force, istmp;

	exp = EXP(sp);
	switch (cmdp->argc) {
	case 1:
		if (exp->tag_last != NULL)
			free(exp->tag_last);

		if ((exp->tag_last = strdup(cmdp->argv[0]->bp)) == NULL) {
			msgq(sp, M_SYSERR, NULL);
			return (1);
		}

		/* Taglength may limit the number of characters. */
		if ((tl =
		    O_VAL(sp, O_TAGLENGTH)) != 0 && strlen(exp->tag_last) > tl)
			exp->tag_last[tl] = '\0';
		break;
	case 0:
		if (exp->tag_last == NULL) {
			msgq(sp, M_ERR, "158|No previous tag entered");
			return (1);
		}
		break;
	default:
		abort();
	}

	/* Get the tag information. */
#ifdef GTAGS
	if (O_ISSET(sp, O_GTAGSMODE)) {
		if ((tqp = gtag_slist(sp, exp->tag_last, F_ISSET(cmdp, E_REFERENCE))) == NULL)
			return (1);
	} else
#endif
	if ((tqp = ctag_slist(sp, exp->tag_last)) == NULL)
		return (1);

	/*
	 * Allocate all necessary memory before swapping screens.  Initialize
	 * flags so we know what to free.
	 */
	rtp = NULL;
	rtqp = NULL;
	if (exp->tq.cqh_first == (void *)&exp->tq) {
		/* Initialize the `local context' tag queue structure. */
		CALLOC_GOTO(sp, rtqp, TAGQ *, 1, sizeof(TAGQ));
		CIRCLEQ_INIT(&rtqp->tagq);

		/* Initialize and link in its tag structure. */
		CALLOC_GOTO(sp, rtp, TAG *, 1, sizeof(TAG));
		CIRCLEQ_INSERT_HEAD(&rtqp->tagq, rtp, q);
		rtqp->current = rtp;
	}

	/*
	 * Stick the current context information in a convenient place, we're
	 * about to lose it.  Note, if we're called on editor startup, there
	 * will be no FREF structure.
	 */
	frp = sp->frp;
	lno = sp->lno;
	cno = sp->cno;
	istmp = frp == NULL ||
	    F_ISSET(frp, FR_TMPFILE) && !F_ISSET(cmdp, E_NEWSCREEN);

	/* Try to switch to the tag. */
	force = FL_ISSET(cmdp->iflags, E_C_FORCE);
	if (F_ISSET(cmdp, E_NEWSCREEN)) {
		if (ex_tag_Nswitch(sp, tqp->tagq.cqh_first, force))
			goto err;

		/* Everything else gets done in the new screen. */
		sp = sp->nextdisp;
		exp = EXP(sp);
	} else
		if (ex_tag_nswitch(sp, tqp->tagq.cqh_first, force))
			goto err;

	/*
	 * If this is the first tag, put a `current location' queue entry
	 * in place, so we can pop all the way back to the current mark.
	 * Note, it doesn't point to much of anything, it's a placeholder.
	 */
	if (exp->tq.cqh_first == (void *)&exp->tq) {
		CIRCLEQ_INSERT_HEAD(&exp->tq, rtqp, q);
	} else
		rtqp = exp->tq.cqh_first;

	/* Link the new TAGQ structure into place. */
	CIRCLEQ_INSERT_HEAD(&exp->tq, tqp, q);

	(void)ctag_search(sp,
	    tqp->current->search, tqp->current->slen, tqp->tag);

	/*
	 * Move the current context from the temporary save area into the
	 * right structure.
	 *
	 * If we were in a temporary file, we don't have a context to which
	 * we can return, so just make it be the same as what we're moving
	 * to.  It will be a little odd that ^T doesn't change anything, but
	 * I don't think it's a big deal.
	 */
	if (istmp) {
		rtqp->current->frp = sp->frp;
		rtqp->current->lno = sp->lno;
		rtqp->current->cno = sp->cno;
	} else {
		rtqp->current->frp = frp;
		rtqp->current->lno = lno;
		rtqp->current->cno = cno;
	}
	return (0);

err:
alloc_err:
	if (rtqp != NULL)
		free(rtqp);
	if (rtp != NULL)
		free(rtp);
	tagq_free(sp, tqp);
	return (1);
}

/* 
 * ex_tag_next --
 *	Switch context to the next TAG.
 *
 * PUBLIC: int ex_tag_next __P((SCR *, EXCMD *));
 */
int
ex_tag_next(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
{
	EX_PRIVATE *exp;
	TAG *tp;
	TAGQ *tqp;

	exp = EXP(sp);
	if ((tqp = exp->tq.cqh_first) == (void *)&exp->tq) {
		tag_msg(sp, TAG_EMPTY, NULL);
		return (1);
	}
	if ((tp = tqp->current->q.cqe_next) == (void *)&tqp->tagq) {
		msgq(sp, M_ERR, "282|Already at the last tag of this group");
		return (1);
	}
	if (ex_tag_nswitch(sp, tp, FL_ISSET(cmdp->iflags, E_C_FORCE)))
		return (1);
	tqp->current = tp;

	if (F_ISSET(tqp, TAG_CSCOPE))
		(void)cscope_search(sp, tqp, tp);
	else
		(void)ctag_search(sp, tp->search, tp->slen, tqp->tag);
	return (0);
}

/* 
 * ex_tag_prev --
 *	Switch context to the next TAG.
 *
 * PUBLIC: int ex_tag_prev __P((SCR *, EXCMD *));
 */
int
ex_tag_prev(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
{
	EX_PRIVATE *exp;
	TAG *tp;
	TAGQ *tqp;

	exp = EXP(sp);
	if ((tqp = exp->tq.cqh_first) == (void *)&exp->tq) {
		tag_msg(sp, TAG_EMPTY, NULL);
		return (0);
	}
	if ((tp = tqp->current->q.cqe_prev) == (void *)&tqp->tagq) {
		msgq(sp, M_ERR, "255|Already at the first tag of this group");
		return (1);
	}
	if (ex_tag_nswitch(sp, tp, FL_ISSET(cmdp->iflags, E_C_FORCE)))
		return (1);
	tqp->current = tp;

	if (F_ISSET(tqp, TAG_CSCOPE))
		(void)cscope_search(sp, tqp, tp);
	else
		(void)ctag_search(sp, tp->search, tp->slen, tqp->tag);
	return (0);
}

/*
 * ex_tag_nswitch --
 *	Switch context to the specified TAG.
 *
 * PUBLIC: int ex_tag_nswitch __P((SCR *, TAG *, int));
 */
int
ex_tag_nswitch(sp, tp, force)
	SCR *sp;
	TAG *tp;
	int force;
{
	/* Get a file structure. */
	if (tp->frp == NULL && (tp->frp = file_add(sp, tp->fname)) == NULL)
		return (1);

	/* If not changing files, return, we're done. */
	if (tp->frp == sp->frp)
		return (0);

	/* Check for permission to leave. */
	if (file_m1(sp, force, FS_ALL | FS_POSSIBLE))
		return (1);

	/* Initialize the new file. */
	if (file_init(sp, tp->frp, NULL, FS_SETALT))
		return (1);

	/* Display tags in the center of the screen. */
	F_CLR(sp, SC_SCR_TOP);
	F_SET(sp, SC_SCR_CENTER);

	/* Switch. */
	F_SET(sp, SC_FSWITCH);
	return (0);
}

/*
 * ex_tag_Nswitch --
 *	Switch context to the specified TAG in a new screen.
 *
 * PUBLIC: int ex_tag_Nswitch __P((SCR *, TAG *, int));
 */
int
ex_tag_Nswitch(sp, tp, force)
	SCR *sp;
	TAG *tp;
	int force;
{
	SCR *new;

	/* Get a file structure. */
	if (tp->frp == NULL && (tp->frp = file_add(sp, tp->fname)) == NULL)
		return (1);

	/* Get a new screen. */
	if (screen_init(sp->gp, sp, &new))
		return (1);
	if (vs_split(sp, new, 0)) {
		(void)file_end(new, new->ep, 1);
		(void)screen_end(new);
		return (1);
	}

	/* Get a backing file. */
	if (tp->frp == sp->frp) {
		/* Copy file state. */
		new->ep = sp->ep;
		++new->ep->refcnt;

		new->frp = tp->frp;
		new->frp->flags = sp->frp->flags;
	} else if (file_init(new, tp->frp, NULL, force)) {
		(void)vs_discard(new, NULL);
		(void)screen_end(new);
		return (1);
	}

	/* Create the argument list. */
	new->cargv = new->argv = ex_buildargv(sp, NULL, tp->frp->name);

	/* Display tags in the center of the screen. */
	F_CLR(new, SC_SCR_TOP);
	F_SET(new, SC_SCR_CENTER);

	/* Switch. */
	sp->nextdisp = new;
	F_SET(sp, SC_SSWITCH);

	return (0);
}

/*
 * ex_tag_pop -- ^T
 *		 :tagp[op][!] [number | file]
 *
 *	Pop to a previous TAGQ context.
 *
 * PUBLIC: int ex_tag_pop __P((SCR *, EXCMD *));
 */
int
ex_tag_pop(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
{
	EX_PRIVATE *exp;
	TAGQ *tqp, *dtqp;
	size_t arglen;
	long off;
	char *arg, *p, *t;

	/* Check for an empty stack. */
	exp = EXP(sp);
	if (exp->tq.cqh_first == (void *)&exp->tq) {
		tag_msg(sp, TAG_EMPTY, NULL);
		return (1);
	}

	/* Find the last TAG structure that we're going to DISCARD! */
	switch (cmdp->argc) {
	case 0:				/* Pop one tag. */
		dtqp = exp->tq.cqh_first;
		break;
	case 1:				/* Name or number. */
		arg = cmdp->argv[0]->bp;
		off = strtol(arg, &p, 10);
		if (*p != '\0')
			goto filearg;

		/* Number: pop that many queue entries. */
		if (off < 1)
			return (0);
		for (tqp = exp->tq.cqh_first;
		    tqp != (void *)&exp->tq && --off > 1;
		    tqp = tqp->q.cqe_next);
		if (tqp == (void *)&exp->tq) {
			msgq(sp, M_ERR,
	"159|Less than %s entries on the tags stack; use :display t[ags]",
			    arg);
			return (1);
		}
		dtqp = tqp;
		break;

		/* File argument: pop to that queue entry. */
filearg:	arglen = strlen(arg);
		for (tqp = exp->tq.cqh_first;
		    tqp != (void *)&exp->tq;
		    dtqp = tqp, tqp = tqp->q.cqe_next) {
			/* Don't pop to the current file. */
			if (tqp == exp->tq.cqh_first)
				continue;
			p = tqp->current->frp->name;
			if ((t = strrchr(p, '/')) == NULL)
				t = p;
			else
				++t;
			if (!strncmp(arg, t, arglen))
				break;
		}
		if (tqp == (void *)&exp->tq) {
			msgq_str(sp, M_ERR, arg,
	"160|No file %s on the tags stack to return to; use :display t[ags]");
			return (1);
		}
		if (tqp == exp->tq.cqh_first)
			return (0);
		break;
	default:
		abort();
	}

	return (tag_pop(sp, dtqp, FL_ISSET(cmdp->iflags, E_C_FORCE)));
}

/*
 * ex_tag_top -- :tagt[op][!]
 *	Clear the tag stack.
 *
 * PUBLIC: int ex_tag_top __P((SCR *, EXCMD *));
 */
int
ex_tag_top(sp, cmdp)
	SCR *sp;
	EXCMD *cmdp;
{
	EX_PRIVATE *exp;

	exp = EXP(sp);

	/* Check for an empty stack. */
	if (exp->tq.cqh_first == (void *)&exp->tq) {
		tag_msg(sp, TAG_EMPTY, NULL);
		return (1);
	}

	/* Return to the oldest information. */
	return (tag_pop(sp,
	    exp->tq.cqh_last->q.cqe_prev, FL_ISSET(cmdp->iflags, E_C_FORCE)));
}

/*
 * tag_pop --
 *	Pop up to and including the specified TAGQ context.
 */
static int
tag_pop(sp, dtqp, force)
	SCR *sp;
	TAGQ *dtqp;
	int force;
{
	EX_PRIVATE *exp;
	TAG *tp;
	TAGQ *tqp;

	exp = EXP(sp);

	/*
	 * Update the cursor from the saved TAG information of the TAG
	 * structure we're moving to.
	 */
	tp = dtqp->q.cqe_next->current;
	if (tp->frp == sp->frp) {
		sp->lno = tp->lno;
		sp->cno = tp->cno;
	} else {
		if (file_m1(sp, force, FS_ALL | FS_POSSIBLE))
			return (1);

		tp->frp->lno = tp->lno;
		tp->frp->cno = tp->cno;
		F_SET(sp->frp, FR_CURSORSET);
		if (file_init(sp, tp->frp, NULL, FS_SETALT))
			return (1);

		F_SET(sp, SC_FSWITCH);
	}

	/* Pop entries off the queue up to and including dtqp. */
	do {
		tqp = exp->tq.cqh_first;
		if (tagq_free(sp, tqp))
			return (0);
	} while (tqp != dtqp);

	/*
	 * If only a single tag left, we've returned to the first tag point,
	 * and the stack is now empty.
	 */
	if (exp->tq.cqh_first->q.cqe_next == (void *)&exp->tq)
		tagq_free(sp, exp->tq.cqh_first);

	return (0);
}

/*
 * ex_tag_display --
 *	Display the list of tags.
 *
 * PUBLIC: int ex_tag_display __P((SCR *));
 */
int
ex_tag_display(sp)
	SCR *sp;
{
	EX_PRIVATE *exp;
	TAG *tp;
	TAGQ *tqp;
	int cnt;
	size_t len;
	char *p, *sep;

	exp = EXP(sp);
	if ((tqp = exp->tq.cqh_first) == (void *)&exp->tq) {
		tag_msg(sp, TAG_EMPTY, NULL);
		return (0);
	}

	/*
	 * We give the file name 20 columns and the search string the rest.
	 * If there's not enough room, we don't do anything special, it's
	 * not worth the effort, it just makes the display more confusing.
	 *
	 * We also assume that characters in file names map 1-1 to printing
	 * characters.  This might not be true, but I don't think it's worth
	 * fixing.  (The obvious fix is to pass the filenames through the
	 * msg_print function.)
	 */
#define	L_NAME	30		/* Name. */
#define	L_SLOP	 4		/* Leading number plus trailing *. */
#define	L_SPACE	 5		/* Spaces after name, before tag. */
#define	L_TAG	20		/* Tag. */
	if (sp->cols <= L_NAME + L_SLOP) {
		msgq(sp, M_ERR, "292|Display too small.");
		return (0);
	}

	/*
	 * Display the list of tags for each queue entry.  The first entry
	 * is numbered, and the current tag entry has an asterisk appended.
	 */
	for (cnt = 1, tqp = exp->tq.cqh_first; !INTERRUPTED(sp) &&
	    tqp != (void *)&exp->tq; ++cnt, tqp = tqp->q.cqe_next)
		for (tp = tqp->tagq.cqh_first;
		    tp != (void *)&tqp->tagq; tp = tp->q.cqe_next) {
			if (tp == tqp->tagq.cqh_first)
				(void)ex_printf(sp, "%2d ", cnt);
			else
				(void)ex_printf(sp, "   ");
			p = tp->frp == NULL ? tp->fname : tp->frp->name;
			if ((len = strlen(p)) > L_NAME) {
				len = len - (L_NAME - 4);
				(void)ex_printf(sp, "   ... %*.*s",
				    L_NAME - 4, L_NAME - 4, p + len);
			} else
				(void)ex_printf(sp,
				    "   %*.*s", L_NAME, L_NAME, p);
			if (tqp->current == tp)
				(void)ex_printf(sp, "*");

			if (tp == tqp->tagq.cqh_first && tqp->tag != NULL &&
			    (sp->cols - L_NAME) >= L_TAG + L_SPACE) {
				len = strlen(tqp->tag);
				if (len > sp->cols - (L_NAME + L_SPACE))
					len = sp->cols - (L_NAME + L_SPACE);
				(void)ex_printf(sp, "%s%.*s",
				    tqp->current == tp ? "    " : "     ",
				    (int)len, tqp->tag);
			}
			(void)ex_printf(sp, "\n");
		}
	return (0);
}

/*
 * ex_tag_copy --
 *	Copy a screen's tag structures.
 *
 * PUBLIC: int ex_tag_copy __P((SCR *, SCR *));
 */
int
ex_tag_copy(orig, sp)
	SCR *orig, *sp;
{
	EX_PRIVATE *oexp, *nexp;
	TAGQ *aqp, *tqp;
	TAG *ap, *tp;
	TAGF *atfp, *tfp;

	oexp = EXP(orig);
	nexp = EXP(sp);

	/* Copy tag queue and tags stack. */
	for (aqp = oexp->tq.cqh_first;
	    aqp != (void *)&oexp->tq; aqp = aqp->q.cqe_next) {
		if (tagq_copy(sp, aqp, &tqp))
			return (1);
		for (ap = aqp->tagq.cqh_first;
		    ap != (void *)&aqp->tagq; ap = ap->q.cqe_next) {
			if (tag_copy(sp, ap, &tp))
				return (1);
			/* Set the current pointer. */
			if (aqp->current == ap)
				tqp->current = tp;
			CIRCLEQ_INSERT_TAIL(&tqp->tagq, tp, q);
		}
		CIRCLEQ_INSERT_TAIL(&nexp->tq, tqp, q);
	}

	/* Copy list of tag files. */
	for (atfp = oexp->tagfq.tqh_first;
	    atfp != NULL; atfp = atfp->q.tqe_next) {
		if (tagf_copy(sp, atfp, &tfp))
			return (1);
		TAILQ_INSERT_TAIL(&nexp->tagfq, tfp, q);
	}

	/* Copy the last tag. */
	if (oexp->tag_last != NULL &&
	    (nexp->tag_last = strdup(oexp->tag_last)) == NULL) {
		msgq(sp, M_SYSERR, NULL);
		return (1);
	}
	return (0);
}

/*
 * tagf_copy --
 *	Copy a TAGF structure and return it in new memory.
 */
static int
tagf_copy(sp, otfp, tfpp)
	SCR *sp;
	TAGF *otfp, **tfpp;
{
	TAGF *tfp;

	MALLOC_RET(sp, tfp, TAGF *, sizeof(TAGF));
	*tfp = *otfp;

	/* XXX: Allocate as part of the TAGF structure!!! */
	if ((tfp->name = strdup(otfp->name)) == NULL)
		return (1);

	*tfpp = tfp;
	return (0);
}

/*
 * tagq_copy --
 *	Copy a TAGQ structure and return it in new memory.
 */
static int
tagq_copy(sp, otqp, tqpp)
	SCR *sp;
	TAGQ *otqp, **tqpp;
{
	TAGQ *tqp;
	size_t len;

	len = sizeof(TAGQ);
	if (otqp->tag != NULL)
		len += otqp->tlen + 1;
	MALLOC_RET(sp, tqp, TAGQ *, len);
	memcpy(tqp, otqp, len);

	CIRCLEQ_INIT(&tqp->tagq);
	tqp->current = NULL;
	if (otqp->tag != NULL)
		tqp->tag = tqp->buf;

	*tqpp = tqp;
	return (0);
}

/*
 * tag_copy --
 *	Copy a TAG structure and return it in new memory.
 */
static int
tag_copy(sp, otp, tpp)
	SCR *sp;
	TAG *otp, **tpp;
{
	TAG *tp;
	size_t len;

	len = sizeof(TAG);
	if (otp->fname != NULL)
		len += otp->fnlen + 1;
	if (otp->search != NULL)
		len += otp->slen + 1;
	MALLOC_RET(sp, tp, TAG *, len);
	memcpy(tp, otp, len);

	if (otp->fname != NULL)
		tp->fname = tp->buf;
	if (otp->search != NULL)
		tp->search = tp->fname + otp->fnlen + 1;

	*tpp = tp;
	return (0);
}

/*
 * tagf_free --
 *	Free a TAGF structure.
 */
static int
tagf_free(sp, tfp)
	SCR *sp;
	TAGF *tfp;
{
	EX_PRIVATE *exp;

	exp = EXP(sp);
	TAILQ_REMOVE(&exp->tagfq, tfp, q);
	free(tfp->name);
	free(tfp);
	return (0);
}

/*
 * tagq_free --
 *	Free a TAGQ structure (and associated TAG structures).
 *
 * PUBLIC: int tagq_free __P((SCR *, TAGQ *));
 */
int
tagq_free(sp, tqp)
	SCR *sp;
	TAGQ *tqp;
{
	EX_PRIVATE *exp;
	TAG *tp;

	exp = EXP(sp);
	while ((tp = tqp->tagq.cqh_first) != (void *)&tqp->tagq) {
		CIRCLEQ_REMOVE(&tqp->tagq, tp, q);
		free(tp);
	}
	/*
	 * !!!
	 * If allocated and then the user failed to switch files, the TAGQ
	 * structure was never attached to any list.
	 */
	if (tqp->q.cqe_next != NULL)
		CIRCLEQ_REMOVE(&exp->tq, tqp, q);
	free(tqp);
	return (0);
}

/*
 * tag_msg
 *	A few common messages.
 *
 * PUBLIC: void tag_msg __P((SCR *, tagmsg_t, char *));
 */
void
tag_msg(sp, msg, tag)
	SCR *sp;
	tagmsg_t msg;
	char *tag;
{
	switch (msg) {
	case TAG_BADLNO:
		msgq_str(sp, M_ERR, tag,
	    "164|%s: the tag's line number is past the end of the file");
		break;
	case TAG_EMPTY:
		msgq(sp, M_INFO, "165|The tags stack is empty");
		break;
	case TAG_SEARCH:
		msgq_str(sp, M_ERR, tag, "166|%s: search pattern not found");
		break;
	default:
		abort();
	}
}

/*
 * ex_tagf_alloc --
 *	Create a new list of ctag files.
 *
 * PUBLIC: int ex_tagf_alloc __P((SCR *, char *));
 */
int
ex_tagf_alloc(sp, str)
	SCR *sp;
	char *str;
{
	EX_PRIVATE *exp;
	TAGF *tfp;
	size_t len;
	char *p, *t;

	/* Free current queue. */
	exp = EXP(sp);
	while ((tfp = exp->tagfq.tqh_first) != NULL)
		tagf_free(sp, tfp);

	/* Create new queue. */
	for (p = t = str;; ++p) {
		if (*p == '\0' || isblank(*p)) {
			if ((len = p - t) > 1) {
				MALLOC_RET(sp, tfp, TAGF *, sizeof(TAGF));
				MALLOC(sp, tfp->name, char *, len + 1);
				if (tfp->name == NULL) {
					free(tfp);
					return (1);
				}
				memcpy(tfp->name, t, len);
				tfp->name[len] = '\0';
				tfp->flags = 0;
				TAILQ_INSERT_TAIL(&exp->tagfq, tfp, q);
			}
			t = p + 1;
		}
		if (*p == '\0')
			 break;
	}
	return (0);
}
						/* Free previous queue. */
/*
 * ex_tag_free --
 *	Free the ex tag information.
 *
 * PUBLIC: int ex_tag_free __P((SCR *));
 */
int
ex_tag_free(sp)
	SCR *sp;
{
	EX_PRIVATE *exp;
	TAGF *tfp;
	TAGQ *tqp;

	/* Free up tag information. */
	exp = EXP(sp);
	while ((tqp = exp->tq.cqh_first) != (void *)&exp->tq)
		tagq_free(sp, tqp);
	while ((tfp = exp->tagfq.tqh_first) != NULL)
		tagf_free(sp, tfp);
	if (exp->tag_last != NULL)
		free(exp->tag_last);
	return (0);
}

/*
 * ctag_search --
 *	Search a file for a tag.
 */
static int
ctag_search(sp, search, slen, tag)
	SCR *sp;
	char *search, *tag;
	size_t slen;
{
	MARK m;
	char *p;

	/*
	 * !!!
	 * The historic tags file format (from a long, long time ago...)
	 * used a line number, not a search string.  I got complaints, so
	 * people are still using the format.  POSIX 1003.2 permits it.
	 */
	if (isdigit(search[0])) {
		m.lno = atoi(search);
		if (!db_exist(sp, m.lno)) {
			tag_msg(sp, TAG_BADLNO, tag);
			return (1);
		}
	} else {
		/*
		 * Search for the tag; cheap fallback for C functions
		 * if the name is the same but the arguments have changed.
		 */
		m.lno = 1;
		m.cno = 0;
		if (f_search(sp, &m, &m,
		    search, slen, NULL, SEARCH_FILE | SEARCH_TAG))
			if ((p = strrchr(search, '(')) != NULL) {
				slen = p - search;
				if (f_search(sp, &m, &m, search, slen,
				    NULL, SEARCH_FILE | SEARCH_TAG))
					goto notfound;
			} else {
notfound:			tag_msg(sp, TAG_SEARCH, tag);
				return (1);
			}
		/*
		 * !!!
		 * Historically, tags set the search direction if it wasn't
		 * already set.
		 */
		if (sp->searchdir == NOTSET)
			sp->searchdir = FORWARD;
	}

	/*
	 * !!!
	 * Tags move to the first non-blank, NOT the search pattern start.
	 */
	sp->lno = m.lno;
	sp->cno = 0;
	(void)nonblank(sp, sp->lno, &sp->cno);
	return (0);
}

#ifdef GTAGS
/*
 * getentry --
 *	get tag information from current line.
 *
 * gtags temporary file format.
 * <tag>   <lineno>  <file>         <image>
 *
 * sample.
 * +------------------------------------------------
 * |main     30      main.c         main(argc, argv)
 * |func     21      subr.c         func(arg)
 */
static int
getentry(buf, tag, file, line)
	char *buf, **tag, **file, **line;
{
	char *p = buf;

	for (*tag = p; *p && !isspace(*p); p++)		/* tag name */
		;
	if (*p == 0)
		goto err;
	*p++ = 0;
	for (; *p && isspace(*p); p++)			/* (skip blanks) */
		;
	if (*p == 0)
		goto err;
	*line = p;					/* line no */
	for (*line = p; *p && !isspace(*p); p++)
		;
	if (*p == 0)
		goto err;
	*p++ = 0;
	for (; *p && isspace(*p); p++)			/* (skip blanks) */
		;
	if (*p == 0)
		goto err;
	*file = p;					/* file name */
	for (*file = p; *p && !isspace(*p); p++)
		;
	if (*p == 0)
		goto err;
	*p = 0;

	/* value check */
	if (strlen(*tag) && strlen(*line) && strlen(*file) && atoi(*line) > 0)
		return 1;	/* OK */
err:
	return 0;		/* ERROR */
}

/*
 * gtag_slist --
 *	Search the list of tags files for a tag, and return tag queue.
 */
static TAGQ *
gtag_slist(sp, tag, ref)
	SCR *sp;
	char *tag;
	int ref;
{
	EX_PRIVATE *exp;
	TAGF *tfp;
	TAGQ *tqp;
	size_t len;
	int echk;
	TAG *tp;
	char *name, *file, *line;
	char command[BUFSIZ];
	char buf[BUFSIZ];
	FILE *fp;

	/* Allocate and initialize the tag queue structure. */
	len = strlen(tag);
	CALLOC_GOTO(sp, tqp, TAGQ *, 1, sizeof(TAGQ) + len + 1);
	CIRCLEQ_INIT(&tqp->tagq);
	tqp->tag = tqp->buf;
	memcpy(tqp->tag, tag, (tqp->tlen = len) + 1);

	/*
	 * Find the tag, only display missing file messages once, and
	 * then only if we didn't find the tag.
	 */
	snprintf(command, sizeof(command), "global -%s '%s'", ref ? "rx" : "x", tag);
	if (fp = popen(command, "r")) {
		while (fgets(buf, sizeof(buf), fp)) {
			if (buf[strlen(buf)-1] == '\n')		/* chop(buf) */
				buf[strlen(buf)-1] = 0;
			else
				while (fgetc(fp) != '\n')
					;
			if (getentry(buf, &name, &file, &line) == 0) {
				echk = 1;
				F_SET(tfp, TAGF_ERR);
				break;
			}
			CALLOC_GOTO(sp, tp,
			    TAG *, 1, sizeof(TAG) + strlen(file) + 1 + strlen(line) + 1);
			tp->fname = tp->buf;
			strcpy(tp->fname, file);
			tp->fnlen = strlen(file);
			tp->search = tp->fname + tp->fnlen + 1;
			strcpy(tp->search, line);
			CIRCLEQ_INSERT_TAIL(&tqp->tagq, tp, q);
		}
		pclose(fp);
	}

	/* Check to see if we found anything. */
	if (tqp->tagq.cqh_first == (void *)&tqp->tagq) {
		msgq_str(sp, M_ERR, tag, "162|%s: tag not found");
		free(tqp);
		return (NULL);
	}

	tqp->current = tqp->tagq.cqh_first;
	return (tqp);

alloc_err:
	return (NULL);
}
#endif
/*
 * ctag_slist --
 *	Search the list of tags files for a tag, and return tag queue.
 */
static TAGQ *
ctag_slist(sp, tag)
	SCR *sp;
	char *tag;
{
	EX_PRIVATE *exp;
	TAGF *tfp;
	TAGQ *tqp;
	size_t len;
	int echk;

	exp = EXP(sp);

	/* Allocate and initialize the tag queue structure. */
	len = strlen(tag);
	CALLOC_GOTO(sp, tqp, TAGQ *, 1, sizeof(TAGQ) + len + 1);
	CIRCLEQ_INIT(&tqp->tagq);
	tqp->tag = tqp->buf;
	memcpy(tqp->tag, tag, (tqp->tlen = len) + 1);

	/*
	 * Find the tag, only display missing file messages once, and
	 * then only if we didn't find the tag.
	 */
	for (echk = 0,
	    tfp = exp->tagfq.tqh_first; tfp != NULL; tfp = tfp->q.tqe_next)
		if (ctag_sfile(sp, tfp, tqp, tag)) {
			echk = 1;
			F_SET(tfp, TAGF_ERR);
		} else
			F_CLR(tfp, TAGF_ERR | TAGF_ERR_WARN);

	/* Check to see if we found anything. */
	if (tqp->tagq.cqh_first == (void *)&tqp->tagq) {
		msgq_str(sp, M_ERR, tag, "162|%s: tag not found");
		if (echk)
			for (tfp = exp->tagfq.tqh_first;
			    tfp != NULL; tfp = tfp->q.tqe_next)
				if (F_ISSET(tfp, TAGF_ERR) &&
				    !F_ISSET(tfp, TAGF_ERR_WARN)) {
					errno = tfp->errnum;
					msgq_str(sp, M_SYSERR, tfp->name, "%s");
					F_SET(tfp, TAGF_ERR_WARN);
				}
		free(tqp);
		return (NULL);
	}

	tqp->current = tqp->tagq.cqh_first;
	return (tqp);

alloc_err:
	return (NULL);
}

/*
 * ctag_sfile --
 *	Search a tags file for a tag, adding any found to the tag queue.
 */
static int
ctag_sfile(sp, tfp, tqp, tname)
	SCR *sp;
	TAGF *tfp;
	TAGQ *tqp;
	char *tname;
{
	struct stat sb;
	TAG *tp;
	size_t dlen, nlen, slen;
	int fd, i, nf1, nf2;
	char *back, *cname, *dname, *front, *map, *name, *p, *search, *t;

	if ((fd = open(tfp->name, O_RDONLY, 0)) < 0) {
		tfp->errnum = errno;
		return (1);
	}

	/*
	 * XXX
	 * Some old BSD systems require MAP_FILE as an argument when mapping
	 * regular files.
	 */
#ifndef MAP_FILE
#define	MAP_FILE	0
#endif
	/*
	 * XXX
	 * We'd like to test if the file is too big to mmap.  Since we don't
	 * know what size or type off_t's or size_t's are, what the largest
	 * unsigned integral type is, or what random insanity the local C
	 * compiler will perpetrate, doing the comparison in a portable way
	 * is flatly impossible.  Hope mmap fails if the file is too large.
	 */
	if (fstat(fd, &sb) != 0 ||
	    (map = mmap(NULL, (size_t)sb.st_size, PROT_READ | PROT_WRITE,
	    MAP_FILE | MAP_PRIVATE, fd, (off_t)0)) == (caddr_t)-1) {
		tfp->errnum = errno;
		(void)close(fd);
		return (1);
	}

	front = map;
	back = front + sb.st_size;
	front = binary_search(tname, front, back);
	front = linear_search(tname, front, back);
	if (front == NULL)
		goto done;

	/*
	 * Initialize and link in the tag structure(s).  The historic ctags
	 * file format only permitted a single tag location per tag.  The
	 * obvious extension to permit multiple tags locations per tag is to
	 * output multiple records in the standard format.  Unfortunately,
	 * this won't work correctly with historic ex/vi implementations,
	 * because their binary search assumes that there's only one record
	 * per tag, and so will use a random tag entry if there si more than
	 * one.  This code handles either format.
	 *
	 * The tags file is in the following format:
	 *
	 *	<tag> <filename> <line number> | <pattern>
	 *
	 * Figure out how long everything is so we can allocate in one swell
	 * foop, but discard anything that looks wrong.
	 */
	for (;;) {
		/* Nul-terminate the end of the line. */
		for (p = front; p < back && *p != '\n'; ++p);
		if (p == back || *p != '\n')
			break;
		*p = '\0';

		/* Update the pointers for the next time. */
		t = p + 1;
		p = front;
		front = t;

		/* Break the line into tokens. */
		for (i = 0; i < 2 && (t = strsep(&p, "\t ")) != NULL; ++i)
			switch (i) {
			case 0:			/* Tag. */
				cname = t;
				break;
			case 1:			/* Filename. */
				name = t;
				nlen = strlen(name);
				break;
			}

		/* Check for corruption. */
		if (i != 2 || p == NULL || t == NULL)
			goto corrupt;

		/* The rest of the string is the search pattern. */
		search = p;
		if ((slen = strlen(p)) == 0) {
corrupt:		p = msg_print(sp, tname, &nf1);
			t = msg_print(sp, tfp->name, &nf2);
			msgq(sp, M_ERR, "163|%s: corrupted tag in %s", p, t);
			if (nf1)
				FREE_SPACE(sp, p, 0);
			if (nf2)
				FREE_SPACE(sp, t, 0);
			continue;
		}

		/* Check for passing the last entry. */
		if (strcmp(tname, cname))
			break;

		/* Resolve the file name. */
		ctag_file(sp, tfp, name, &dname, &dlen);

		CALLOC_GOTO(sp, tp,
		    TAG *, 1, sizeof(TAG) + dlen + 2 + nlen + 1 + slen + 1);
		tp->fname = tp->buf;
		if (dlen != 0) {
			memcpy(tp->fname, dname, dlen);
			tp->fname[dlen] = '/';
			++dlen;
		}
		memcpy(tp->fname + dlen, name, nlen + 1);
		tp->fnlen = dlen + nlen;
		tp->search = tp->fname + tp->fnlen + 1;
		memcpy(tp->search, search, (tp->slen = slen) + 1);
		CIRCLEQ_INSERT_TAIL(&tqp->tagq, tp, q);
	}

alloc_err:
done:	if (munmap(map, (size_t)sb.st_size))
		msgq(sp, M_SYSERR, "munmap");
	if (close(fd))
		msgq(sp, M_SYSERR, "close");
	return (0);
}

/*
 * ctag_file --
 *	Search for the right path to this file.
 */
static void
ctag_file(sp, tfp, name, dirp, dlenp)
	SCR *sp;
	TAGF *tfp;
	char *name, **dirp;
	size_t *dlenp;
{
	struct stat sb;
	size_t len;
	char *p, buf[MAXPATHLEN];

	/*
	 * !!!
	 * If the tag file path is a relative path, see if it exists.  If it
	 * doesn't, look relative to the tags file path.  It's okay for a tag
	 * file to not exist, and historically, vi simply displayed a "new"
	 * file.  However, if the path exists relative to the tag file, it's
	 * pretty clear what's happening, so we may as well get it right.
	 */
	*dlenp = 0;
	if (name[0] != '/' &&
	    stat(name, &sb) && (p = strrchr(tfp->name, '/')) != NULL) {
		*p = '\0';
		len = snprintf(buf, sizeof(buf), "%s/%s", tfp->name, name);
		*p = '/';
		if (stat(buf, &sb) == 0) {
			*dirp = tfp->name;
			*dlenp = strlen(*dirp);
		}
	}
}

/*
 * Binary search for "string" in memory between "front" and "back".
 *
 * This routine is expected to return a pointer to the start of a line at
 * *or before* the first word matching "string".  Relaxing the constraint
 * this way simplifies the algorithm.
 *
 * Invariants:
 * 	front points to the beginning of a line at or before the first
 *	matching string.
 *
 * 	back points to the beginning of a line at or after the first
 *	matching line.
 *
 * Base of the Invariants.
 * 	front = NULL;
 *	back = EOF;
 *
 * Advancing the Invariants:
 *
 * 	p = first newline after halfway point from front to back.
 *
 * 	If the string at "p" is not greater than the string to match,
 *	p is the new front.  Otherwise it is the new back.
 *
 * Termination:
 *
 * 	The definition of the routine allows it return at any point,
 *	since front is always at or before the line to print.
 *
 * 	In fact, it returns when the chosen "p" equals "back".  This
 *	implies that there exists a string is least half as long as
 *	(back - front), which in turn implies that a linear search will
 *	be no more expensive than the cost of simply printing a string or two.
 *
 * 	Trying to continue with binary search at this point would be
 *	more trouble than it's worth.
 */
#define	EQUAL		0
#define	GREATER		1
#define	LESS		(-1)

#define	SKIP_PAST_NEWLINE(p, back)	while (p < back && *p++ != '\n');

static char *
binary_search(string, front, back)
	register char *string, *front, *back;
{
	register char *p;

	p = front + (back - front) / 2;
	SKIP_PAST_NEWLINE(p, back);

	while (p != back) {
		if (compare(string, p, back) == GREATER)
			front = p;
		else
			back = p;
		p = front + (back - front) / 2;
		SKIP_PAST_NEWLINE(p, back);
	}
	return (front);
}

/*
 * Find the first line that starts with string, linearly searching from front
 * to back.
 *
 * Return NULL for no such line.
 *
 * This routine assumes:
 *
 * 	o front points at the first character in a line.
 *	o front is before or at the first line to be printed.
 */
static char *
linear_search(string, front, back)
	char *string, *front, *back;
{
	while (front < back) {
		switch (compare(string, front, back)) {
		case EQUAL:		/* Found it. */
			return (front);
		case LESS:		/* No such string. */
			return (NULL);
		case GREATER:		/* Keep going. */
			break;
		}
		SKIP_PAST_NEWLINE(front, back);
	}
	return (NULL);
}

/*
 * Return LESS, GREATER, or EQUAL depending on how the string1 compares
 * with string2 (s1 ??? s2).
 *
 * 	o Matches up to len(s1) are EQUAL.
 *	o Matches up to len(s2) are GREATER.
 *
 * The string "s1" is null terminated.  The string s2 is '\t', space, (or
 * "back") terminated.
 *
 * !!!
 * Reasonably modern ctags programs use tabs as separators, not spaces.
 * However, historic programs did use spaces, and, I got complaints.
 */
static int
compare(s1, s2, back)
	register char *s1, *s2, *back;
{
	for (; *s1 && s2 < back && (*s2 != '\t' && *s2 != ' '); ++s1, ++s2)
		if (*s1 != *s2)
			return (*s1 < *s2 ? LESS : GREATER);
	return (*s1 ? GREATER : s2 < back &&
	    (*s2 != '\t' && *s2 != ' ') ? LESS : EQUAL);
}
