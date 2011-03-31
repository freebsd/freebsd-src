/*	$NetBSD: history.c,v 1.5 1997/04/11 17:52:46 christos Exp $	*/

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Christos Zoulas of Cornell University.
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
 */

#if !defined(lint) && !defined(SCCSID)
static char sccsid[] = "@(#)history.c	8.1 (Berkeley) 6/4/93";
#endif /* not lint && not SCCSID */

/*
 * hist.c: History access functions
 */
#include "sys.h"

#include <string.h>
#include <stdlib.h>
#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

static const char hist_cookie[] = "_HiStOrY_V1_\n";

#include "histedit.h"

typedef const HistEvent *	(*history_gfun_t) __P((ptr_t));
typedef const HistEvent *	(*history_efun_t) __P((ptr_t, const char *));
typedef void 			(*history_vfun_t) __P((ptr_t));

struct history {
    ptr_t	   h_ref;		/* Argument for history fcns	*/
    history_gfun_t h_first;		/* Get the first element	*/
    history_gfun_t h_next;		/* Get the next element		*/
    history_gfun_t h_last;		/* Get the last element		*/
    history_gfun_t h_prev;		/* Get the previous element	*/
    history_gfun_t h_curr;		/* Get the current element	*/
    history_vfun_t h_clear;		/* Clear the history list	*/ 
    history_efun_t h_enter;		/* Add an element		*/
    history_efun_t h_add;		/* Append to an element		*/
};

#define	HNEXT(h)  	(*(h)->h_next)((h)->h_ref)
#define	HFIRST(h) 	(*(h)->h_first)((h)->h_ref)
#define	HPREV(h)  	(*(h)->h_prev)((h)->h_ref)
#define	HLAST(h) 	(*(h)->h_last)((h)->h_ref)
#define	HCURR(h) 	(*(h)->h_curr)((h)->h_ref)
#define	HCLEAR(h) 	(*(h)->h_clear)((h)->h_ref)
#define	HENTER(h, str)	(*(h)->h_enter)((h)->h_ref, str)
#define	HADD(h, str)	(*(h)->h_add)((h)->h_ref, str)

#define h_malloc(a)	malloc(a)
#define h_free(a)	free(a)


private int		 history_set_num	__P((History *, int));
private int		 history_set_fun	__P((History *, History *));
private int 		 history_load		__P((History *, const char *));
private int 		 history_save		__P((History *, const char *));
private const HistEvent *history_prev_event	__P((History *, int));
private const HistEvent *history_next_event	__P((History *, int));
private const HistEvent *history_next_string	__P((History *, const char *));
private const HistEvent *history_prev_string	__P((History *, const char *));


/***********************************************************************/

/*
 * Builtin- history implementation
 */
typedef struct hentry_t {
    HistEvent ev;		/* What we return		*/
    struct hentry_t *next;	/* Next entry			*/
    struct hentry_t *prev;	/* Previous entry		*/
} hentry_t;

typedef struct history_t {
    hentry_t  list;		/* Fake list header element	*/
    hentry_t *cursor;		/* Current element in the list	*/
    int	max;			/* Maximum number of events	*/
    int cur;			/* Current number of events	*/
    int	eventno;		/* Current event number		*/
} history_t;

private const HistEvent *history_def_first  __P((ptr_t));
private const HistEvent *history_def_last   __P((ptr_t));
private const HistEvent *history_def_next   __P((ptr_t));
private const HistEvent *history_def_prev   __P((ptr_t));
private const HistEvent *history_def_curr   __P((ptr_t));
private const HistEvent *history_def_enter  __P((ptr_t, const char *));
private const HistEvent *history_def_add    __P((ptr_t, const char *));
private void             history_def_init   __P((ptr_t *, int));
private void             history_def_clear  __P((ptr_t));
private const HistEvent *history_def_insert __P((history_t *, const char *));
private void             history_def_delete __P((history_t *, hentry_t *));

#define history_def_set(p, num)	(void) (((history_t *) p)->max = (num))


/* history_def_first():
 *	Default function to return the first event in the history.
 */
