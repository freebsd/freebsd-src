/*-
 * Copyright (c) 1998 Berkeley Software Design, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Berkeley Software Design Inc's name may not be used to endorse or
 *    promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BERKELEY SOFTWARE DESIGN INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL BERKELEY SOFTWARE DESIGN INC BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from BSDI $Id: mutex_witness.c,v 1.1.2.20 2000/04/27 03:10:27 cp Exp $
 * $FreeBSD$
 */

/*
 *	Main Entry: witness
 *	Pronunciation: 'wit-n&s
 *	Function: noun
 *	Etymology: Middle English witnesse, from Old English witnes knowledge,
 *	    testimony, witness, from 2wit
 *	Date: before 12th century
 *	1 : attestation of a fact or event : TESTIMONY
 *	2 : one that gives evidence; specifically : one who testifies in
 *	    a cause or before a judicial tribunal
 *	3 : one asked to be present at a transaction so as to be able to
 *	    testify to its having taken place
 *	4 : one who has personal knowledge of something
 *	5 a : something serving as evidence or proof : SIGN
 *	  b : public affirmation by word or example of usually
 *	      religious faith or conviction <the heroic witness to divine
 *	      life -- Pilot>
 *	6 capitalized : a member of the Jehovah's Witnesses 
 */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/ktr.h>

#include <machine/cpu.h>
#define _KERN_MUTEX_C_		/* Cause non-inlined mtx_*() to be compiled. */
#include <machine/mutex.h>

/*
 * The non-inlined versions of the mtx_*() functions are always built (above),
 * but the witness code depends on the SMP_DEBUG and WITNESS kernel options
 * being specified.
 */
#if (defined(SMP_DEBUG) && defined(WITNESS))

#define WITNESS_COUNT 200
#define	WITNESS_NCHILDREN 2

#ifndef WITNESS
#define	WITNESS		0	/* default off */
#endif

#ifndef SMP
extern int witness_spin_check;
#endif

int witness_watch;

typedef struct witness {
	struct witness	*w_next;
	char		*w_description;
	const char	*w_file;
	int		 w_line;
	struct witness	*w_morechildren;
	u_char		 w_childcnt;
	u_char		 w_Giant_squawked:1;
	u_char		 w_other_squawked:1;
	u_char		 w_same_squawked:1;
	u_char		 w_sleep:1;
	u_char		 w_spin:1;	/* this is a spin mutex */
	u_int		 w_level;
	struct witness	*w_children[WITNESS_NCHILDREN];
} witness_t;

typedef struct witness_blessed {
	char 	*b_lock1;
	char	*b_lock2;
} witness_blessed_t;

#ifdef KDEBUG
/*
 * When WITNESS_KDEBUG is set to 1, it will cause the system to
 * drop into kdebug() when:
 *	- a lock heirarchy violation occurs
 *	- locks are held when going to sleep.
 */
#ifndef WITNESS_KDEBUG
#define WITNESS_KDEBUG 0
#endif
int	witness_kdebug = WITNESS_KDEBUG;
#endif /* KDEBUG */

#ifndef WITNESS_SKIPSPIN
#define WITNESS_SKIPSPIN 0
#endif
int	witness_skipspin = WITNESS_SKIPSPIN;


static mtx_t	 w_mtx;
static witness_t *w_free;
static witness_t *w_all;
static int	 w_inited;
static int	 witness_dead;	/* fatal error, probably no memory */

static witness_t w_data[WITNESS_COUNT];

static witness_t *enroll __P((char *description, int flag));
static int itismychild __P((witness_t *parent, witness_t *child));
static void removechild __P((witness_t *parent, witness_t *child));
static int isitmychild __P((witness_t *parent, witness_t *child));
static int isitmydescendant __P((witness_t *parent, witness_t *child));
static int dup_ok __P((witness_t *));
static int blessed __P((witness_t *, witness_t *));
static void witness_displaydescendants
    __P((void(*)(const char *fmt, ...), witness_t *));
