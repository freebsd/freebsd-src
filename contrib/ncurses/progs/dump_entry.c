/****************************************************************************
 * Copyright (c) 1998-2000 Free Software Foundation, Inc.                   *
 *                                                                          *
 * Permission is hereby granted, free of charge, to any person obtaining a  *
 * copy of this software and associated documentation files (the            *
 * "Software"), to deal in the Software without restriction, including      *
 * without limitation the rights to use, copy, modify, merge, publish,      *
 * distribute, distribute with modifications, sublicense, and/or sell       *
 * copies of the Software, and to permit persons to whom the Software is    *
 * furnished to do so, subject to the following conditions:                 *
 *                                                                          *
 * The above copyright notice and this permission notice shall be included  *
 * in all copies or substantial portions of the Software.                   *
 *                                                                          *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS  *
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF               *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.   *
 * IN NO EVENT SHALL THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,   *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR    *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR    *
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.                               *
 *                                                                          *
 * Except as contained in this notice, the name(s) of the above copyright   *
 * holders shall not be used in advertising or otherwise to promote the     *
 * sale, use or other dealings in this Software without prior written       *
 * authorization.                                                           *
 ****************************************************************************/

/****************************************************************************
 *  Author: Zeyd M. Ben-Halim <zmbenhal@netcom.com> 1992,1995               *
 *     and: Eric S. Raymond <esr@snark.thyrsus.com>                         *
 ****************************************************************************/

#define __INTERNAL_CAPS_VISIBLE
#include <progs.priv.h>

#include "dump_entry.h"
#include "termsort.c"		/* this C file is generated */
#include <parametrized.h>	/* so is this */

MODULE_ID("$Id: dump_entry.c,v 1.55 2001/03/10 19:45:51 tom Exp $")

#define INDENT			8
#define DISCARD(string) string = ABSENT_STRING
#define PRINTF (void) printf

typedef struct {
    char *text;
    size_t used;
    size_t size;
} DYNBUF;

static int tversion;		/* terminfo version */
static int outform;		/* output format to use */
static int sortmode;		/* sort mode to use */
static int width = 60;		/* max line width for listings */
static int column;		/* current column, limited by 'width' */
static int oldcol;		/* last value of column before wrap */
static int tracelevel;		/* level of debug output */
static bool pretty;		/* true if we format if-then-else strings */

static DYNBUF outbuf;
static DYNBUF tmpbuf;

/* indirection pointers for implementing sort and display modes */
static const int *bool_indirect, *num_indirect, *str_indirect;
static NCURSES_CONST char *const *bool_names;
static NCURSES_CONST char *const *num_names;
static NCURSES_CONST char *const *str_names;

static const char *separator, *trailer;

/* cover various ports and variants of terminfo */
#define V_ALLCAPS	0	/* all capabilities (SVr4, XSI, ncurses) */
#define V_SVR1		1	/* SVR1, Ultrix */
#define V_HPUX		2	/* HP/UX */
#define V_AIX		3	/* AIX */
#define V_BSD		4	/* BSD */

#if NCURSES_XNAMES
#define OBSOLETE(n) (!_nc_user_definable && (n[0] == 'O' && n[1] == 'T'))
#else
#define OBSOLETE(n) (n[0] == 'O' && n[1] == 'T')
#endif

#define isObsolete(f,n) ((f == F_TERMINFO || f == F_VARIABLE) && OBSOLETE(n))

#if NCURSES_XNAMES
#define BoolIndirect(j) ((j >= BOOLCOUNT) ? (j) : ((sortmode == S_NOSORT) ? j : bool_indirect[j]))
#define NumIndirect(j)  ((j >= NUMCOUNT)  ? (j) : ((sortmode == S_NOSORT) ? j : num_indirect[j]))
#define StrIndirect(j)  ((j >= STRCOUNT)  ? (j) : ((sortmode == S_NOSORT) ? j : str_indirect[j]))
#else
#define BoolIndirect(j) ((sortmode == S_NOSORT) ? (j) : bool_indirect[j])
#define NumIndirect(j)  ((sortmode == S_NOSORT) ? (j) : num_indirect[j])
#define StrIndirect(j)  ((sortmode == S_NOSORT) ? (j) : str_indirect[j])
#endif

