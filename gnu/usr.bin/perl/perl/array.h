/* $RCSfile: array.h,v $$Revision: 1.1.1.1 $$Date: 1993/08/23 21:29:34 $
 *
 *    Copyright (c) 1991, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 * $Log: array.h,v $
 * Revision 1.1.1.1  1993/08/23  21:29:34  nate
 * PERL!
 *
 * Revision 4.0.1.2  92/06/08  11:45:57  lwall
 * patch20: removed implicit int declarations on funcions
 * 
 * Revision 4.0.1.1  91/06/07  10:19:20  lwall
 * patch4: new copyright notice
 * 
 * Revision 4.0  91/03/20  01:03:44  lwall
 * 4.0 baseline.
 * 
 */

struct atbl {
    STR	**ary_array;
    STR **ary_alloc;
    STR *ary_magic;
    int ary_max;
    int ary_fill;
    char ary_flags;
};

#define ARF_REAL 1	/* free old entries */

STR *afetch();
bool astore();
STR *apop();
STR *ashift();
void afree();
void aclear();
bool apush();
int alen();
ARRAY *anew();
ARRAY *afake();
void aunshift();
void afill();