static void witness_leveldescendents __P((witness_t *parent, int level));
static void witness_levelall __P((void));
static witness_t * witness_get __P((void));
static void witness_free __P((witness_t *m));


static char *ignore_list[] = {
	"witness lock",
	"Kdebug",		/* breaks rules and may or may not work */
	"Page Alias",		/* sparc only, witness lock won't block intr */
	NULL
};

static char *spin_order_list[] = {
	"sched lock",
	"log mtx",
	"zslock",	/* sparc only above log, this one is a real hack */
	"time lock",	/* above callout */
	"callout mtx",	/* above wayout */
	/*
	 * leaf locks
	 */
	"wayout mtx",
	"kernel_pmap",  /* sparc only, logically equal "pmap" below */
	"pmap",		/* sparc only */
	NULL
};

static char *order_list[] = {
	"tcb", "inp", "so_snd", "so_rcv", "Giant lock", NULL,
	"udb", "inp", NULL,
	"unp head", "unp", "so_snd", NULL,
	"de0", "Giant lock", NULL,
	"ifnet", "Giant lock", NULL,
	"fifo", "so_snd", NULL,
	"hme0", "Giant lock", NULL,
	"esp0", "Giant lock", NULL,
	"hfa0", "Giant lock", NULL,
	"so_rcv", "atm_global", NULL,
	"so_snd", "atm_global", NULL,
	"NFS", "Giant lock", NULL,
	NULL
};

static char *dup_list[] = {
	"inp",
	"process group",
	"session",
	"unp",
	"rtentry",
	"rawcb",
	NULL
};

static char *sleep_list[] = {
	"Giant lock",
	NULL
};

/*
 * Pairs of locks which have been blessed
 * Don't complain about order problems with blessed locks
 */
static witness_blessed_t blessed_list[] = {
};
static int blessed_count = sizeof (blessed_list) / sizeof (witness_blessed_t);

void
witness_init(mtx_t *m, int flag)
{
	m->mtx_witness = enroll(m->mtx_description, flag);
}

void
witness_destroy(mtx_t *m)
{
	mtx_t *m1;
	struct proc *p;
	p = CURPROC;
	for ((m1 = LIST_FIRST(&p->p_heldmtx)); m1 != NULL;
		m1 = LIST_NEXT(m1, mtx_held)) {
		if (m1 == m) {
			LIST_REMOVE(m, mtx_held);
			break;
		}
	}
	return;

}