private const HistEvent * 
history_def_first(p)
    ptr_t p;
{
    history_t *h = (history_t *) p;
    h->cursor = h->list.next;
    if (h->cursor != &h->list)
	return &h->cursor->ev;
    else
	return NULL;
}

/* history_def_last():
 *	Default function to return the last event in the history.
 */
private const HistEvent * 
history_def_last(p)
    ptr_t p;
{
    history_t *h = (history_t *) p;
    h->cursor = h->list.prev;
    if (h->cursor != &h->list)
	return &h->cursor->ev;
    else
	return NULL;
}

/* history_def_next():
 *	Default function to return the next event in the history.
 */
private const HistEvent * 
history_def_next(p)
    ptr_t p;
{
    history_t *h = (history_t *) p;

    if (h->cursor != &h->list)
	h->cursor = h->cursor->next;
    else
	return NULL;

    if (h->cursor != &h->list)
	return &h->cursor->ev;
    else
	return NULL;
}


/* history_def_prev():
 *	Default function to return the previous event in the history.
 */
private const HistEvent * 
history_def_prev(p)
    ptr_t p;
{
    history_t *h = (history_t *) p;

    if (h->cursor != &h->list)
	h->cursor = h->cursor->prev;
    else
	return NULL;

    if (h->cursor != &h->list)
	return &h->cursor->ev;
    else
	return NULL;
}


/* history_def_curr():
 *	Default function to return the current event in the history.
 */
private const HistEvent * 
history_def_curr(p)
    ptr_t p;
{
    history_t *h = (history_t *) p;

    if (h->cursor != &h->list)
	return &h->cursor->ev;
    else
	return NULL;
}

/* history_def_add():
 *	Append string to element
 */
private const HistEvent *
history_def_add(p, str)
    ptr_t p;
    const char *str;
{
    history_t *h = (history_t *) p;
    size_t len;
    char *s;

    if (h->cursor == &h->list)
	return (history_def_enter(p, str));
    len = strlen(h->cursor->ev.str) + strlen(str) + 1;
    s = (char *) h_malloc(len);
    (void)strcpy(s, h->cursor->ev.str);	/* XXX strcpy is safe */
    (void)strcat(s, str);			/* XXX strcat is safe */
    h_free((ptr_t) h->cursor->ev.str);
    h->cursor->ev.str = s;
    return &h->cursor->ev;
}


/* history_def_delete():
 *	Delete element hp of the h list
 */
private void
history_def_delete(h, hp)
    history_t *h;
    hentry_t *hp;
{
    if (hp == &h->list)
	abort();
    hp->prev->next = hp->next;
    hp->next->prev = hp->prev;
    h_free((ptr_t) hp->ev.str);
    h_free(hp);
    h->cur--;
}


/* history_def_insert():
 *	Insert element with string str in the h list
 */
private const HistEvent *
history_def_insert(h, str)
    history_t *h;
    const char *str;
{
    h->cursor = (hentry_t *) h_malloc(sizeof(hentry_t));
    h->cursor->ev.str = strdup(str);
    h->cursor->next = h->list.next;
    h->cursor->prev = &h->list;
    h->list.next->prev = h->cursor;
    h->list.next = h->cursor;
    h->cur++;

    return &h->cursor->ev;
}


/* history_def_enter():
 *	Default function to enter an item in the history
 */
private const HistEvent *
history_def_enter(p, str)
    ptr_t p;
    const char *str;
{
    history_t *h = (history_t *) p;
    const HistEvent *ev;


    ev = history_def_insert(h, str);
    ((HistEvent*) ev)->num = ++h->eventno;

    /*
     * Always keep at least one entry.
     * This way we don't have to check for the empty list.
     */
    while (h->cur > h->max + 1) 
	history_def_delete(h, h->list.prev);
    return ev;
}


/* history_def_init():
 *	Default history initialization function
 */
private void
history_def_init(p, n)
    ptr_t *p;
    int n;
{
    history_t *h = (history_t *) h_malloc(sizeof(history_t));
    if (n <= 0)
	n = 0;
    h->eventno = 0;
    h->cur = 0;
    h->max = n;
    h->list.next = h->list.prev = &h->list;
    h->list.ev.str = NULL;
    h->list.ev.num = 0;
    h->cursor = &h->list;
    *p = (ptr_t) h;
}