static void
strncpy_DYN(DYNBUF * dst, const char *src, size_t need)
{
    size_t want = need + dst->used + 1;
    if (want > dst->size) {
	dst->size += (want + 1024);	/* be generous */
	dst->text = typeRealloc(char, dst->size, dst->text);
    }
    (void) strncpy(dst->text + dst->used, src, need);
    dst->used += need;
    dst->text[dst->used] = 0;
}

static void
strcpy_DYN(DYNBUF * dst, const char *src)
{
    if (src == 0) {
	dst->used = 0;
	strcpy_DYN(dst, "");
    } else {
	strncpy_DYN(dst, src, strlen(src));
    }
}

#if NO_LEAKS
static void
free_DYN(DYNBUF * p)
{
    if (p->text != 0)
	free(p->text);
    p->text = 0;
    p->size = 0;
    p->used = 0;
}

void
_nc_leaks_dump_entry(void)
{
    free_DYN(&outbuf);
    free_DYN(&tmpbuf);
}
#endif

NCURSES_CONST char *
nametrans(const char *name)
/* translate a capability name from termcap to terminfo */
{
    const struct name_table_entry *np;

    if ((np = _nc_find_entry(name, _nc_get_hash_table(0))) != 0)
	switch (np->nte_type) {
	case BOOLEAN:
	    if (bool_from_termcap[np->nte_index])
		return (boolcodes[np->nte_index]);
	    break;

	case NUMBER:
	    if (num_from_termcap[np->nte_index])
		return (numcodes[np->nte_index]);
	    break;

	case STRING:
	    if (str_from_termcap[np->nte_index])
		return (strcodes[np->nte_index]);
	    break;
	}

    return (0);
}

void
dump_init(const char *version, int mode, int sort, int twidth, int traceval,
    bool formatted)
/* set up for entry display */
{
    width = twidth;
    pretty = formatted;
    tracelevel = traceval;

    /* versions */
    if (version == 0)
	tversion = V_ALLCAPS;
    else if (!strcmp(version, "SVr1") || !strcmp(version, "SVR1")
	|| !strcmp(version, "Ultrix"))
	tversion = V_SVR1;
    else if (!strcmp(version, "HP"))
	tversion = V_HPUX;
    else if (!strcmp(version, "AIX"))
	tversion = V_AIX;
    else if (!strcmp(version, "BSD"))
	tversion = V_BSD;
    else
	tversion = V_ALLCAPS;

    /* implement display modes */
    switch (outform = mode) {
    case F_LITERAL:
    case F_TERMINFO:
	bool_names = boolnames;
	num_names = numnames;
	str_names = strnames;
	separator = twidth ? ", " : ",";
	trailer = "\n\t";
	break;

    case F_VARIABLE:
	bool_names = boolfnames;
	num_names = numfnames;
	str_names = strfnames;
	separator = twidth ? ", " : ",";
	trailer = "\n\t";
	break;

    case F_TERMCAP:
    case F_TCONVERR:
	bool_names = boolcodes;
	num_names = numcodes;
	str_names = strcodes;
	separator = ":";
	trailer = "\\\n\t:";
	break;
    }

    /* implement sort modes */
    switch (sortmode = sort) {
    case S_NOSORT:
	if (traceval)
	    (void) fprintf(stderr,
		"%s: sorting by term structure order\n", _nc_progname);
	break;

    case S_TERMINFO:
	if (traceval)
	    (void) fprintf(stderr,
		"%s: sorting by terminfo name order\n", _nc_progname);
	bool_indirect = bool_terminfo_sort;
	num_indirect = num_terminfo_sort;
	str_indirect = str_terminfo_sort;
	break;

    case S_VARIABLE:
	if (traceval)
	    (void) fprintf(stderr,
		"%s: sorting by C variable order\n", _nc_progname);
	bool_indirect = bool_variable_sort;
	num_indirect = num_variable_sort;
	str_indirect = str_variable_sort;
	break;

    case S_TERMCAP:
	if (traceval)
	    (void) fprintf(stderr,
		"%s: sorting by termcap name order\n", _nc_progname);
	bool_indirect = bool_termcap_sort;
	num_indirect = num_termcap_sort;
	str_indirect = str_termcap_sort;
	break;
    }

    if (traceval)
	(void) fprintf(stderr,
	    "%s: width = %d, tversion = %d, outform = %d\n",
	    _nc_progname, width, tversion, outform);
}

static TERMTYPE *cur_type;

