/* $RCSfile: str.c,v $$Revision: 4.1 $$Date: 92/08/07 18:29:26 $
 *
 *    Copyright (c) 1991-2001, Larry Wall
 *
 *    You may distribute under the terms of either the GNU General Public
 *    License or the Artistic License, as specified in the README file.
 *
 * $Log:	str.c,v $
 */

#include "EXTERN.h"
#include "a2p.h"
#include "util.h"

void
str_numset(register STR *str, double num)
{
    str->str_nval = num;
    str->str_pok = 0;		/* invalidate pointer */
    str->str_nok = 1;		/* validate number */
}

char *
str_2ptr(register STR *str)
{
    register char *s;

    if (!str)
	return "";
    GROWSTR(&(str->str_ptr), &(str->str_len), 24);
    s = str->str_ptr;
    if (str->str_nok) {
	sprintf(s,"%.20g",str->str_nval);
	while (*s) s++;
    }
    *s = '\0';
    str->str_cur = s - str->str_ptr;
    str->str_pok = 1;
#ifdef DEBUGGING
    if (debug & 32)
	fprintf(stderr,"0x%lx ptr(%s)\n",(unsigned long)str,str->str_ptr);
#endif
    return str->str_ptr;
}

double
str_2num(register STR *str)
{
    if (!str)
	return 0.0;
    if (str->str_len && str->str_pok)
	str->str_nval = atof(str->str_ptr);
    else
	str->str_nval = 0.0;
    str->str_nok = 1;
#ifdef DEBUGGING
    if (debug & 32)
	fprintf(stderr,"0x%lx num(%g)\n",(unsigned long)str,str->str_nval);
#endif
    return str->str_nval;
}

void
str_sset(STR *dstr, register STR *sstr)
{
    if (!sstr)
	str_nset(dstr,No,0);
    else if (sstr->str_nok)
	str_numset(dstr,sstr->str_nval);
    else if (sstr->str_pok)
	str_nset(dstr,sstr->str_ptr,sstr->str_cur);
    else
	str_nset(dstr,"",0);
}

void
str_nset(register STR *str, register char *ptr, register int len)
{
    GROWSTR(&(str->str_ptr), &(str->str_len), len + 1);
    bcopy(ptr,str->str_ptr,len);
    str->str_cur = len;
    *(str->str_ptr+str->str_cur) = '\0';
    str->str_nok = 0;		/* invalidate number */
    str->str_pok = 1;		/* validate pointer */
}

void
str_set(register STR *str, register char *ptr)
{
    register int len;

    if (!ptr)
	ptr = "";
    len = strlen(ptr);
    GROWSTR(&(str->str_ptr), &(str->str_len), len + 1);
    bcopy(ptr,str->str_ptr,len+1);
    str->str_cur = len;
    str->str_nok = 0;		/* invalidate number */
    str->str_pok = 1;		/* validate pointer */
}

void
str_chop(register STR *str, register char *ptr)	/* like set but assuming ptr is in str */
                  
                   
{
    if (!(str->str_pok))
	str_2ptr(str);
    str->str_cur -= (ptr - str->str_ptr);
    bcopy(ptr,str->str_ptr, str->str_cur + 1);
    str->str_nok = 0;		/* invalidate number */
    str->str_pok = 1;		/* validate pointer */
}

void
str_ncat(register STR *str, register char *ptr, register int len)
{
    if (!(str->str_pok))
	str_2ptr(str);
    GROWSTR(&(str->str_ptr), &(str->str_len), str->str_cur + len + 1);
    bcopy(ptr,str->str_ptr+str->str_cur,len);
    str->str_cur += len;
    *(str->str_ptr+str->str_cur) = '\0';
    str->str_nok = 0;		/* invalidate number */
    str->str_pok = 1;		/* validate pointer */
}

void
str_scat(STR *dstr, register STR *sstr)
{
    if (!(sstr->str_pok))
	str_2ptr(sstr);
    if (sstr)
	str_ncat(dstr,sstr->str_ptr,sstr->str_cur);
}