/* history_def_clear():
 *	Default history cleanup function
 */
private void
history_def_clear(p)
    ptr_t p;
{
    history_t *h = (history_t *) p;

    while (h->list.prev != &h->list)
	history_def_delete(h, h->list.prev);
    h->eventno = 0;
    h->cur = 0;
}




/************************************************************************/

/* history_init():
 *	Initialization function.
 */
public History *
history_init()
{
    History *h = (History *) h_malloc(sizeof(History));

    history_def_init(&h->h_ref, 0);

    h->h_next  = history_def_next;
    h->h_first = history_def_first;
    h->h_last  = history_def_last;
    h->h_prev  = history_def_prev;
    h->h_curr  = history_def_curr;
    h->h_clear = history_def_clear;
    h->h_enter = history_def_enter;
    h->h_add   = history_def_add;

    return h;
}


/* history_end():
 *	clean up history;
 */
public void
history_end(h)
    History *h;
{
    if (h->h_next == history_def_next)
	history_def_clear(h->h_ref);
}



/* history_set_num():
 *	Set history number of events
 */
private int
history_set_num(h, num)
    History *h;
    int num;
{
    if (h->h_next != history_def_next || num < 0)
	return -1;
    history_def_set(h->h_ref, num);
    return 0;
}


/* history_set_fun():
 *	Set history functions
 */
private int
history_set_fun(h, nh)
    History *h, *nh;
{
    if (nh->h_first == NULL || nh->h_next == NULL ||
        nh->h_last == NULL  || nh->h_prev == NULL || nh->h_curr == NULL ||
	nh->h_enter == NULL || nh->h_add == NULL || nh->h_clear == NULL ||
	nh->h_ref == NULL) {
	if (h->h_next != history_def_next) {
	    history_def_init(&h->h_ref, 0);
	    h->h_first = history_def_first;
	    h->h_next  = history_def_next;
	    h->h_last  = history_def_last;
	    h->h_prev  = history_def_prev;
	    h->h_curr  = history_def_curr;
	    h->h_clear = history_def_clear;
	    h->h_enter = history_def_enter;
	    h->h_add   = history_def_add;
	}
	return -1;
    }

    if (h->h_next == history_def_next)
	history_def_clear(h->h_ref);

    h->h_first = nh->h_first;
    h->h_next  = nh->h_next;
    h->h_last  = nh->h_last;
    h->h_prev  = nh->h_prev;
    h->h_curr  = nh->h_curr;
    h->h_clear = nh->h_clear;
    h->h_enter = nh->h_enter;
    h->h_add   = nh->h_add;

    return 0;
}


/* history_load():
 *	History load function
 */
private int
history_load(h, fname)
    History *h;
    const char *fname;
{
    FILE *fp;
    char *line;
    size_t sz;
    int i = -1;

    if ((fp = fopen(fname, "r")) == NULL)
	return i;

    if ((line = fgetln(fp, &sz)) == NULL)
	goto done;

    if (strncmp(line, hist_cookie, sz) != 0)
	goto done;
	
    for (i = 0; (line = fgetln(fp, &sz)) != NULL; i++) {
	char c = line[sz];
	line[sz] = '\0';
	HENTER(h, line);
	line[sz] = c;
    }

done:
    (void) fclose(fp);
    return i;
}


/* history_save():
 *	History save function
 */
private int
history_save(h, fname)
    History *h;
    const char *fname;
{
    FILE *fp;
    const HistEvent *ev;
    int i = 0;

    if ((fp = fopen(fname, "w")) == NULL)
	return -1;

    (void) fputs(hist_cookie, fp);
    for (ev = HLAST(h); ev != NULL; ev = HPREV(h), i++)
	(void) fprintf(fp, "%s", ev->str);
    (void) fclose(fp);
    return i;
}


/* history_prev_event():
 *	Find the previous event, with number given
 */
