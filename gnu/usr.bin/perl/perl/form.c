/* $RCSfile: form.c,v $$Revision: 1.1.1.1 $$Date: 1994/09/10 06:27:33 $
 *
 *    Copyright (c) 1991, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 * $Log: form.c,v $
 * Revision 1.1.1.1  1994/09/10  06:27:33  gclarkii
 * Initial import of Perl 4.046 bmaked
 *
 * Revision 1.1.1.1  1993/08/23  21:29:36  nate
 * PERL!
 *
 * Revision 4.0.1.4  1993/02/05  19:34:32  lwall
 * patch36: formats now ignore literal text for ~~ loop determination
 *
 * Revision 4.0.1.3  92/06/08  13:21:42  lwall
 * patch20: removed implicit int declarations on funcions
 * patch20: form feed for formats is now specifiable via $^L
 * patch20: Perl now distinguishes overlapped copies from non-overlapped
 *
 * Revision 4.0.1.2  91/11/05  17:18:43  lwall
 * patch11: formats didn't fill their fields as well as they could
 * patch11: ^ fields chopped hyphens on line break
 * patch11: # fields could write outside allocated memory
 *
 * Revision 4.0.1.1  91/06/07  11:07:59  lwall
 * patch4: new copyright notice
 * patch4: default top-of-form format is now FILEHANDLE_TOP
 *
 * Revision 4.0  91/03/20  01:19:23  lwall
 * 4.0 baseline.
 *
 */

#include "EXTERN.h"
#include "perl.h"

/* Forms stuff */

static int countlines();

void
form_parseargs(fcmd)
register FCMD *fcmd;
{
    register int i;
    register ARG *arg;
    register int items;
    STR *str;
    ARG *parselist();
    line_t oldline = curcmd->c_line;
    int oldsave = savestack->ary_fill;

    str = fcmd->f_unparsed;
    curcmd->c_line = fcmd->f_line;
    fcmd->f_unparsed = Nullstr;
    (void)savehptr(&curstash);
    curstash = str->str_u.str_hash;
    arg = parselist(str);
    restorelist(oldsave);

    items = arg->arg_len - 1;	/* ignore $$ on end */
    for (i = 1; i <= items; i++) {
	if (!fcmd || fcmd->f_type == F_NULL)
	    fatal("Too many field values");
	dehoist(arg,i);
	fcmd->f_expr = make_op(O_ITEM,1,
	  arg[i].arg_ptr.arg_arg,Nullarg,Nullarg);
	if (fcmd->f_flags & FC_CHOP) {
	    if ((fcmd->f_expr[1].arg_type & A_MASK) == A_STAB)
		fcmd->f_expr[1].arg_type = A_LVAL;
	    else if ((fcmd->f_expr[1].arg_type & A_MASK) == A_EXPR)
		fcmd->f_expr[1].arg_type = A_LEXPR;
	    else
		fatal("^ field requires scalar lvalue");
	}
	fcmd = fcmd->f_next;
    }
    if (fcmd && fcmd->f_type)
	fatal("Not enough field values");
    curcmd->c_line = oldline;
    Safefree(arg);
    str_free(str);
}

int newsize;

#define CHKLEN(allow) \
newsize = (d - orec->o_str) + (allow); \
if (newsize >= curlen) { \
    curlen = d - orec->o_str; \
    GROWSTR(&orec->o_str,&orec->o_len,orec->o_len + (allow)); \
    d = orec->o_str + curlen;	/* in case it moves */ \
    curlen = orec->o_len - 2; \
}