void
witness_enter(mtx_t *m, int flags, const char *file, int line)
{
	witness_t *w, *w1;
	mtx_t *m1;
	struct proc *p;
	int i;
#ifdef KDEBUG
	int go_into_kdebug = 0;
#endif /* KDEBUG */

	w = m->mtx_witness;
	p = CURPROC;

	if (flags & MTX_SPIN) {
		if (!w->w_spin)
			panic("mutex_enter: MTX_SPIN on MTX_DEF mutex %s @"
			    " %s:%d", m->mtx_description, file, line);
		if (m->mtx_recurse != 0)
			return;
		mtx_enter(&w_mtx, MTX_SPIN);
		i = witness_spin_check;
		if (i != 0 && w->w_level < i) {
			mtx_exit(&w_mtx, MTX_SPIN);
			panic("mutex_enter(%s:%x, MTX_SPIN) out of order @"
			    " %s:%d already holding %s:%x",
			    m->mtx_description, w->w_level, file, line,
			    spin_order_list[ffs(i)-1], i);
		}
		PCPU_SET(witness_spin_check, i | w->w_level);
		mtx_exit(&w_mtx, MTX_SPIN);
		return;
	}
	if (w->w_spin)
		panic("mutex_enter: MTX_DEF on MTX_SPIN mutex %s @ %s:%d",
		    m->mtx_description, file, line);

	if (m->mtx_recurse != 0)
		return;
	if (witness_dead)
		goto out;
	if (cold)
		goto out;

	if (!mtx_legal2block())
		panic("blockable mtx_enter() of %s when not legal @ %s:%d",
			    m->mtx_description, file, line);
	/*
	 * Is this the first mutex acquired 
	 */
	if ((m1 = LIST_FIRST(&p->p_heldmtx)) == NULL)
		goto out;

	if ((w1 = m1->mtx_witness) == w) {
		if (w->w_same_squawked || dup_ok(w))
			goto out;
		w->w_same_squawked = 1;
		printf("acquring duplicate lock of same type: \"%s\"\n", 
			m->mtx_description);
		printf(" 1st @ %s:%d\n", w->w_file, w->w_line);
		printf(" 2nd @ %s:%d\n", file, line);
#ifdef KDEBUG
		go_into_kdebug = 1;
#endif /* KDEBUG */
		goto out;
	}
	MPASS(!mtx_owned(&w_mtx));
	mtx_enter(&w_mtx, MTX_SPIN);
	/*
	 * If we have a known higher number just say ok
	 */
	if (witness_watch > 1 && w->w_level > w1->w_level) {
		mtx_exit(&w_mtx, MTX_SPIN);
		goto out;
	}
	if (isitmydescendant(m1->mtx_witness, w)) {
		mtx_exit(&w_mtx, MTX_SPIN);
		goto out;
	}
	for (i = 0; m1 != NULL; m1 = LIST_NEXT(m1, mtx_held), i++) {

		ASS(i < 200);
		w1 = m1->mtx_witness;
		if (isitmydescendant(w, w1)) {
			mtx_exit(&w_mtx, MTX_SPIN);
			if (blessed(w, w1))
				goto out;
			if (m1 == &Giant) {
				if (w1->w_Giant_squawked)
					goto out;
				else
					w1->w_Giant_squawked = 1;
			} else {
				if (w1->w_other_squawked)
					goto out;
				else
					w1->w_other_squawked = 1;
			}
			printf("lock order reversal\n");
			printf(" 1st %s last acquired @ %s:%d\n",
			    w->w_description, w->w_file, w->w_line);
			printf(" 2nd %p %s @ %s:%d\n",
			    m1, w1->w_description, w1->w_file, w1->w_line);
			printf(" 3rd %p %s @ %s:%d\n",
			    m, w->w_description, file, line);
#ifdef KDEBUG
			go_into_kdebug = 1;
#endif /* KDEBUG */
			goto out;
		}
	}
	m1 = LIST_FIRST(&p->p_heldmtx);
	if (!itismychild(m1->mtx_witness, w))
		mtx_exit(&w_mtx, MTX_SPIN);

out:
#ifdef KDEBUG
	if (witness_kdebug && go_into_kdebug)
		kdebug();
#endif /* KDEBUG */
	w->w_file = file;
	w->w_line = line;
	m->mtx_line = line;
	m->mtx_file = file;

	/*
	 * If this pays off it likely means that a mutex  being witnessed
	 * is acquired in hardclock. Put it in the ignore list. It is
	 * likely not the mutex this assert fails on.
	 */
	ASS(m->mtx_held.le_prev == NULL);
	LIST_INSERT_HEAD(&p->p_heldmtx, (struct mtx*)m, mtx_held);
}