static int
dump_predicate(int type, int idx)
/* predicate function to use for ordinary decompilation */
{
    switch (type) {
    case BOOLEAN:
	return (cur_type->Booleans[idx] == FALSE)
	    ? FAIL : cur_type->Booleans[idx];

    case NUMBER:
	return (cur_type->Numbers[idx] == ABSENT_NUMERIC)
	    ? FAIL : cur_type->Numbers[idx];

    case STRING:
	return (cur_type->Strings[idx] != ABSENT_STRING)
	    ? (int) TRUE : FAIL;
    }

    return (FALSE);		/* pacify compiler */
}

static void set_obsolete_termcaps(TERMTYPE * tp);

/* is this the index of a function key string? */
#define FNKEY(i)	(((i)<= 65 && (i)>= 75) || ((i)<= 216 && (i)>= 268))

static bool
version_filter(int type, int idx)
/* filter out capabilities we may want to suppress */
{
    switch (tversion) {
    case V_ALLCAPS:		/* SVr4, XSI Curses */
	return (TRUE);

    case V_SVR1:		/* System V Release 1, Ultrix */
	switch (type) {
	case BOOLEAN:
	    /* below and including xon_xoff */
	    return ((idx <= 20) ? TRUE : FALSE);
	case NUMBER:
	    /* below and including width_status_line */
	    return ((idx <= 7) ? TRUE : FALSE);
	case STRING:
	    /* below and including prtr_non */
	    return ((idx <= 144) ? TRUE : FALSE);
	}
	break;

    case V_HPUX:		/* Hewlett-Packard */
	switch (type) {
	case BOOLEAN:
	    /* below and including xon_xoff */
	    return ((idx <= 20) ? TRUE : FALSE);
	case NUMBER:
	    /* below and including label_width */
	    return ((idx <= 10) ? TRUE : FALSE);
	case STRING:
	    if (idx <= 144)	/* below and including prtr_non */
		return (TRUE);
	    else if (FNKEY(idx))	/* function keys */
		return (TRUE);
	    else if (idx == 147 || idx == 156 || idx == 157)	/* plab_norm,label_on,label_off */
		return (TRUE);
	    else
		return (FALSE);
	}
	break;

    case V_AIX:		/* AIX */
	switch (type) {
	case BOOLEAN:
	    /* below and including xon_xoff */
	    return ((idx <= 20) ? TRUE : FALSE);
	case NUMBER:
	    /* below and including width_status_line */
	    return ((idx <= 7) ? TRUE : FALSE);
	case STRING:
	    if (idx <= 144)	/* below and including prtr_non */
		return (TRUE);
	    else if (FNKEY(idx))	/* function keys */
		return (TRUE);
	    else
		return (FALSE);
	}
	break;

    case V_BSD:		/* BSD */
	switch (type) {
	case BOOLEAN:
	    return bool_from_termcap[idx];
	case NUMBER:
	    return num_from_termcap[idx];
	case STRING:
	    return str_from_termcap[idx];
	}
	break;
    }

    return (FALSE);		/* pacify the compiler */
}

static void
force_wrap(void)
{
    oldcol = column;
    strcpy_DYN(&outbuf, trailer);
    column = INDENT;
}

static void
wrap_concat(const char *src)
{
    int need = strlen(src);
    int want = strlen(separator) + need;

    if (column > INDENT
	&& column + want > width) {
	force_wrap();
    }
    strcpy_DYN(&outbuf, src);
    strcpy_DYN(&outbuf, separator);
    column += need;
}

#define IGNORE_SEP_TRAIL(first,last,sep_trail) \
	if ((size_t)(last - first) > sizeof(sep_trail)-1 \
	 && !strncmp(first, sep_trail, sizeof(sep_trail)-1)) \
	 	first += sizeof(sep_trail)-2

/* Returns the nominal length of the buffer assuming it is termcap format,
 * i.e., the continuation sequence is treated as a single character ":".
 *
 * There are several implementations of termcap which read the text into a
 * fixed-size buffer.  Generally they strip the newlines from the text, but may
 * not do it until after the buffer is read.  Also, "tc=" resolution may be
 * expanded in the same buffer.  This function is useful for measuring the size
 * of the best fixed-buffer implementation; the worst case may be much worse.
 */
