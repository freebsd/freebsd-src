/* $Header: /src/pub/tcsh/tw.color.c,v 1.8 2001/03/18 19:06:32 christos Exp $ */
/*
 * tw.color.c: builtin color ls-F
 */
/*-
 * Copyright (c) 1998 The Regents of the University of California.
 * All rights reserved.
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
#include "sh.h"

RCSID("$Id: tw.color.c,v 1.8 2001/03/18 19:06:32 christos Exp $")

#include "tw.h"
#include "ed.h"
#include "tc.h"

#ifdef COLOR_LS_F

typedef struct {
    char   *s;
    int     len;
} Str;


#define VAR(suffix,variable,defaultcolor) \
{ \
    suffix, variable, { defaultcolor, sizeof(defaultcolor) - 1 }, \
      { defaultcolor, sizeof(defaultcolor) - 1 } \
}
#define NOS '\0' /* no suffix */

typedef struct {
    const char suffix;
    const char *variable;
    Str     color;
    Str     defaultcolor;
} Variable;

static Variable variables[] = {
    VAR('/', "di", "01;34"),	/* Directory */
    VAR('@', "ln", "01;36"),	/* Symbolic link */
    VAR('&', "or", ""),		/* Orphanned symbolic link (defaults to ln) */
    VAR('|', "pi", "33"),	/* Named pipe (FIFO) */
    VAR('=', "so", "01;35"),	/* Socket */
    VAR('>', "do", "01;35"),	/* Door (solaris fast ipc mechanism)  */
    VAR('#', "bd", "01;33"),	/* Block device */
    VAR('%', "cd", "01;33"),	/* Character device */
    VAR('*', "ex", "01;32"),	/* Executable file */
    VAR(NOS, "fi", "0"),	/* Regular file */
    VAR(NOS, "no", "0"),	/* Normal (non-filename) text */
    VAR(NOS, "mi", ""),		/* Missing file (defaults to fi) */
#ifdef IS_ASCII
    VAR(NOS, "lc", "\033["),	/* Left code (ASCII) */
#else
    VAR(NOS, "lc", "\x27["),	/* Left code (EBCDIC)*/
#endif
    VAR(NOS, "rc", "m"),	/* Right code */
    VAR(NOS, "ec", ""),		/* End code (replaces lc+no+rc) */
};

enum FileType {
    VDir, VSym, VOrph, VPipe, VSock, VDoor, VBlock, VChr, VExe,
    VFile, VNormal, VMiss, VLeft, VRight, VEnd
};

#define nvariables (sizeof(variables)/sizeof(variables[0]))

typedef struct {
    Str     extension;	/* file extension */
    Str     color;	/* color string */
} Extension;

static Extension *extensions = NULL;
static int nextensions = 0;

static char *colors = NULL;
bool	     color_context_ls = FALSE;	/* do colored ls */
static bool  color_context_lsmF = FALSE;	/* do colored ls-F */

static bool getstring __P((char **, const Char **, Str *, int));
static void put_color __P((Str *));
static void print_color __P((Char *, size_t, int));

/* set_color_context():
 */
void
set_color_context()
{
    struct varent *vp = adrof(STRcolor);

    if (!vp) {
	color_context_ls = FALSE;
	color_context_lsmF = FALSE;
    }
    else if (!vp->vec[0] || vp->vec[0][0] == '\0') {
	color_context_ls = TRUE;
	color_context_lsmF = TRUE;
    }
    else {
	size_t i;

	color_context_ls = FALSE;
	color_context_lsmF = FALSE;
	for (i = 0; vp->vec[i]; i++)
	    if (Strcmp(vp->vec[i], STRls) == 0)
		color_context_ls = TRUE;
	    else if (Strcmp(vp->vec[i], STRlsmF) == 0)
		color_context_lsmF = TRUE;
    }
}


/* getstring():
 */
static  bool
getstring(dp, sp, pd, f)
    char        **dp;		/* dest buffer */
    const Char  **sp;		/* source buffer */
    Str          *pd;		/* pointer to dest buffer */
    int           f;		/* final character */
{
    const Char *s = *sp;
    char *d = *dp;
    int sc;

    while (*s && (*s & CHAR) != f && (*s & CHAR) != ':') {
	if ((*s & CHAR) == '\\' || (*s & CHAR) == '^') {
	    if ((sc = parseescape(&s)) == -1)
		return 0;
	    else
		*d++ = (char) sc;
	}
	else
	    *d++ = *s++ & CHAR;
    }

    pd->s = *dp;
    pd->len = (int) (d - *dp);
    *sp = s;
    *dp = d;
    return *s == f;
}


