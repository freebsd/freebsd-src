/* $RCSfile: hash.c,v $$Revision: 4.1 $$Date: 92/08/07 18:29:20 $
 *
 *    Copyright (c) 1991-2001, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 * $Log:	hash.c,v $
 */

#include <stdio.h>
#include "EXTERN.h"
#include "a2p.h"
#include "util.h"

STR *
hfetch(register HASH *tb, char *key)
{
    register char *s;
    register int i;
    register int hash;
    register HENT *entry;

    if (!tb)
	return Nullstr;
    for (s=key,		i=0,	hash = 0;
      /* while */ *s;
	 s++,		i++,	hash *= 5) {
	hash += *s * coeff[i];
    }
    entry = tb->tbl_array[hash & tb->tbl_max];
    for (; entry; entry = entry->hent_next) {
	if (entry->hent_hash != hash)		/* strings can't be equal */
	    continue;
	if (strNE(entry->hent_key,key))	/* is this it? */
	    continue;
	return entry->hent_val;
    }
    return Nullstr;
}

bool
hstore(register HASH *tb, char *key, STR *val)
{
    register char *s;
    register int i;
    register int hash;
    register HENT *entry;
    register HENT **oentry;

    if (!tb)
	return FALSE;
    for (s=key,		i=0,	hash = 0;
      /* while */ *s;
	 s++,		i++,	hash *= 5) {
	hash += *s * coeff[i];
    }

    oentry = &(tb->tbl_array[hash & tb->tbl_max]);
    i = 1;

    for (entry = *oentry; entry; i=0, entry = entry->hent_next) {
	if (entry->hent_hash != hash)		/* strings can't be equal */
	    continue;
	if (strNE(entry->hent_key,key))	/* is this it? */
	    continue;
	/*NOSTRICT*/
	safefree(entry->hent_val);
	entry->hent_val = val;
	return TRUE;
    }
    /*NOSTRICT*/
    entry = (HENT*) safemalloc(sizeof(HENT));

    entry->hent_key = savestr(key);
    entry->hent_val = val;
    entry->hent_hash = hash;
    entry->hent_next = *oentry;
    *oentry = entry;

    if (i) {				/* initial entry? */
	tb->tbl_fill++;
	if ((tb->tbl_fill * 100 / (tb->tbl_max + 1)) > FILLPCT)
	    hsplit(tb);
    }

    return FALSE;
}

#ifdef NOTUSED
bool
hdelete(register HASH *tb, char *key)
{
    register char *s;
    register int i;
    register int hash;
    register HENT *entry;
    register HENT **oentry;

    if (!tb)
	return FALSE;
    for (s=key,		i=0,	hash = 0;
      /* while */ *s;
	 s++,		i++,	hash *= 5) {
	hash += *s * coeff[i];
    }

    oentry = &(tb->tbl_array[hash & tb->tbl_max]);
    entry = *oentry;
    i = 1;
    for (; entry; i=0, oentry = &entry->hent_next, entry = entry->hent_next) {
	if (entry->hent_hash != hash)		/* strings can't be equal */
	    continue;
	if (strNE(entry->hent_key,key))	/* is this it? */
	    continue;
	safefree((char*)entry->hent_val);
	safefree(entry->hent_key);
	*oentry = entry->hent_next;
	safefree((char*)entry);
	if (i)
	    tb->tbl_fill--;
	return TRUE;
    }
    return FALSE;
}
#endif

void
hsplit(HASH *tb)
{
    int oldsize = tb->tbl_max + 1;
    register int newsize = oldsize * 2;
    register int i;
    register HENT **a;
    register HENT **b;
    register HENT *entry;
    register HENT **oentry;

    a = (HENT**) saferealloc((char*)tb->tbl_array, newsize * sizeof(HENT*));
    bzero((char*)&a[oldsize], oldsize * sizeof(HENT*)); /* zero second half */
    tb->tbl_max = --newsize;
    tb->tbl_array = a;

    for (i=0; i<oldsize; i++,a++) {
	if (!*a)				/* non-existent */
	    continue;
	b = a+oldsize;
	for (oentry = a, entry = *a; entry; entry = *oentry) {
	    if ((entry->hent_hash & newsize) != i) {
		*oentry = entry->hent_next;
		entry->hent_next = *b;
		if (!*b)
		    tb->tbl_fill++;
		*b = entry;
		continue;
	    }
	    else
		oentry = &entry->hent_next;
	}
	if (!*a)				/* everything moved */
	    tb->tbl_fill--;
    }
}

HASH *
hnew(void)
{
    register HASH *tb = (HASH*)safemalloc(sizeof(HASH));

    tb->tbl_array = (HENT**) safemalloc(8 * sizeof(HENT*));
    tb->tbl_fill = 0;
    tb->tbl_max = 7;
    hiterinit(tb);	/* so each() will start off right */
    bzero((char*)tb->tbl_array, 8 * sizeof(HENT*));
    return tb;
}

#ifdef NOTUSED
hshow(register HASH *tb)
{
    fprintf(stderr,"%5d %4d (%2d%%)\n",
	tb->tbl_max+1,
	tb->tbl_fill,
	tb->tbl_fill * 100 / (tb->tbl_max+1));
}
#endif

int
hiterinit(register HASH *tb)
{
    tb->tbl_riter = -1;
    tb->tbl_eiter = Null(HENT*);
    return tb->tbl_fill;
}

HENT *
hiternext(register HASH *tb)
{
    register HENT *entry;

    entry = tb->tbl_eiter;
    do {
	if (entry)
	    entry = entry->hent_next;
	if (!entry) {
	    tb->tbl_riter++;
	    if (tb->tbl_riter > tb->tbl_max) {
		tb->tbl_riter = -1;
		break;
	    }
	    entry = tb->tbl_array[tb->tbl_riter];
	}
    } while (!entry);

    tb->tbl_eiter = entry;
    return entry;
}

char *
hiterkey(register HENT *entry)
{
    return entry->hent_key;
}

STR *
hiterval(register HENT *entry)
{
    return entry->hent_val;
}