#ifdef TEST_TERMCAP_LENGTH
static int
termcap_length(const char *src)
{
    static const char pattern[] = ":\\\n\t:";

    int len = 0;
    const char *const t = src + strlen(src);

    while (*src != '\0') {
	IGNORE_SEP_TRAIL(src, t, pattern);
	src++;
	len++;
    }
    return len;
}
#else
#define termcap_length(src) strlen(src)
#endif

static char *
fmt_complex(char *src, int level)
{
    int percent = 0;
    int n;
    bool if_then = strstr(src, "%?") != 0;
    bool params = !if_then && (strlen(src) > 50) && (strstr(src, "%p") != 0);

    while (*src != '\0') {
	switch (*src) {
	case '\\':
	    percent = 0;
	    strncpy_DYN(&tmpbuf, src++, 1);
	    break;
	case '%':
	    percent = 1;
	    break;
	case '?':		/* "if" */
	case 't':		/* "then" */
	case 'e':		/* "else" */
	    if (percent) {
		percent = 0;
		tmpbuf.text[tmpbuf.used - 1] = '\n';
		/* treat a "%e%?" as else-if, on the same level */
		if (!strncmp(src, "e%?", 3)) {
		    for (n = 0; n < level; n++)
			strncpy_DYN(&tmpbuf, "\t", 1);
		    strncpy_DYN(&tmpbuf, "%", 1);
		    strncpy_DYN(&tmpbuf, src, 3);
		    src += 3;
		} else {
		    for (n = 0; n <= level; n++)
			strncpy_DYN(&tmpbuf, "\t", 1);
		    strncpy_DYN(&tmpbuf, "%", 1);
		    strncpy_DYN(&tmpbuf, src, 1);
		    if (*src++ == '?') {
			src = fmt_complex(src, level + 1);
		    } else if (level == 1) {
			_nc_warning("%%%c without %%?", *src);
		    }
		}
		continue;
	    }
	    break;
	case ';':		/* "endif" */
	    if (percent) {
		percent = 0;
		if (level > 1) {
		    tmpbuf.text[tmpbuf.used - 1] = '\n';
		    for (n = 0; n < level; n++)
			strncpy_DYN(&tmpbuf, "\t", 1);
		    strncpy_DYN(&tmpbuf, "%", 1);
		    strncpy_DYN(&tmpbuf, src++, 1);
		    return src;
		}
		_nc_warning("%%; without %%?");
	    }
	    break;
	case 'p':
	    if (percent && params) {
		tmpbuf.text[tmpbuf.used - 1] = '\n';
		for (n = 0; n <= level; n++)
		    strncpy_DYN(&tmpbuf, "\t", 1);
		strncpy_DYN(&tmpbuf, "%", 1);
	    }
	    percent = 0;
	    break;
	default:
	    percent = 0;
	    break;
	}
	strncpy_DYN(&tmpbuf, src++, 1);
    }
    return src;
}