void
witness_exit(mtx_t *m, int flags, const char *file, int line)
{
	witness_t *w;

	w = m->mtx_witness;

	if (flags & MTX_SPIN) {
		if (!w->w_spin)
			panic("mutex_exit: MTX_SPIN on MTX_DEF mutex %s @"
			    " %s:%d", m->mtx_description, file, line);
		if (m->mtx_recurse != 0)
			return;
		mtx_enter(&w_mtx, MTX_SPIN);
		PCPU_SET(witness_spin_check, witness_spin_check & ~w->w_level);
		mtx_exit(&w_mtx, MTX_SPIN);
		return;
	}
	if (w->w_spin)
		panic("mutex_exit: MTX_DEF on MTX_SPIN mutex %s @ %s:%d",
		    m->mtx_description, file, line);

	if (m->mtx_recurse != 0)
		return;

	if ((flags & MTX_NOSWITCH) == 0 && !mtx_legal2block() && !cold)
		panic("switchable mtx_exit() of %s when not legal @ %s:%d",
			    m->mtx_description, file, line);
	LIST_REMOVE(m, mtx_held);
	m->mtx_held.le_prev = NULL;
}

void
witness_try_enter(mtx_t *m, int flags, const char *file, int line)
{
	struct proc *p;
	witness_t *w = m->mtx_witness;

	if (flags & MTX_SPIN) {
		if (!w->w_spin)
			panic("mutex_try_enter: "
			    "MTX_SPIN on MTX_DEF mutex %s @ %s:%d",
			    m->mtx_description, file, line);
		if (m->mtx_recurse != 0)
			return;
		mtx_enter(&w_mtx, MTX_SPIN);
		PCPU_SET(witness_spin_check, witness_spin_check | w->w_level);
		mtx_exit(&w_mtx, MTX_SPIN);
		return;
	}

	if (w->w_spin)
		panic("mutex_try_enter: MTX_DEF on MTX_SPIN mutex %s @ %s:%d",
		    m->mtx_description, file, line);

	if (m->mtx_recurse != 0)
		return;

	w->w_file = file;
	w->w_line = line;
	m->mtx_line = line;
	m->mtx_file = file;
	p = CURPROC;
	ASS(m->mtx_held.le_prev == NULL);
	LIST_INSERT_HEAD(&p->p_heldmtx, (struct mtx*)m, mtx_held);
}

void
witness_display(void(*prnt)(const char *fmt, ...))
{
	witness_t *w, *w1;

	witness_levelall();

	for (w = w_all; w; w = w->w_next) {
		if (w->w_file == NULL)
			continue;
		for (w1 = w_all; w1; w1 = w1->w_next) {
			if (isitmychild(w1, w))
				break;
		}
		if (w1 != NULL)
			continue;
		/*
		 * This lock has no anscestors, display its descendants. 
		 */
		witness_displaydescendants(prnt, w);
	}
	prnt("\nMutex which were never acquired\n");
	for (w = w_all; w; w = w->w_next) {
		if (w->w_file != NULL)
			continue;
		prnt("%s\n", w->w_description);
	}
}

int
witness_sleep(int check_only, mtx_t *mtx, const char *file, int line)
{
	mtx_t *m;
	struct proc *p;
	char **sleep;
	int n = 0;

	p = CURPROC;
	for ((m = LIST_FIRST(&p->p_heldmtx)); m != NULL;
	    m = LIST_NEXT(m, mtx_held)) {
		if (m == mtx)
			continue;
		for (sleep = sleep_list; *sleep!= NULL; sleep++)
			if (strcmp(m->mtx_description, *sleep) == 0)
				goto next;
		printf("%s:%d: %s with \"%s\" locked from %s:%d\n",
			file, line, check_only ? "could sleep" : "sleeping",
			m->mtx_description,
			m->mtx_witness->w_file, m->mtx_witness->w_line);
		n++;
	next:
	}
#ifdef KDEBUG
	if (witness_kdebug && n)
		kdebug();
#endif /* KDEBUG */
	return (n);
}