void
str_cat(register STR *str, register char *ptr)
{
    register int len;

    if (!ptr)
	return;
    if (!(str->str_pok))
	str_2ptr(str);
    len = strlen(ptr);
    GROWSTR(&(str->str_ptr), &(str->str_len), str->str_cur + len + 1);
    bcopy(ptr,str->str_ptr+str->str_cur,len+1);
    str->str_cur += len;
    str->str_nok = 0;		/* invalidate number */
    str->str_pok = 1;		/* validate pointer */
}

char *
str_append_till(register STR *str, register char *from, register int delim, char *keeplist)
{
    register char *to;
    register int len;

    if (!from)
	return Nullch;
    len = strlen(from);
    GROWSTR(&(str->str_ptr), &(str->str_len), str->str_cur + len + 1);
    str->str_nok = 0;		/* invalidate number */
    str->str_pok = 1;		/* validate pointer */
    to = str->str_ptr+str->str_cur;
    for (; *from; from++,to++) {
	if (*from == '\\' && from[1] && delim != '\\') {
	    if (!keeplist) {
		if (from[1] == delim || from[1] == '\\')
		    from++;
		else
		    *to++ = *from++;
	    }
	    else if (strchr(keeplist,from[1]))
		*to++ = *from++;
	    else
		from++;
	}
	else if (*from == delim)
	    break;
	*to = *from;
    }
    *to = '\0';
    str->str_cur = to - str->str_ptr;
    return from;
}

STR *
str_new(int len)
{
    register STR *str;
    
    if (freestrroot) {
	str = freestrroot;
	freestrroot = str->str_link.str_next;
    }
    else {
	str = (STR *) safemalloc(sizeof(STR));
	bzero((char*)str,sizeof(STR));
    }
    if (len)
	GROWSTR(&(str->str_ptr), &(str->str_len), len + 1);
    return str;
}

void
str_grow(register STR *str, int len)
{
    if (len && str)
	GROWSTR(&(str->str_ptr), &(str->str_len), len + 1);
}

/* make str point to what nstr did */

void
str_replace(register STR *str, register STR *nstr)
{
    safefree(str->str_ptr);
    str->str_ptr = nstr->str_ptr;
    str->str_len = nstr->str_len;
    str->str_cur = nstr->str_cur;
    str->str_pok = nstr->str_pok;
    if (str->str_nok = nstr->str_nok)
	str->str_nval = nstr->str_nval;
    safefree((char*)nstr);
}

void
str_free(register STR *str)
{
    if (!str)
	return;
    if (str->str_len)
	str->str_ptr[0] = '\0';
    str->str_cur = 0;
    str->str_nok = 0;
    str->str_pok = 0;
    str->str_link.str_next = freestrroot;
    freestrroot = str;
}

int
str_len(register STR *str)
{
    if (!str)
	return 0;
    if (!(str->str_pok))
	str_2ptr(str);
    if (str->str_len)
	return str->str_cur;
    else
	return 0;
}