int
fmt_entry(TERMTYPE * tterm,
    int (*pred) (int type, int idx),
    bool suppress_untranslatable,
    bool infodump,
    int numbers)
{
    int i, j;
    char buffer[MAX_TERMINFO_LENGTH];
    NCURSES_CONST char *name;
    int predval, len;
    int num_bools = 0;
    int num_values = 0;
    int num_strings = 0;
    bool outcount = 0;

#define WRAP_CONCAT	\
	wrap_concat(buffer); \
	outcount = TRUE

    len = 12;			/* terminfo file-header */

    if (pred == 0) {
	cur_type = tterm;
	pred = dump_predicate;
    }

    strcpy_DYN(&outbuf, 0);
    strcpy_DYN(&outbuf, tterm->term_names);
    strcpy_DYN(&outbuf, separator);
    column = outbuf.used;
    force_wrap();

    for_each_boolean(j, tterm) {
	i = BoolIndirect(j);
	name = ExtBoolname(tterm, i, bool_names);

	if (!version_filter(BOOLEAN, i))
	    continue;
	else if (isObsolete(outform, name))
	    continue;

	predval = pred(BOOLEAN, i);
	if (predval != FAIL) {
	    (void) strcpy(buffer, name);
	    if (predval <= 0)
		(void) strcat(buffer, "@");
	    else if (i + 1 > num_bools)
		num_bools = i + 1;
	    WRAP_CONCAT;
	}
    }

    if (column != INDENT)
	force_wrap();

    for_each_number(j, tterm) {
	i = NumIndirect(j);
	name = ExtNumname(tterm, i, num_names);

	if (!version_filter(NUMBER, i))
	    continue;
	else if (isObsolete(outform, name))
	    continue;

	predval = pred(NUMBER, i);
	if (predval != FAIL) {
	    if (tterm->Numbers[i] < 0) {
		sprintf(buffer, "%s@", name);
	    } else {
		sprintf(buffer, "%s#%d", name, tterm->Numbers[i]);
		if (i + 1 > num_values)
		    num_values = i + 1;
	    }
	    WRAP_CONCAT;
	}
    }

    if (column != INDENT)
	force_wrap();

    len += num_bools
	+ num_values * 2
	+ strlen(tterm->term_names) + 1;
    if (len & 1)
	len++;

#undef CUR
#define CUR tterm->
    if (outform == F_TERMCAP) {
	if (termcap_reset != ABSENT_STRING) {
	    if (init_3string != ABSENT_STRING
		&& !strcmp(init_3string, termcap_reset))
		DISCARD(init_3string);

	    if (reset_2string != ABSENT_STRING
		&& !strcmp(reset_2string, termcap_reset))
		DISCARD(reset_2string);
	}
    }

    for_each_string(j, tterm) {
	i = StrIndirect(j);
	name = ExtStrname(tterm, i, str_names);

	if (!version_filter(STRING, i))
	    continue;
	else if (isObsolete(outform, name))
	    continue;

	/*
	 * Some older versions of vi want rmir/smir to be defined
	 * for ich/ich1 to work.  If they're not defined, force
	 * them to be output as defined and empty.
	 */
	if (outform == F_TERMCAP) {
	    if (insert_character || parm_ich) {
		if (&tterm->Strings[i] == &enter_insert_mode
		    && enter_insert_mode == ABSENT_STRING) {
		    (void) strcpy(buffer, "im=");
		    WRAP_CONCAT;
		    continue;
		}

		if (&tterm->Strings[i] == &exit_insert_mode
		    && exit_insert_mode == ABSENT_STRING) {
		    (void) strcpy(buffer, "ei=");
		    WRAP_CONCAT;
		    continue;
		}
	    }
	}

	predval = pred(STRING, i);
	buffer[0] = '\0';

	if (predval != FAIL) {
	    if (tterm->Strings[i] != ABSENT_STRING
		&& i + 1 > num_strings)
		num_strings = i + 1;

	    if (!VALID_STRING(tterm->Strings[i])) {
		sprintf(buffer, "%s@", name);
		WRAP_CONCAT;
	    } else if (outform == F_TERMCAP || outform == F_TCONVERR) {
		int params = (i < (int) SIZEOF(parametrized)) ? parametrized[i] : 0;
		char *srccap = _nc_tic_expand(tterm->Strings[i], TRUE, numbers);
		char *cv = _nc_infotocap(name, srccap, params);

		if (cv == 0) {
		    if (outform == F_TCONVERR) {
			sprintf(buffer, "%s=!!! %s WILL NOT CONVERT !!!",
			    name, srccap);
		    } else if (suppress_untranslatable) {
			continue;
		    } else {
			char *s = srccap, *d = buffer;
			sprintf(d, "..%s=", name);
			d += strlen(d);
			while ((*d = *s++) != 0) {
			    if (*d == ':') {
				*d++ = '\\';
				*d = ':';
			    } else if (*d == '\\') {
				*++d = *s++;
			    }
			    d++;
			}
		    }
		} else {
		    sprintf(buffer, "%s=%s", name, cv);
		}
		len += strlen(tterm->Strings[i]) + 1;
		WRAP_CONCAT;
	    } else {
		char *src = _nc_tic_expand(tterm->Strings[i],
		    outform == F_TERMINFO, numbers);

		strcpy_DYN(&tmpbuf, 0);
		strcpy_DYN(&tmpbuf, name);
		strcpy_DYN(&tmpbuf, "=");
		if (pretty
		    && (outform == F_TERMINFO
			|| outform == F_VARIABLE)) {
		    fmt_complex(src, 1);
		} else {
		    strcpy_DYN(&tmpbuf, src);
		}
		len += strlen(tterm->Strings[i]) + 1;
		wrap_concat(tmpbuf.text);
		outcount = TRUE;
	    }
	}
    }
    len += num_strings * 2;

    /*
     * This piece of code should be an effective inverse of the functions
     * postprocess_terminfo and postprocess_terminfo in parse_entry.c.
     * Much more work should be done on this to support dumping termcaps.
     */
    if (tversion == V_HPUX) {
	if (memory_lock) {
	    (void) sprintf(buffer, "meml=%s", memory_lock);
	    WRAP_CONCAT;
	}
	if (memory_unlock) {
	    (void) sprintf(buffer, "memu=%s", memory_unlock);
	    WRAP_CONCAT;
	}
    } else if (tversion == V_AIX) {
	if (VALID_STRING(acs_chars)) {
	    bool box_ok = TRUE;
	    const char *acstrans = "lqkxjmwuvtn";
	    const char *cp;
	    char *tp, *sp, boxchars[11];

	    tp = boxchars;
	    for (cp = acstrans; *cp; cp++) {
		sp = strchr(acs_chars, *cp);
		if (sp)
		    *tp++ = sp[1];
		else {
		    box_ok = FALSE;
		    break;
		}
	    }
	    tp[0] = '\0';

	    if (box_ok) {
		(void) strcpy(buffer, "box1=");
		(void) strcat(buffer, _nc_tic_expand(boxchars,
			outform == F_TERMINFO, numbers));
		WRAP_CONCAT;
	    }
	}
    }

    /*
     * kludge: trim off trailer to avoid an extra blank line
     * in infocmp -u output when there are no string differences
     */
    if (outcount) {
	bool trimmed = FALSE;
	j = outbuf.used;
	if (j >= 2
	    && outbuf.text[j - 1] == '\t'
	    && outbuf.text[j - 2] == '\n') {
	    outbuf.used -= 2;
	    trimmed = TRUE;
	} else if (j >= 4
		&& outbuf.text[j - 1] == ':'
		&& outbuf.text[j - 2] == '\t'
		&& outbuf.text[j - 3] == '\n'
	    && outbuf.text[j - 4] == '\\') {
	    outbuf.used -= 4;
	    trimmed = TRUE;
	}
	if (trimmed) {
	    outbuf.text[outbuf.used] = '\0';
	    column = oldcol;
	}
    }
#if 0
    fprintf(stderr, "num_bools = %d\n", num_bools);
    fprintf(stderr, "num_values = %d\n", num_values);
    fprintf(stderr, "num_strings = %d\n", num_strings);
    fprintf(stderr, "term_names=%s, len=%d, strlen(outbuf)=%d, outbuf=%s\n",
	tterm->term_names, len, outbuf.used, outbuf.text);
#endif
    /*
     * Here's where we use infodump to trigger a more stringent length check
     * for termcap-translation purposes.
     * Return the length of the raw entry, without tc= expansions,
     * It gives an idea of which entries are deadly to even *scan past*,
     * as opposed to *use*.
     */
    return (infodump ? len : (int) termcap_length(outbuf.text));
}