static witness_t *
enroll(char *description, int flag)
{
	int i;
	witness_t *w, *w1;
	char **ignore;
	char **order;

	if (!witness_watch)
		return (NULL);
	for (ignore = ignore_list; *ignore != NULL; ignore++)
		if (strcmp(description, *ignore) == 0)
			return (NULL);

	if (w_inited == 0) {
		mtx_init(&w_mtx, "witness lock", MTX_DEF);
		for (i = 0; i < WITNESS_COUNT; i++) {
			w = &w_data[i];
			witness_free(w);
		}
		w_inited = 1;
		for (order = order_list; *order != NULL; order++) {
			w = enroll(*order, MTX_DEF);
			w->w_file = "order list";
			for (order++; *order != NULL; order++) {
				w1 = enroll(*order, MTX_DEF);
				w1->w_file = "order list";
				itismychild(w, w1);
				w = w1;
    	    	    	}
		}
	}
	if ((flag & MTX_SPIN) && witness_skipspin)
		return (NULL);
	mtx_enter(&w_mtx, MTX_SPIN);
	for (w = w_all; w; w = w->w_next) {
		if (strcmp(description, w->w_description) == 0) {
			mtx_exit(&w_mtx, MTX_SPIN);
			return (w);
		}
	}
	if ((w = witness_get()) == NULL)
		return (NULL);
	w->w_next = w_all;
	w_all = w;
	w->w_description = description;
	mtx_exit(&w_mtx, MTX_SPIN);
	if (flag & MTX_SPIN) {
		w->w_spin = 1;
	
		i = 1;
		for (order = spin_order_list; *order != NULL; order++) {
			if (strcmp(description, *order) == 0)
				break;
			i <<= 1;
		}
		if (*order == NULL)
			panic("spin lock %s not in order list", description);
		w->w_level = i; 
	}
	return (w);
}

static int
itismychild(witness_t *parent, witness_t *child)
{
	static int recursed;

	/*
	 * Insert "child" after "parent"
	 */
	while (parent->w_morechildren)
		parent = parent->w_morechildren;

	if (parent->w_childcnt == WITNESS_NCHILDREN) {
		if ((parent->w_morechildren = witness_get()) == NULL)
			return (1);
		parent = parent->w_morechildren;
	}
	ASS(child != NULL);
	parent->w_children[parent->w_childcnt++] = child;
	/*
	 * now prune whole tree
	 */
	if (recursed)
		return (0);
	recursed = 1;
	for (child = w_all; child != NULL; child = child->w_next) {
		for (parent = w_all; parent != NULL;
		    parent = parent->w_next) {
			if (!isitmychild(parent, child))
				continue;
			removechild(parent, child);
			if (isitmydescendant(parent, child))
				continue;
			itismychild(parent, child);
		}
	}
	recursed = 0;
	witness_levelall();
	return (0);
}

static void
removechild(witness_t *parent, witness_t *child)
{
	witness_t *w, *w1;
	int i;

	for (w = parent; w != NULL; w = w->w_morechildren)
		for (i = 0; i < w->w_childcnt; i++)
			if (w->w_children[i] == child)
				goto found;
	return;
found:
	for (w1 = w; w1->w_morechildren != NULL; w1 = w1->w_morechildren)
		continue;
	w->w_children[i] = w1->w_children[--w1->w_childcnt];
	ASS(w->w_children[i] != NULL);

	if (w1->w_childcnt != 0)
		return;

	if (w1 == parent)
		return;
	for (w = parent; w->w_morechildren != w1; w = w->w_morechildren)
		continue;
	w->w_morechildren = 0;
	witness_free(w1);
}

static int
isitmychild(witness_t *parent, witness_t *child)
{
	witness_t *w;
	int i;

	for (w = parent; w != NULL; w = w->w_morechildren) {
		for (i = 0; i < w->w_childcnt; i++) {
			if (w->w_children[i] == child)
				return (1);
		}
	}
	return (0);
}

static int
isitmydescendant(witness_t *parent, witness_t *child)
{
	witness_t *w;
	int i;
	int j;

	for (j = 0, w = parent; w != NULL; w = w->w_morechildren, j++) {
		ASS(j < 1000);
		for (i = 0; i < w->w_childcnt; i++) {
			if (w->w_children[i] == child)
				return (1);
		}
		for (i = 0; i < w->w_childcnt; i++) {
			if (isitmydescendant(w->w_children[i], child))
				return (1);
		}
	}
	return (0);
}