void
format(orec,fcmd,sp)
register struct outrec *orec;
register FCMD *fcmd;
int sp;
{
    register char *d = orec->o_str;
    register char *s;
    register int curlen = orec->o_len - 2;
    register int size;
    FCMD *nextfcmd;
    FCMD *linebeg = fcmd;
    char tmpchar;
    char *t;
    CMD mycmd;
    STR *str;
    char *chophere;
    int blank = TRUE;

    mycmd.c_type = C_NULL;
    orec->o_lines = 0;
    for (; fcmd; fcmd = nextfcmd) {
	nextfcmd = fcmd->f_next;
	CHKLEN(fcmd->f_presize);
	/*SUPPRESS 560*/
	if (s = fcmd->f_pre) {
	    while (*s) {
		if (*s == '\n') {
		    t = orec->o_str;
		    if (blank && (fcmd->f_flags & FC_REPEAT)) {
			while (d > t && (d[-1] != '\n'))
			    d--;
		    }
		    else {
			while (d > t && (d[-1] == ' ' || d[-1] == '\t'))
			    d--;
		    }
		    if (fcmd->f_flags & FC_NOBLANK) {
			if (blank || d == orec->o_str || d[-1] == '\n') {
			    orec->o_lines--;	/* don't print blank line */
			    linebeg = fcmd->f_next;
			    break;
			}
			else if (fcmd->f_flags & FC_REPEAT)
			    nextfcmd = linebeg;
			else
			    linebeg = fcmd->f_next;
		    }
		    else
			linebeg = fcmd->f_next;
		    blank = TRUE;
		}
		*d++ = *s++;
	    }
	}
	if (fcmd->f_unparsed)
	    form_parseargs(fcmd);
	switch (fcmd->f_type) {
	case F_NULL:
	    orec->o_lines++;
	    break;
	case F_LEFT:
	    (void)eval(fcmd->f_expr,G_SCALAR,sp);
	    str = stack->ary_array[sp+1];
	    s = str_get(str);
	    size = fcmd->f_size;
	    CHKLEN(size);
	    chophere = Nullch;
	    while (size && *s && *s != '\n') {
		if (*s == '\t')
		    *s = ' ';
		else if (*s != ' ')
		    blank = FALSE;
		size--;
		if (*s && index(chopset,(*d++ = *s++)))
		    chophere = s;
		if (*s == '\n' && (fcmd->f_flags & FC_CHOP))
		    *s = ' ';
	    }
	    if (size || !*s)
		chophere = s;
	    else if (chophere && chophere < s && *s && index(chopset,*s))
		chophere = s;
	    if (fcmd->f_flags & FC_CHOP) {
		if (!chophere)
		    chophere = s;
		size += (s - chophere);
		d -= (s - chophere);
		if (fcmd->f_flags & FC_MORE &&
		  *chophere && strNE(chophere,"\n")) {
		    while (size < 3) {
			d--;
			size++;
		    }
		    while (d[-1] == ' ' && size < fcmd->f_size) {
			d--;
			size++;
		    }
		    *d++ = '.';
		    *d++ = '.';
		    *d++ = '.';
		    size -= 3;
		}
		while (*chophere && index(chopset,*chophere)
		  && isSPACE(*chophere))
		    chophere++;
		str_chop(str,chophere);
	    }
	    if (fcmd->f_next && fcmd->f_next->f_pre[0] == '\n')
		size = 0;			/* no spaces before newline */
	    while (size) {
		size--;
		*d++ = ' ';
	    }
	    break;
	case F_RIGHT:
	    (void)eval(fcmd->f_expr,G_SCALAR,sp);
	    str = stack->ary_array[sp+1];
	    t = s = str_get(str);
	    size = fcmd->f_size;
	    CHKLEN(size);
	    chophere = Nullch;
	    while (size && *s && *s != '\n') {
		if (*s == '\t')
		    *s = ' ';
		else if (*s != ' ')
		    blank = FALSE;
		size--;
		if (*s && index(chopset,*s++))
		    chophere = s;
		if (*s == '\n' && (fcmd->f_flags & FC_CHOP))
		    *s = ' ';
	    }
	    if (size || !*s)
		chophere = s;
	    else if (chophere && chophere < s && *s && index(chopset,*s))
		chophere = s;
	    if (fcmd->f_flags & FC_CHOP) {
		if (!chophere)
		    chophere = s;
		size += (s - chophere);
		s = chophere;
		while (*chophere && index(chopset,*chophere)
		  && isSPACE(*chophere))
		    chophere++;
	    }
	    tmpchar = *s;
	    *s = '\0';
	    while (size) {
		size--;
		*d++ = ' ';
	    }
	    size = s - t;
	    Copy(t,d,size,char);
	    d += size;
	    *s = tmpchar;
	    if (fcmd->f_flags & FC_CHOP)
		str_chop(str,chophere);
	    break;
	case F_CENTER: {
	    int halfsize;

	    (void)eval(fcmd->f_expr,G_SCALAR,sp);
	    str = stack->ary_array[sp+1];
	    t = s = str_get(str);
	    size = fcmd->f_size;
	    CHKLEN(size);
	    chophere = Nullch;
	    while (size && *s && *s != '\n') {
		if (*s == '\t')
		    *s = ' ';
		else if (*s != ' ')
		    blank = FALSE;
		size--;
		if (*s && index(chopset,*s++))
		    chophere = s;
		if (*s == '\n' && (fcmd->f_flags & FC_CHOP))
		    *s = ' ';
	    }
	    if (size || !*s)
		chophere = s;
	    else if (chophere && chophere < s && *s && index(chopset,*s))
		chophere = s;
	    if (fcmd->f_flags & FC_CHOP) {
		if (!chophere)
		    chophere = s;
		size += (s - chophere);
		s = chophere;
		while (*chophere && index(chopset,*chophere)
		  && isSPACE(*chophere))
		    chophere++;
	    }
	    tmpchar = *s;
	    *s = '\0';
	    halfsize = size / 2;
	    while (size > halfsize) {
		size--;
		*d++ = ' ';
	    }
	    size = s - t;
	    Copy(t,d,size,char);
	    d += size;
	    *s = tmpchar;
	    if (fcmd->f_next && fcmd->f_next->f_pre[0] == '\n')
		size = 0;			/* no spaces before newline */
	    else
		size = halfsize;
	    while (size) {
		size--;
		*d++ = ' ';
	    }
	    if (fcmd->f_flags & FC_CHOP)
		str_chop(str,chophere);
	    break;
	}
	case F_LINES:
	    (void)eval(fcmd->f_expr,G_SCALAR,sp);
	    str = stack->ary_array[sp+1];
	    s = str_get(str);
	    size = str_len(str);
	    CHKLEN(size+1);
	    orec->o_lines += countlines(s,size) - 1;
	    Copy(s,d,size,char);
	    d += size;
	    if (size && s[size-1] != '\n') {
		*d++ = '\n';
		orec->o_lines++;
	    }
	    linebeg = fcmd->f_next;
	    break;
	case F_DECIMAL: {
	    double value;

	    (void)eval(fcmd->f_expr,G_SCALAR,sp);
	    str = stack->ary_array[sp+1];
	    size = fcmd->f_size;
	    CHKLEN(size+1);
	    /* If the field is marked with ^ and the value is undefined,
	       blank it out. */
	    if ((fcmd->f_flags & FC_CHOP) && !str->str_pok && !str->str_nok) {
		while (size) {
		    size--;
		    *d++ = ' ';
		}
		break;
	    }
	    blank = FALSE;
	    value = str_gnum(str);
	    if (fcmd->f_flags & FC_DP) {
		sprintf(d, "%#*.*f", size, fcmd->f_decimals, value);
	    } else {
		sprintf(d, "%*.0f", size, value);
	    }
	    d += size;
	    break;
	}
	}
    }
    CHKLEN(1);
    *d++ = '\0';
}