int
dump_entry(TERMTYPE * tterm, bool limited, int numbers, int (*pred) (int
	type, int idx))
/* dump a single entry */
{
    int len, critlen;
    const char *legend;
    bool infodump;

    if (outform == F_TERMCAP || outform == F_TCONVERR) {
	critlen = MAX_TERMCAP_LENGTH;
	legend = "older termcap";
	infodump = FALSE;
	set_obsolete_termcaps(tterm);
    } else {
	critlen = MAX_TERMINFO_LENGTH;
	legend = "terminfo";
	infodump = TRUE;
    }

    if (((len = fmt_entry(tterm, pred, FALSE, infodump, numbers)) > critlen)
	&& limited) {
	PRINTF("# (untranslatable capabilities removed to fit entry within %d bytes)\n",
	    critlen);
	if ((len = fmt_entry(tterm, pred, TRUE, infodump, numbers)) > critlen) {
	    /*
	     * We pick on sgr because it's a nice long string capability that
	     * is really just an optimization hack.  Another good candidate is
	     * acsc since it is both long and unused by BSD termcap.
	     */
	    char *oldsgr = set_attributes;
	    char *oldacsc = acs_chars;
	    set_attributes = ABSENT_STRING;
	    PRINTF("# (sgr removed to fit entry within %d bytes)\n",
		critlen);
	    if ((len = fmt_entry(tterm, pred, TRUE, infodump, numbers)) > critlen) {
		acs_chars = ABSENT_STRING;
		PRINTF("# (acsc removed to fit entry within %d bytes)\n",
		    critlen);
	    }
	    if ((len = fmt_entry(tterm, pred, TRUE, infodump, numbers)) > critlen) {
		int oldversion = tversion;

		tversion = V_BSD;
		PRINTF("# (terminfo-only capabilities suppressed to fit entry within %d bytes)\n",
		    critlen);

		if ((len = fmt_entry(tterm, pred, TRUE, infodump, numbers))
		    > critlen) {
		    (void) fprintf(stderr,
			"warning: %s entry is %d bytes long\n",
			_nc_first_name(tterm->term_names),
			len);
		    PRINTF(
			"# WARNING: this entry, %d bytes long, may core-dump %s libraries!\n",
			len, legend);
		}
		tversion = oldversion;
	    }
	    set_attributes = oldsgr;
	    acs_chars = oldacsc;
	}
    }

    (void) fputs(outbuf.text, stdout);
    return len;
}