void
witness_levelall (void)
{
	witness_t *w, *w1;

	for (w = w_all; w; w = w->w_next)
		if (!w->w_spin)
			w->w_level = 0;
	for (w = w_all; w; w = w->w_next) {
		if (w->w_spin)
			continue;
		for (w1 = w_all; w1; w1 = w1->w_next) {
			if (isitmychild(w1, w))
				break;
		}
		if (w1 != NULL)
			continue;
		witness_leveldescendents(w, 0);
	}
}

static void
witness_leveldescendents(witness_t *parent, int level)
{
	int i;
	witness_t *w;

	if (parent->w_level < level)
		parent->w_level = level;
	level++;
	for (w = parent; w != NULL; w = w->w_morechildren)
		for (i = 0; i < w->w_childcnt; i++)
			witness_leveldescendents(w->w_children[i], level);
}

static void
witness_displaydescendants(void(*prnt)(const char *fmt, ...), witness_t *parent)
{
	witness_t *w;
	int i;
	int level = parent->w_level;

	prnt("%d", level);
	if (level < 10)
		prnt(" ");
	for (i = 0; i < level; i++)
		prnt(" ");
	prnt("%s", parent->w_description);
	if (parent->w_file != NULL) {
		prnt(" -- last acquired @ %s", parent->w_file);
#ifndef W_USE_WHERE
		prnt(":%d", parent->w_line);
#endif
		prnt("\n");
	}

	for (w = parent; w != NULL; w = w->w_morechildren)
		for (i = 0; i < w->w_childcnt; i++)
			    witness_displaydescendants(prnt, w->w_children[i]);
    }

static int
dup_ok(witness_t *w)
{
	char **dup;
	
	for (dup = dup_list; *dup!= NULL; dup++)
		if (strcmp(w->w_description, *dup) == 0)
			return (1);
	return (0);
}

static int
blessed(witness_t *w1, witness_t *w2)
{
	int i;
	witness_blessed_t *b;

	for (i = 0; i < blessed_count; i++) {
		b = &blessed_list[i];
		if (strcmp(w1->w_description, b->b_lock1) == 0) {
			if (strcmp(w2->w_description, b->b_lock2) == 0)
				return (1);
			continue;
		}
		if (strcmp(w1->w_description, b->b_lock2) == 0)
			if (strcmp(w2->w_description, b->b_lock1) == 0)
				return (1);
	}
	return (0);
}

static witness_t *
witness_get()
{
	witness_t *w;

	if ((w = w_free) == NULL) {
		witness_dead = 1;
		mtx_exit(&w_mtx, MTX_SPIN);
		printf("witness exhausted\n");
		return (NULL);
	}
	w_free = w->w_next;
	bzero(w, sizeof (*w));
	return (w);
}

static void
witness_free(witness_t *w)
{
	w->w_next = w_free;
	w_free = w;
}

void
witness_list(struct proc *p)
{
	mtx_t *m;

	for ((m = LIST_FIRST(&p->p_heldmtx)); m != NULL;
	    m = LIST_NEXT(m, mtx_held)) {
		printf("\t\"%s\" (%p) locked at %s:%d\n",
		    m->mtx_description, m,
		    m->mtx_witness->w_file, m->mtx_witness->w_line);
	}
}

void
witness_save(mtx_t *m, const char **filep, int *linep)
{
	*filep = m->mtx_witness->w_file;
	*linep = m->mtx_witness->w_line;
}

void
witness_restore(mtx_t *m, const char *file, int line)
{
	m->mtx_witness->w_file = file;
	m->mtx_witness->w_line = line;
}

#endif	/* (defined(SMP_DEBUG) && defined(WITNESS)) */