private const HistEvent *
history_prev_event(h, num)
    History *h;
    int num;
{
    const HistEvent *ev;
    for (ev = HCURR(h); ev != NULL; ev = HPREV(h))
	if (ev->num == num)
	    return ev;
    return NULL;
}


/* history_next_event():
 *	Find the next event, with number given
 */
private const HistEvent *
history_next_event(h, num)
    History *h;
    int num;
{
    const HistEvent *ev;
    for (ev = HCURR(h); ev != NULL; ev = HNEXT(h))
	if (ev->num == num)
	    return ev;
    return NULL;
}


/* history_prev_string():
 *	Find the previous event beginning with string
 */
private const HistEvent *
history_prev_string(h, str)
    History *h;
    const char* str;
{
    const HistEvent *ev;
    size_t len = strlen(str);

    for (ev = HCURR(h); ev != NULL; ev = HNEXT(h))
	if (strncmp(str, ev->str, len) == 0)
	    return ev;
    return NULL;
}




/* history_next_string():
 *	Find the next event beginning with string
 */
private const HistEvent *
history_next_string(h, str)
    History *h;
    const char* str;
{
    const HistEvent *ev;
    size_t len = strlen(str);

    for (ev = HCURR(h); ev != NULL; ev = HPREV(h))
	if (strncmp(str, ev->str, len) == 0)
	    return ev;
    return NULL;
}


/* history():
 *	User interface to history functions.
 */
const HistEvent *
#ifdef __STDC__
history(History *h, int fun, ...)
#else
history(va_alist)
    va_dcl
#endif
{
    va_list va;
    const HistEvent *ev = NULL;
    const char *str;
    static HistEvent sev = { 0, "" };

#ifdef __STDC__
    va_start(va, fun);
#else
    History *h; 
    int fun;
    va_start(va);
    h = va_arg(va, History *);
    fun = va_arg(va, int);
#endif

    switch (fun) {
    case H_ADD:
	str = va_arg(va, const char *);
	ev = HADD(h, str);
	break;

    case H_ENTER:
	str = va_arg(va, const char *);
	ev = HENTER(h, str);
	break;

    case H_FIRST:
	ev = HFIRST(h);
	break;

    case H_NEXT:
	ev = HNEXT(h);
	break;

    case H_LAST:
	ev = HLAST(h);
	break;

    case H_PREV:
	ev = HPREV(h);
	break;

    case H_CURR:
	ev = HCURR(h);
	break;

    case H_CLEAR:
	HCLEAR(h);
	break;

    case H_LOAD:
	sev.num = history_load(h, va_arg(va, const char *));
	ev = &sev;
	break;

    case H_SAVE:
	sev.num = history_save(h, va_arg(va, const char *));
	ev = &sev;
	break;

    case H_PREV_EVENT:
	ev = history_prev_event(h, va_arg(va, int));
	break;

    case H_NEXT_EVENT:
	ev = history_next_event(h, va_arg(va, int));
	break;

    case H_PREV_STR:
	ev = history_prev_string(h, va_arg(va, const char*));
	break;

    case H_NEXT_STR:
	ev = history_next_string(h, va_arg(va, const char*));
	break;

    case H_EVENT:
	if (history_set_num(h, va_arg(va, int)) == 0) {
	    sev.num = -1;
	    ev = &sev;
	}
	break;

    case H_FUNC:
	{
	    History hf;
	    hf.h_ref   = va_arg(va, ptr_t);
	    hf.h_first = va_arg(va, history_gfun_t);
	    hf.h_next  = va_arg(va, history_gfun_t);
	    hf.h_last  = va_arg(va, history_gfun_t);
	    hf.h_prev  = va_arg(va, history_gfun_t);
	    hf.h_curr  = va_arg(va, history_gfun_t);
	    hf.h_clear = va_arg(va, history_vfun_t);
	    hf.h_enter = va_arg(va, history_efun_t);
	    hf.h_add   = va_arg(va, history_efun_t);

	    if (history_set_fun(h, &hf) == 0) {
		sev.num = -1;
		ev = &sev;
	    }
	}
	break;

    case H_END:
	history_end(h);
	break;

    default:
	break;
    }
    va_end(va);
    return ev;
}