int
dump_uses(const char *name, bool infodump)
/* dump "use=" clauses in the appropriate format */
{
    char buffer[MAX_TERMINFO_LENGTH];

    strcpy_DYN(&outbuf, 0);
    (void) sprintf(buffer, "%s%s", infodump ? "use=" : "tc=", name);
    wrap_concat(buffer);
    (void) fputs(outbuf.text, stdout);
    return outbuf.used;
}

void
compare_entry(void (*hook) (int t, int i, const char *name), TERMTYPE * tp
    GCC_UNUSED, bool quiet)
/* compare two entries */
{
    int i, j;
    NCURSES_CONST char *name;

    if (!quiet)
	fputs("    comparing booleans.\n", stdout);
    for_each_boolean(j, tp) {
	i = BoolIndirect(j);
	name = ExtBoolname(tp, i, bool_names);

	if (isObsolete(outform, name))
	    continue;

	(*hook) (CMP_BOOLEAN, i, name);
    }

    if (!quiet)
	fputs("    comparing numbers.\n", stdout);
    for_each_number(j, tp) {
	i = NumIndirect(j);
	name = ExtNumname(tp, i, num_names);

	if (isObsolete(outform, name))
	    continue;

	(*hook) (CMP_NUMBER, i, name);
    }

    if (!quiet)
	fputs("    comparing strings.\n", stdout);
    for_each_string(j, tp) {
	i = StrIndirect(j);
	name = ExtStrname(tp, i, str_names);

	if (isObsolete(outform, name))
	    continue;

	(*hook) (CMP_STRING, i, name);
    }

    /* (void) fputs("    comparing use entries.\n", stdout); */
    (*hook) (CMP_USE, 0, "use");

}

#define NOTSET(s)	((s) == 0)

/*
 * This bit of legerdemain turns all the terminfo variable names into
 * references to locations in the arrays Booleans, Numbers, and Strings ---
 * precisely what's needed.
 */
#undef CUR
#define CUR tp->

static void
set_obsolete_termcaps(TERMTYPE * tp)
{
#include "capdefaults.c"
}

/*
 * Convert an alternate-character-set string to canonical form: sorted and
 * unique.
 */
void
repair_acsc(TERMTYPE * tp)
{
    if (VALID_STRING(acs_chars)) {
	size_t n, m;
	char mapped[256];
	char extra = 0;
	unsigned source;
	unsigned target;
	bool fix_needed = FALSE;

	for (n = 0, source = 0; acs_chars[n] != 0; n++) {
	    target = acs_chars[n];
	    if (source >= target) {
		fix_needed = TRUE;
		break;
	    }
	    source = target;
	    if (acs_chars[n + 1])
		n++;
	}
	if (fix_needed) {
	    memset(mapped, 0, sizeof(mapped));
	    for (n = 0; acs_chars[n] != 0; n++) {
		source = acs_chars[n];
		if ((target = (unsigned char) acs_chars[n + 1]) != 0) {
		    mapped[source] = target;
		    n++;
		} else {
		    extra = source;
		}
	    }
	    for (n = m = 0; n < sizeof(mapped); n++) {
		if (mapped[n]) {
		    acs_chars[m++] = n;
		    acs_chars[m++] = mapped[n];
		}
	    }
	    if (extra)
		acs_chars[m++] = extra;		/* garbage in, garbage out */
	    acs_chars[m] = 0;
	}
    }
}