char *
str_gets(register STR *str, register FILE *fp)
{
#if defined(USE_STDIO_PTR) && defined(STDIO_PTR_LVALUE) && defined(STDIO_CNT_LVALUE)
    /* Here is some breathtakingly efficient cheating */

    register char *bp;		/* we're going to steal some values */
    register int cnt;		/*  from the stdio struct and put EVERYTHING */
    register STDCHAR *ptr;	/*   in the innermost loop into registers */
    register char newline = '\n';	/* (assuming at least 6 registers) */
    int i;
    int bpx;

#if defined(VMS)
    /* An ungetc()d char is handled separately from the regular
     * buffer, so we getc() it back out and stuff it in the buffer.
     */
    i = getc(fp);
    if (i == EOF) return Nullch;
    *(--((*fp)->_ptr)) = (unsigned char) i;
    (*fp)->_cnt++;
#endif

    cnt = FILE_cnt(fp);			/* get count into register */
    str->str_nok = 0;			/* invalidate number */
    str->str_pok = 1;			/* validate pointer */
    if (str->str_len <= cnt)		/* make sure we have the room */
	GROWSTR(&(str->str_ptr), &(str->str_len), cnt+1);
    bp = str->str_ptr;			/* move these two too to registers */
    ptr = FILE_ptr(fp);
    for (;;) {
	while (--cnt >= 0) {
	    if ((*bp++ = *ptr++) == newline)
		if (bp <= str->str_ptr || bp[-2] != '\\')
		    goto thats_all_folks;
		else {
		    line++;
		    bp -= 2;
		}
	}
	
	FILE_cnt(fp) = cnt;		/* deregisterize cnt and ptr */
	FILE_ptr(fp) = ptr;
	i = getc(fp);		/* get more characters */
	cnt = FILE_cnt(fp);
	ptr = FILE_ptr(fp);		/* reregisterize cnt and ptr */

	bpx = bp - str->str_ptr;	/* prepare for possible relocation */
	GROWSTR(&(str->str_ptr), &(str->str_len), str->str_cur + cnt + 1);
	bp = str->str_ptr + bpx;	/* reconstitute our pointer */

	if (i == newline) {		/* all done for now? */
	    *bp++ = i;
	    goto thats_all_folks;
	}
	else if (i == EOF)		/* all done for ever? */
	    goto thats_all_folks;
	*bp++ = i;			/* now go back to screaming loop */
    }

thats_all_folks:
    FILE_cnt(fp) = cnt;			/* put these back or we're in trouble */
    FILE_ptr(fp) = ptr;
    *bp = '\0';
    str->str_cur = bp - str->str_ptr;	/* set length */

#else /* USE_STDIO_PTR && STDIO_PTR_LVALUE && STDIO_CNT_LVALUE */
    /* The big, slow, and stupid way */

    static char buf[4192];

    if (fgets(buf, sizeof buf, fp) != Nullch)
	str_set(str, buf);
    else
	str_set(str, No);

#endif /* USE_STDIO_PTR && STDIO_PTR_LVALUE && STDIO_CNT_LVALUE */

    return str->str_cur ? str->str_ptr : Nullch;
}

void
str_inc(register STR *str)
{
    register char *d;

    if (!str)
	return;
    if (str->str_nok) {
	str->str_nval += 1.0;
	str->str_pok = 0;
	return;
    }
    if (!str->str_pok) {
	str->str_nval = 1.0;
	str->str_nok = 1;
	return;
    }
    for (d = str->str_ptr; *d && *d != '.'; d++) ;
    d--;
    if (!isdigit(*str->str_ptr) || !isdigit(*d) ) {
        str_numset(str,atof(str->str_ptr) + 1.0);  /* punt */
	return;
    }
    while (d >= str->str_ptr) {
	if (++*d <= '9')
	    return;
	*(d--) = '0';
    }
    /* oh,oh, the number grew */
    GROWSTR(&(str->str_ptr), &(str->str_len), str->str_cur + 2);
    str->str_cur++;
    for (d = str->str_ptr + str->str_cur; d > str->str_ptr; d--)
	*d = d[-1];
    *d = '1';
}

void
str_dec(register STR *str)
{
    register char *d;

    if (!str)
	return;
    if (str->str_nok) {
	str->str_nval -= 1.0;
	str->str_pok = 0;
	return;
    }
    if (!str->str_pok) {
	str->str_nval = -1.0;
	str->str_nok = 1;
	return;
    }
    for (d = str->str_ptr; *d && *d != '.'; d++) ;
    d--;
    if (!isdigit(*str->str_ptr) || !isdigit(*d) || (*d == '0' && d == str->str_ptr)) {
        str_numset(str,atof(str->str_ptr) - 1.0);  /* punt */
	return;
    }
    while (d >= str->str_ptr) {
	if (--*d >= '0')
	    return;
	*(d--) = '9';
    }
}

/* make a string that will exist for the duration of the expression eval */

STR *
str_mortal(STR *oldstr)
{
    register STR *str = str_new(0);
    static long tmps_size = -1;

    str_sset(str,oldstr);
    if (++tmps_max > tmps_size) {
	tmps_size = tmps_max;
	if (!(tmps_size & 127)) {
	    if (tmps_size)
		tmps_list = (STR**)saferealloc((char*)tmps_list,
		    (tmps_size + 128) * sizeof(STR*) );
	    else
		tmps_list = (STR**)safemalloc(128 * sizeof(char*));
	}
    }
    tmps_list[tmps_max] = str;
    return str;
}

STR *
str_make(char *s)
{
    register STR *str = str_new(0);

    str_set(str,s);
    return str;
}

STR *
str_nmake(double n)
{
    register STR *str = str_new(0);

    str_numset(str,n);
    return str;
}