/* parseLS_COLORS():
 *	Parse the LS_COLORS environment variable
 */
void
parseLS_COLORS(value)
    Char   *value;		/* LS_COLOR variable's value */
{
    int     i;
    size_t  len;
    const Char   *v;		/* pointer in value */
    char   *c;			/* pointer in colors */
    Extension *e;		/* pointer in extensions */
    jmp_buf_t osetexit;

    /* init */
    if (extensions)
        xfree((ptr_t) extensions);
    for (i = 0; i < nvariables; i++)
	variables[i].color = variables[i].defaultcolor;
    colors = NULL;
    extensions = NULL;
    nextensions = 0;

    if (value == NULL)
	return;

    len = Strlen(value);
    /* allocate memory */
    i = 1;
    for (v = value; *v; v++)
	if ((*v & CHAR) == ':')
	    i++;
    extensions = (Extension *) xmalloc((size_t) (len + i * sizeof(Extension)));
    colors = i * sizeof(Extension) + (char *)extensions;
    nextensions = 0;

    /* init pointers */
    v = value;
    c = colors;
    e = &extensions[0];

    /* Prevent from crashing if unknown parameters are given. */

    getexit(osetexit);

    if (setexit() == 0) {
	    
    /* parse */
    while (*v) {
	switch (*v & CHAR) {
	case ':':
	    v++;
	    continue;

	case '*':		/* :*ext=color: */
	    v++;
	    if (getstring(&c, &v, &e->extension, '=') &&
		0 < e->extension.len) {
		v++;
		getstring(&c, &v, &e->color, ':');
		e++;
		continue;
	    }
	    break;

	default:		/* :vl=color: */
	    if (v[0] && v[1] && (v[2] & CHAR) == '=') {
		for (i = 0; i < nvariables; i++)
		    if (variables[i].variable[0] == (v[0] & CHAR) &&
			variables[i].variable[1] == (v[1] & CHAR))
			break;
		if (i < nvariables) {
		    v += 3;
		    getstring(&c, &v, &variables[i].color, ':');
		    continue;
		}
		else
		    stderror(ERR_BADCOLORVAR, v[0], v[1]);
	    }
	    break;
	}
	while (*v && (*v & CHAR) != ':')
	    v++;
    }
    }

    resexit(osetexit);

    nextensions = (int) (e - extensions);
}


/* put_color():
 */
static void
put_color(color)
    Str    *color;
{
    extern bool output_raw;	/* PWP: in sh.print.c */
    size_t  i;
    char   *c = color->s;
    bool    original_output_raw = output_raw;

    output_raw = TRUE;
    for (i = color->len; 0 < i; i--)
	xputchar(*c++);
    output_raw = original_output_raw;
}


/* print_color():
 */
static void
print_color(fname, len, suffix)
    Char   *fname;
    size_t  len;
    int     suffix;
{
    int     i;
    char   *filename = short2str(fname);
    char   *last = filename + len;
    Str    *color = &variables[VFile].color;

    switch (suffix) {
    case '>':			/* File is a symbolic link pointing to
				 * a directory */
        color = &variables[VDir].color;
	    break;
    case '+':			/* File is a hidden directory [aix] or
				 * context dependent [hpux] */
    case ':':			/* File is network special [hpux] */
        break;
    default:
	for (i = 0; i < nvariables; i++)
	    if (variables[i].suffix != NOS &&
		variables[i].suffix == suffix) {
		color = &variables[i].color;
		break;
	    }
	if (i == nvariables) {
	    for (i = 0; i < nextensions; i++)
	        if (strncmp(last - extensions[i].extension.len,
			    extensions[i].extension.s,
			    extensions[i].extension.len) == 0) {
		  color = &extensions[i].color;
		break;
	    }
	}
	break;
    }

    put_color(&variables[VLeft].color);
    put_color(color);
    put_color(&variables[VRight].color);
}


/* print_with_color():
 */
void
print_with_color(filename, len, suffix)
    Char   *filename;
    size_t  len;
    int    suffix;
{
    if (color_context_lsmF &&
	(haderr ? (didfds ? is2atty : isdiagatty) :
	 (didfds ? is1atty : isoutatty))) {
	print_color(filename, len, suffix);
	xprintf("%S", filename);
	if (0 < variables[VEnd].color.len)
	    put_color(&variables[VEnd].color);
	else {
	    put_color(&variables[VLeft].color);
	    put_color(&variables[VNormal].color);
	    put_color(&variables[VRight].color);
	}
	xputchar(suffix);
    }
    else
	xprintf("%S%c", filename, suffix);
}


#endif /* COLOR_LS_F */