static int
countlines(s,size)
register char *s;
register int size;
{
    register int count = 0;

    while (size--) {
	if (*s++ == '\n')
	    count++;
    }
    return count;
}

void
do_write(orec,stab,sp)
struct outrec *orec;
STAB *stab;
int sp;
{
    register STIO *stio = stab_io(stab);
    FILE *ofp = stio->ofp;

#ifdef DEBUGGING
    if (debug & 256)
	fprintf(stderr,"left=%ld, todo=%ld\n",
	  (long)stio->lines_left, (long)orec->o_lines);
#endif
    if (stio->lines_left < orec->o_lines) {
	if (!stio->top_stab) {
	    STAB *topstab;
	    char tmpbuf[256];

	    if (!stio->top_name) {
		if (!stio->fmt_name)
		    stio->fmt_name = savestr(stab_name(stab));
		sprintf(tmpbuf, "%s_TOP", stio->fmt_name);
		topstab = stabent(tmpbuf,FALSE);
		if (topstab && stab_form(topstab))
		    stio->top_name = savestr(tmpbuf);
		else
		    stio->top_name = savestr("top");
	    }
	    topstab = stabent(stio->top_name,FALSE);
	    if (!topstab || !stab_form(topstab)) {
		stio->lines_left = 100000000;
		goto forget_top;
	    }
	    stio->top_stab = topstab;
	}
	if (stio->lines_left >= 0 && stio->page > 0)
	    fwrite(formfeed->str_ptr, formfeed->str_cur, 1, ofp);
	stio->lines_left = stio->page_len;
	stio->page++;
	format(&toprec,stab_form(stio->top_stab),sp);
	fputs(toprec.o_str,ofp);
	stio->lines_left -= toprec.o_lines;
    }
  forget_top:
    fputs(orec->o_str,ofp);
    stio->lines_left -= orec->o_lines;
}
