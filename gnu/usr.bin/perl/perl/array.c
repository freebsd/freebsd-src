/* $RCSfile: array.c,v $$Revision: 1.1.1.1 $$Date: 1994/09/10 06:27:31 $
 *
 *    Copyright (c) 1991, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 * $Log: array.c,v $
 * Revision 1.1.1.1  1994/09/10  06:27:31  gclarkii
 * Initial import of Perl 4.046 bmaked
 *
 * Revision 1.1.1.1  1993/08/23  21:29:34  nate
 * PERL!
 *
 * Revision 4.0.1.3  92/06/08  11:45:05  lwall
 * patch20: Perl now distinguishes overlapped copies from non-overlapped
 *
 * Revision 4.0.1.2  91/11/05  16:00:14  lwall
 * patch11: random cleanup
 * patch11: passing non-existend array elements to subrouting caused core dump
 *
 * Revision 4.0.1.1  91/06/07  10:19:08  lwall
 * patch4: new copyright notice
 *
 * Revision 4.0  91/03/20  01:03:32  lwall
 * 4.0 baseline.
 *
 */

#include "EXTERN.h"
#include "perl.h"

STR *
afetch(ar,key,lval)
register ARRAY *ar;
int key;
int lval;
{
    STR *str;

    if (key < 0 || key > ar->ary_fill) {
	if (lval && key >= 0) {
	    if (ar->ary_flags & ARF_REAL)
		str = Str_new(5,0);
	    else
		str = str_mortal(&str_undef);
	    (void)astore(ar,key,str);
	    return str;
	}
	else
	    return &str_undef;
    }
    if (!ar->ary_array[key]) {
	if (lval) {
	    str = Str_new(6,0);
	    (void)astore(ar,key,str);
	    return str;
	}
	return &str_undef;
    }
    return ar->ary_array[key];
}

bool
astore(ar,key,val)
register ARRAY *ar;
int key;
STR *val;
{
    int retval;

    if (key < 0)
	return FALSE;
    if (key > ar->ary_max) {
	int newmax;

	if (ar->ary_alloc != ar->ary_array) {
	    retval = ar->ary_array - ar->ary_alloc;
	    Move(ar->ary_array, ar->ary_alloc, ar->ary_max+1, STR*);
	    Zero(ar->ary_alloc+ar->ary_max+1, retval, STR*);
	    ar->ary_max += retval;
	    ar->ary_array -= retval;
	    if (key > ar->ary_max - 10) {
		newmax = key + ar->ary_max;
		goto resize;
	    }
	}
	else {
	    if (ar->ary_alloc) {
		newmax = key + ar->ary_max / 5;
	      resize:
		Renew(ar->ary_alloc,newmax+1, STR*);
		Zero(&ar->ary_alloc[ar->ary_max+1], newmax - ar->ary_max, STR*);
	    }
	    else {
		newmax = key < 4 ? 4 : key;
		Newz(2,ar->ary_alloc, newmax+1, STR*);
	    }
	    ar->ary_array = ar->ary_alloc;
	    ar->ary_max = newmax;
	}
    }
    if (ar->ary_flags & ARF_REAL) {
	if (ar->ary_fill < key) {
	    while (++ar->ary_fill < key) {
		if (ar->ary_array[ar->ary_fill] != Nullstr) {
		    str_free(ar->ary_array[ar->ary_fill]);
		    ar->ary_array[ar->ary_fill] = Nullstr;
		}
	    }
	}
	retval = (ar->ary_array[key] != Nullstr);
	if (retval)
	    str_free(ar->ary_array[key]);
    }
    else
	retval = 0;
    ar->ary_array[key] = val;
    return retval;
}

ARRAY *
anew(stab)
STAB *stab;
{
    register ARRAY *ar;

    New(1,ar,1,ARRAY);
    ar->ary_magic = Str_new(7,0);
    ar->ary_alloc = ar->ary_array = 0;
    str_magic(ar->ary_magic, stab, '#', Nullch, 0);
    ar->ary_max = ar->ary_fill = -1;
    ar->ary_flags = ARF_REAL;
    return ar;
}

