/* $RCSfile: str.h,v $$Revision: 4.1 $$Date: 92/08/07 18:29:27 $
 *
 *    Copyright (c) 1991-1997, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 * $Log:	str.h,v $
 */

struct string {
    char *	str_ptr;	/* pointer to malloced string */
    double	str_nval;	/* numeric value, if any */
    int		str_len;	/* allocated size */
    int		str_cur;	/* length of str_ptr as a C string */
    union {
	STR *str_next;		/* while free, link to next free str */
    } str_link;
    char	str_pok;	/* state of str_ptr */
    char	str_nok;	/* state of str_nval */
};

#define Nullstr Null(STR*)

/* the following macro updates any magic values this str is associated with */

#define STABSET(x) (x->str_link.str_magic && stabset(x->str_link.str_magic,x))

EXT STR **tmps_list;
EXT long tmps_max INIT(-1);

double str_2num _(( STR *str ));
char * str_2ptr _(( STR *str ));
char * str_append_till _(( STR *str, char *from, int delim, char *keeplist ));
void str_cat _(( STR *str, char *ptr ));
void str_chop _(( STR *str, char *ptr ));
void str_dec _(( STR *str ));
void str_free _(( STR *str ));
char * str_gets _(( STR *str, FILE *fp ));
void str_grow _(( STR *str, int len ));
void str_inc _(( STR *str ));
int str_len _(( STR *str ));
STR * str_make _(( char *s ));
STR * str_mortal _(( STR *oldstr ));
void str_ncat _(( STR *str, char *ptr, int len ));
STR * str_new _(( int len ));
STR * str_nmake _(( double n ));
void str_nset _(( STR *str, char *ptr, int len ));
void str_numset _(( STR *str, double num ));
void str_replace _(( STR *str, STR *nstr ));
void str_scat _(( STR *dstr, STR *sstr ));
void str_set _(( STR *str, char *ptr ));
void str_sset _(( STR *dstr, STR *sstr ));