ARRAY *
afake(stab,size,strp)
STAB *stab;
register int size;
register STR **strp;
{
    register ARRAY *ar;

    New(3,ar,1,ARRAY);
    New(4,ar->ary_alloc,size+1,STR*);
    Copy(strp,ar->ary_alloc,size,STR*);
    ar->ary_array = ar->ary_alloc;
    ar->ary_magic = Str_new(8,0);
    str_magic(ar->ary_magic, stab, '#', Nullch, 0);
    ar->ary_fill = size - 1;
    ar->ary_max = size - 1;
    ar->ary_flags = 0;
    while (size--) {
	if (*strp)
	    (*strp)->str_pok &= ~SP_TEMP;
	strp++;
    }
    return ar;
}

void
aclear(ar)
register ARRAY *ar;
{
    register int key;

    if (!ar || !(ar->ary_flags & ARF_REAL) || ar->ary_max < 0)
	return;
    /*SUPPRESS 560*/
    if (key = ar->ary_array - ar->ary_alloc) {
	ar->ary_max += key;
	ar->ary_array -= key;
    }
    for (key = 0; key <= ar->ary_max; key++)
	str_free(ar->ary_array[key]);
    ar->ary_fill = -1;
    Zero(ar->ary_array, ar->ary_max+1, STR*);
}

void
afree(ar)
register ARRAY *ar;
{
    register int key;

    if (!ar)
	return;
    /*SUPPRESS 560*/
    if (key = ar->ary_array - ar->ary_alloc) {
	ar->ary_max += key;
	ar->ary_array -= key;
    }
    if (ar->ary_flags & ARF_REAL) {
	for (key = 0; key <= ar->ary_max; key++)
	    str_free(ar->ary_array[key]);
    }
    str_free(ar->ary_magic);
    Safefree(ar->ary_alloc);
    Safefree(ar);
}

bool
apush(ar,val)
register ARRAY *ar;
STR *val;
{
    return astore(ar,++(ar->ary_fill),val);
}

STR *
apop(ar)
register ARRAY *ar;
{
    STR *retval;

    if (ar->ary_fill < 0)
	return Nullstr;
    retval = ar->ary_array[ar->ary_fill];
    ar->ary_array[ar->ary_fill--] = Nullstr;
    return retval;
}

void
aunshift(ar,num)
register ARRAY *ar;
register int num;
{
    register int i;
    register STR **sstr,**dstr;

    if (num <= 0)
	return;
    if (ar->ary_array - ar->ary_alloc >= num) {
	ar->ary_max += num;
	ar->ary_fill += num;
	while (num--)
	    *--ar->ary_array = Nullstr;
    }
    else {
	(void)astore(ar,ar->ary_fill+num,(STR*)0);	/* maybe extend array */
	dstr = ar->ary_array + ar->ary_fill;
	sstr = dstr - num;
#ifdef BUGGY_MSC5
 # pragma loop_opt(off)	/* don't loop-optimize the following code */
#endif /* BUGGY_MSC5 */
	for (i = ar->ary_fill - num; i >= 0; i--) {
	    *dstr-- = *sstr--;
#ifdef BUGGY_MSC5
 # pragma loop_opt()	/* loop-optimization back to command-line setting */
#endif /* BUGGY_MSC5 */
	}
	Zero(ar->ary_array, num, STR*);
    }
}

STR *
ashift(ar)
register ARRAY *ar;
{
    STR *retval;

    if (ar->ary_fill < 0)
	return Nullstr;
    retval = *ar->ary_array;
    *(ar->ary_array++) = Nullstr;
    ar->ary_max--;
    ar->ary_fill--;
    return retval;
}

int
alen(ar)
register ARRAY *ar;
{
    return ar->ary_fill;
}

void
afill(ar, fill)
register ARRAY *ar;
int fill;
{
    if (fill < 0)
	fill = -1;
    if (fill <= ar->ary_max)
	ar->ary_fill = fill;
    else
	(void)astore(ar,fill,Nullstr);
}
