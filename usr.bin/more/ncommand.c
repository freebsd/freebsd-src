/*-
 * Copyright (c) 1999 Timmy M. Vanderhoek
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

/*
 * These functions handle evaluation of primitive commands.  In general,
 * commands either come from macro.h as it expands user input, or
 * directly from a .morerc file (in which case only a limited set of
 * commands is valid.
 *
 * Commands are matched by command() against a command table.  The rest
 * of the command line string passed to command() is then passed to a
 * function corresponding to the given command.  The specific command
 * function evaluates the remainder of the command string with the help
 * of getstr() and getnumb(), both of which also handle variable expansion
 * into a single word.  It may in the future be desirable to add a special
 * getsstring(), get-search-string, function.  Specific command functions
 * should not try grokking the command string by themselves.
 *
 * A command and its arguments are terminated by either a NUL or a ';'.
 * This is recognized by both getstr() and getint().  Specific command
 * functions return a pointer to the end of the command (and its arguments)
 * thus allowing command() to accept commands that are chained together
 * by semicolons.  If a specific command fails it returns NULL preventing
 * any proceeding commands (chained together with ';') from being parsed.
 * This can be considered as a feature.
 * 
 * All variable-access functions and variable state are internal to
 * ncommand.c.  The sole exceptions are setvar() and setvari().
 */

#include <sys/param.h>

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "less.h"
#include "pathnames.h"

static getint(), getstr_free();
static void **getstr_raisectxt();

/* The internal command table. */

static const char *cscroll(), *cquit(), *cerror(), *ceval(), *cset(),
                  *cflush(), *cmacro(), *caskfile(), *cusercom(),
                  *ctags(), *chscroll(), *cgomisc(), *cgoend(), *csearch(),
                  *cstat(), *cdeftog(), *ccondition(), *chelp(), *cfile(),
                  *cfile_list(), *cedit(), *cmark(), *creadrc();

/* An enum identifying each command */
enum cident {
	DEFTOG,          /* Initialize toggle values */
	EVAL,            /* Evaluate a subexpression */
	SET,             /* Set a variable */
	MACRO,           /* Create a new macro */
	ERROR,           /* Print a notification message */
	CONDITION,       /* Condition evaluation of (almost) _all_ commands */
	CONDITION_N,     /* CONDITION with an inverse truth table */
	CONDITION_TOGGLE,/* Switch to the reverse sense of the last condition */
	USERCOM,         /* Get the user to type in a direct command */
	READRC,          /* Read-in a named rc file */
	QUIT,            /* Quit */
	HELP,            /* Help */
	FLUSH,           /* Flush file buffer and friends */
	REPAINT,         /* Redraw the screen (useful if it got trashed) */
	FORW_SCROLL,     /* Scroll forward N lines */
	BACK_SCROLL,     /* Scroll backward N lines */
	FORW,            /* Jump or scroll forward N lines */
	BACK,            /* Jump or scroll backwards N lines */
	LSCROLL,         /* Scroll horizontally leftwards */
	RSCROLL,         /* Scroll horizontally to the right */
	GOLINE,          /* Goto line number N */
	GOPERCENT,       /* Goto percent N of the file */
	GOEND,           /* Goto the end of the file */
	EDIT,            /* Edit the current file, using getenv(EDITOR) */
	ASKFILE,         /* Ask for a different, new file */
	CFILE,           /* Page/view the N'th next or prev file */
	FILE_LIST,       /* List the files that CFILE moves around in */
	STAT,            /* List detailed file statistics in prompt */
	MAGICASKSEARCH,  /* Ask for a regexp search string */
	SEARCH,          /* Search for a regexp */
	RESEARCH,        /* Search for the next N'th occurrence */
	SETMARK,         /* Set a bookmark to the current position */
	GOMARK,          /* Goto a previously set bookmark */
	ASKFTAG,         /* Ask for a tag to goto */
	NEXTFTAG,        /* Move forward N in the tag queue */
	PREVFTAG,        /* Move backwards N in the tag queue */
};

static struct ctable {
	const char *cname;
	enum cident cident;
	const char * (*cfunc)(enum cident, const char *args);
} ctable[] = {
	{ "deftog",           DEFTOG,           cdeftog },
	{ "eval",             EVAL,             ceval },
	{ "set",              SET,              cset },
	{ "macro",            MACRO,            cmacro },
	{ "error",            ERROR,            cerror },
	{ "condition",        CONDITION,        ccondition },
	{ "condition_!",      CONDITION_N,      ccondition },
	{ "condition_toggle", CONDITION_TOGGLE, ccondition },
	{ "condition_else",   CONDITION_TOGGLE, ccondition },
	{ "usercom",          USERCOM,          cusercom },
	{ "readrc",           READRC,           creadrc },
	{ "quit",             QUIT,             cquit },
	{ "help",             HELP,             chelp },
	{ "flush",            FLUSH,            cflush },
	{ "repaint",          REPAINT,          cflush },
	{ "forw_scroll",      FORW_SCROLL,      cscroll },
	{ "back_scroll",      BACK_SCROLL,      cscroll },
	{ "forw",             FORW,             cscroll },
	{ "back",             BACK,             cscroll },
	{ "rscroll",          RSCROLL,          chscroll },
	{ "lscroll",          LSCROLL,          chscroll },
	{ "goline",           GOLINE,           cgomisc },
	{ "gopercent",        GOPERCENT,        cgomisc },
	{ "goend",            GOEND,            cgoend },
	{ "edit",             EDIT,             cedit },
	{ "askfile",          ASKFILE,          caskfile },
	{ "file",             CFILE,            cfile },
	{ "file_list",        FILE_LIST,        cfile_list },
	{ "stat",             STAT,             cstat },
	{ "magicasksearch",   MAGICASKSEARCH,   csearch },
	{ "search",           SEARCH,           csearch },
	{ "research",         RESEARCH,         csearch },
	{ "setmark",          SETMARK,          cmark },
	{ "gomark",           GOMARK,           cmark },
	{ "asktag",           ASKFTAG,          ctags },
	{ "nexttag",          NEXTFTAG,         ctags },
	{ "prevtag",          PREVFTAG,         ctags },
};


/* I believe this is just for cosmetic purposes. */
#define CMD_EXEC lower_left(); flush()


/*
 * Prototypes are for people who can't program.
 */


/*
 * The main command string evaluator.  Returns -1 if an error occurred
 * in the command or in executing the command, returns 0 otherwise.  If an
 * error occurs while evaluating a command line containing multiple commands,
 * commands after the error are not processed.  Multiple commands may be
 * separated by ';' or '\n'.  (Multiple commands may also be separated by
 * a ' ', but this is really a bug...)
 */
int
command(line)
	const char *line;
{
	struct ctable *i;

donextcommand:

	while (isspace(*line) || *line == ';' || *line == '\n') line++;
	if (!*line)
		return 0;

	for (i = ctable; i != ctable + sizeof(ctable) / sizeof(struct ctable);
	    i++) {
		if (!strncmp(i->cname, line, strlen(i->cname)) &&
		    (line[strlen(i->cname)] == ' ' ||
		     line[strlen(i->cname)] == ';' ||
		     line[strlen(i->cname)] == '\0')) {
			/* Found a match! */
			void **ctxt;
			CMD_EXEC;
			ctxt = getstr_raisectxt();
			line = i->cfunc (i->cident, line + strlen(i->cname));
			getstr_free(ctxt);
			if (!line)
				return -1;  /* error evaluating command */
			goto donextcommand;
		}
	}

	SETERRSTR(E_BOGCOM, "invalid command: ``%s''", line);
	(void) command("condition true");
	return -1;
}


/*****************************************************************************
 *
 * Functions to help specific command functions to parse their arguments.
 *
 * The three functions here, getstr(), getint(), and gettog() could in theory
 * have vastly different concepts of what a number is, and what a string is,
 * etc., but in practice they don't.
 */

static char *readvar();

#define NCTXTS 30
void *getstr_ctxts[NCTXTS];  /* could easily be made dynamic... */
void **getstr_curctxt = getstr_ctxts;

/*
 * Read a single argument string from a command string.  This understands
 * $variables, "double quotes", 'single quotes', and backslash escapes
 * for \\, \$, \n, \e, \t, and \" (the latter only inside double quotes).  A
 * string may be delimited by double quotes or spaces, not both (duh).  It
 * may be worthwhile to add another quotation style in which arithmetic
 * expressions are expanded.  Currently an arithmetic expression is expanded
 * iff it is the only component of the string.
 *
 * Returns a pointer to the beginning of the string or NULL if it was unable to
 * read a string.  The line is modified to point somewhere between the end of
 * the command argument just read-in and the beginning of the next command
 * argument (if any).  The returned pointer will be free()'d by calling
 * getstr_free().
 */
static char *
getstr(line)
	char **line;	/* Where to look for the return string */
{
	int doquotes = 0;  /* Doing a double-quote string */
	char *retr;

	if (getstr_curctxt - getstr_ctxts == NCTXTS) {
		SETERRSTR(E_COMPLIM,
		    "compile-time limit exceeded: command contexts");
		return NULL;  /* wouldn't be able to register return pointer */
	}

	while (isspace(**line)) (*line)++;
	if (**line == '\'') {
		/* Read until closing quote or '\0'. */
		char *nextw = retr = malloc(1);
		char *c = ++(*line);
		int l;
		for (; *c; c++) {
			if (*c == '\'') {
				if (c[-1] == '\\') {
					nextw[-1] = '\'';
					continue;
				} else {
					*nextw = '\0';
					*line = c + 1;
					*getstr_curctxt = retr;
					getstr_curctxt++;
					return retr;
				}
			}
			l = nextw - retr;
			/* XXX How many realloc()'s can you make per second? */
			if (!(retr = reallocf(retr, c - *line + 250))) {
				SETERR (E_MALLOC);
				return NULL;
			}
			nextw = retr + l;
			*nextw = *c;
			nextw++;
		}
		SETERR(E_CANTPARSE);
		return NULL;
	}
	if (**line == '"') {
		doquotes = 1;
		(*line)++;
	}
	if (**line == '(') {
		/* An arithmetic expression instead of a string...  Well, I
		 * guess this is valid.  See comment leading this function. */
		int n;
		if (getint(&n, line))
			return NULL;
		retr = NULL;
		asprintf(&retr, "%d", n);
		if (!retr)
			SETERR (E_MALLOC);
		*getstr_curctxt = retr;
		getstr_curctxt++;
		return retr;
	}

	if (!FMALLOC(1, retr))
		return NULL;
	*retr = '\0';
	for (;;) {
		char *c, hack[2];

		switch (**line) {
		case '\\':
			switch (*(*line + 1)) {
			case '\\': case '$': case '\'':
			case 't':  case ' ': case ';':
				hack[0] = *(*line + 1);
				hack[1] = '\0';
				c = hack;
				(*line) += 2;
				break;
			case 'n':
				c = "\n";
				(*line) += 2;
				break;
			case 'e':
				c = "\e";
				(*line) += 2;
				break;
			case '"':
				if (doquotes) {
					c = "\"";
					(*line) += 2;
					break;
				} else
					; /* fallthrough */
			default:
				c = "\\";
				(*line)++;
				break;
			}
			break;
		case '$':
			(*line)++;
			if (!(c = readvar(line))) {
				free (retr);
				return NULL;
			}
			break;
		case ' ': case '\t': case ';':
			if (!doquotes) {
				doquotes = 1;
		case '"':
				if (doquotes) {
					/* The end of the string */
					(*line)++;
		case '\0':
					*getstr_curctxt = retr;
					getstr_curctxt++;
					return retr;
				}
			}
			/* fallthrough */
		default:
			hack[0] = **line;
			hack[1] = '\0';
			c = hack;
			(*line)++;
			break;
		}

		retr = reallocf(retr, strlen(retr) + strlen(c) + 1);
		if (!retr) {
			SETERR (E_MALLOC);
			return NULL;
		}
		strcat(retr, c);
	}
}

/*
 * Returns a new context that should be passed to getstr_free() so that
 * getstr_free() only free()'s memory from that particular context.
 */
static void **
getstr_raisectxt()
{
	return getstr_curctxt;
}

/*
 * Calls free() on all memory from context or higher.
 */
static
getstr_free(context)
	void **context;
{
	while (getstr_curctxt != context) {
		getstr_curctxt--;
		free (*getstr_curctxt);
	}
}

/*
 * Reads an integer value from a command string.  Typed numbers must be
 * in base10.  If a '(' is found as the first character of the integer value,
 * then getint() will read until a closing ')' unless interupted by an
 * end-of-command marker (error).  The parentheses are expected to contain a
 * simple arithmetic statement involving only one '*', '/', etc. operation.  The
 * rightmost digit or the closing parenthesis should be followed by either a
 * space or an end-of-command marker.
 *
 * Returns 0 on success, -1 on failure.  The line will be modified to just
 * after the last piece of text parsed.
 *
 * XXX We may add support for negative numbers, someday...
 */
static int
getint(numb, line)
	long *numb;	/* The read-in number is returned through this */
	char **line;	/* The command line from which to read numb */
{
	long n;
	int j;
	char *p, *t;

	while (isspace(**line)) (*line)++;

	switch (**line) {
	case '(':
		(*line)++;
		if (getint(numb, line))
			return -1;
		while (isspace(**line)) (*line)++;
		j = **line;
		(*line)++;
		if (j == ')')
			return 0;
		if (**line == '=' && (j == '!' || j == '=')
		   || j == '&' && **line == '&' || j == '|' && **line == '|')
			j = (j << 8) + *((*line)++);
		if (getint(&n, line))
			return -1;
		while (isspace(**line)) (*line)++;
		if (**line != ')') {
			SETERRSTR (E_BADMATH,
			    "missing arithmetic close parenthesis");
			return -1;
		} else
			(*line)++;
		switch (j) {
		case ('!' << 8) + '=':
			*numb = *numb != n;
			return 0;
		case ('=' << 8) + '=':
			*numb = *numb == n;
			return 0;
		case ('&' << 8) + '&':
			*numb = *numb && n;
			return 0;
		case ('|' << 8) + '|':
			*numb = *numb || n;
			return 0;
		case '+':
			*numb += n;
			return 0;
		case '-':
			*numb -= n;
			return 0;
		case '*':
			*numb *= n;
			return 0;
		case '/':
			if (n == 0)
				*numb = 1;
			else
				*numb /= n;
			return 0;
		default:
			SETERRSTR (E_BADMATH,
			   "bad arithmetic operator: ``%c''", j);
			return -1;
		}
	case '$':
		t = (*line)++;
		if (!(p = readvar(line)))
			return -1;
		if (!isdigit(*p)) {
			SETERRSTR (E_BADMATH,
			    "non-number found (``%s'') "
			    "after expanding variable at ``%s''", p, t);
			return -1;
		}
		*numb = atol(p);
		return 0;
	case '9': case '0': case '8': case '1': case '7': case '2': case '6':
	case '3': case '5': case '4':
		*numb = atol(*line);
		while (isdigit(**line)) (*line)++;
		return 0;
	case '"': case '\'':
		/* Uh-oh.  It's really a string.  We'll go through getstr()
		 * and hope for the best, but this isn't looking good. */
		if (!(p = getstr(line)))
			return -1;
		*numb = atol(p);
		return 0;
	default:
		SETERRSTR (E_BADMATH,
		    "non-number found, number expected, before parsing ``%s''",
		    *line);
		return -1;
	}
}

/*
 * Read an argument from the command string and match that argument against
 * a series of legitimate values.  For example,
 *
 * command <<opt0|opt1|opt2>>
 *
 * This command by be given to the command() processor as a variant of either
 * "command opt1" or "command 4", both of which will cause this function to
 * return the value 1.  This function returns -1 on failure.
 *
 * Note that an option (eg. "opt1") must _not_ start with a digit!!
 */
static int
gettog(const char **line, int nopts, ...)
{
	char *str;
	int n;
	va_list opts;

	if (!(str = getstr(line)))
		return -1;

	if (isdigit(*str)) {
		n = atol(str) % nopts;
		return n;
	}

	va_start(opts, nopts);
	for (n=0; n < nopts; n++) {
		if (!strcasecmp(str, va_arg(opts, const char *))) {
			va_end(opts);
			return n;
		}
	}
	va_end(opts);
	SETERR (E_NOTOG);  /* XXX would be nice to list valid toggles... */
	return -1;
}

/*
 * A companion function for gettog().  Example,
 *
 * optnumb = gettog(&args, 3, "opt1", "opt2", "opt3");
 * settog("_lastoptnumb", optnumb, "opt1", "opt2", "opt3");
 *
 * And the variable named _lastoptnumb_s will be set to one of "opt1", "opt2",
 * or "opt3" as per the value of optnumb.  The variable _lastoptnumb_n will
 * also be set to a corresponding value.  The optnumb argument had better
 * be within the correct range (between 0 and 2 in the above example)!!
 */
settog(const char *varname, int optval, int nargs, ...)
{
	va_list opts;
	char *s;
	int optval_orig = optval;

	assert(optval < nargs); assert(optval >= 0);
	if (!(s = malloc(strlen(varname) + 3)))
		return;
	strcpy (s, varname);
	va_start(opts, nargs);
	for (; optval; optval--) va_arg(opts, const char *);
	s[strlen(varname)] = '_';
	s[strlen(varname) + 1] = 's';
	s[strlen(varname) + 2] = '\0';
	(void) setvar(s, va_arg(opts, const char *));
	s[strlen(varname) + 1] = 'n';
	(void) setvari(s, (long) optval_orig);
	clear_error();
	va_end(opts);
	free(s);
}

/*
 * Read {text} and return the string associated with the variable named
 * <<text>>.  Returns NULL on failure.
 */
static char *
readvar(line)
	char **line;
{
	int vlength;
	char *vstart;
	static char *getvar();

	if (**line != '{') {
		SETERR (E_BADVAR);
		return NULL;
	}
	(*line)++;
	for (vlength = 0, vstart = *line; **line && 
	    (isalpha(**line) || **line == '_'); (*line)++)
		vlength++;
	if (**line != '}' || vlength == 0) {
		SETERRSTR (E_BADVAR,
		    "bad character ``%c'' in variable ``%.*s''", **line,
		    vlength, vstart);
		return NULL;
	}
	(*line)++;
	return getvar(vstart, vlength);
}


/*****************************************************************************
 *
 * Track variables.
 *
 */

static struct vble {
	struct vble *next;
	char *name;
	char *value;
} *vble_l;  /* linked-list of existing variables */

/*
 * Return a pointer to the string that variable var represents.  Returns
 * NULL if a match could not be found and sets erreur.
 */
static const char *
getvar(var, len)
	char *var;
	int len;  /* strncmp(var, varmatch, len); is used to match variables */
{
	struct vble *i;

	for (i = vble_l; i; i = i->next) {
		if (!strncasecmp (i->name, var, len))
			return i->value;
	}

	SETERRSTR (E_BADVAR, "variable ``%.*s'' not set", len, var);
	return NULL;
}

/*
 * Set variable var to val.  Returns -1 on failure, 0 on success.
 */
int
setvar(var, val)
	char *var;  /* variable to set */
	char *val;  /* value to set variable to */
{
	struct vble *i, *last;
	char *var_n, *val_n;
	char *c;

	for (c = var; *c && (isalpha(*c) || *c == '_'); c++) ;
	if (*c) {
		SETERRSTR (E_BADVAR,
		    "bad character ``%c'' in variable ``%s''", *c, var);
		return -1;
	}

	for (i = vble_l; i; last = i, i = i->next) {
		if (!strcasecmp (i->name, var)) {
			if (!FMALLOC(strlen(val) + 1, val_n))
				return -1;
			free(i->value);
			i->value = val_n;
			strcpy(i->value, val);
			return 0;
		}
	}

	/* Need to add another variable to the list vble_l */
	if (!FMALLOC(strlen(var) + 1, var_n))
		return -1;
	if (!FMALLOC(strlen(val) + 1, val_n))
		return -1;
	if (!vble_l) {
		if (!FMALLOC(sizeof(struct vble), vble_l))
			return -1;
		i = vble_l;
	} else {
		if (!FMALLOC(sizeof(struct vble), last->next))
			return -1;
		i = last->next;
	}
	i->next = NULL;
	i->name = var_n; strcpy(i->name, var);
	i->value = val_n; strcpy(i->value, val);
	return 0;
}

/*
 * Set or reset, as appropriate, variable var to val.
 */
int
setvari(var, val)
	const char *var;
	long val;
{
	char n[21];  /* XXX */
	snprintf(n, sizeof(n), "%ld", val);
	n[20] = '\0';
	setvar(var, n);
}


/*****************************************************************************
 *
 * Specific command functions.  These aren't actually individual functions,
 * since using a gigantic switch statement is faster to type, but they
 * pretend to be individual functions.
 *
 */

int condition_eval = 1;  /* false if we just parse commands, but do nothing */

#define ARGSTR(v) do {                                                         \
		if (!((v) = getstr(&args))) return NULL;                       \
	} while (0)
#define ARGNUM(v) do {                                                         \
		if (getint(&(v), &args)) return NULL;                          \
	} while (0)
/* semi-gratuitous use of GNU cpp extension */ 
#define ARGTOG(v, n, togs...) do {                                             \
		if (((v) = gettog(&args, n, togs)) == -1)                      \
			return NULL;                                           \
	} while (0)
#define ENDPARSE do {                                                          \
		if (!condition_eval) return args;                              \
	} while (0)

/*
 * deftog
 *
 * Set all toggle options to their default values, provided the toggle option
 * is registered with this function.  This command is meant to be used at the
 * beginning of the startup command list.
 */
static const char *
cdeftog(cident, args)
	enum cident cident;
	const char *args;
{
	extern int horiz_off, wraplines;

	ENDPARSE;
	settog("_stat", 1, 2, "on", "off");
	settog("_ls_direction", 0, 2, "forw", "back");
	settog("_ls_sense", 0, 2, "noinvert", "invert");
	setvari("_curhscroll", (long) horiz_off);
	settog("_wraplines", wraplines, 2, "off", "on");
	setvar("_ls_regexp", "");
	/*
	 * not present: _file_direction
	 */
	return args;
}

/*
 * eval <<string>>
 *
 * Passes string back into the command evaluator.
 */
static const char *
ceval(cident, args)
	enum cident cident;
	const char *args;
{
	const char *com;

	ARGSTR(com);  /* The command line to evaluate */
	ENDPARSE;

	/* It's not clear what to do with the command() return code */
	(void) command(com);

	return args;
}

/*
 * set <<variablename>> <<variablestring>>
 *
 * Sets variable variablename to string variablestring.
 */
static const char *
cset(cident, args)
	enum cident cident;
	const char *args;
{
	const char *str, *var;

	ARGSTR(var);  /* name of variable to set */
	ARGSTR(str);  /* value to set variable to */
	ENDPARSE;

	if (*var == '_') {
		SETERRSTR (E_BADVAR,
		    "variables beginning with '_' are reserved");
		return NULL;
	}
	if (setvar(var, str))
		return NULL;

	return args;
}

/*
 * macro <<default_number>> <<keys>> <<command>>
 *
 * Associates the macro keys with command.
 */
static const char *
cmacro(cident, args)
	enum cident cident;
	const char *args;
{
	const char *keys, *com;
	long num;

	ARGNUM(num);   /* the default number N for this macro */
	ARGSTR(keys);  /* string of keys representing a macro */
	ARGSTR(com);   /* command line to associate with macro */
	ENDPARSE;

	if (setmacro(keys, com))
		return NULL;
	if (setmacnumb(keys, num))
		return NULL;

	return args;
}

/*
 * error <<string>>
 *
 * Prints a notification message.
 */
static const char *
cerror(cident, args)
	enum cident cident;
	const char *args;
{
	char *s;

	ARGSTR(s);  /* error message */
	ENDPARSE;
	error(s);
	return args;
}

/*
 * condition <<boolean>>
 * condition_! <<boolean>>
 *
 * If boolean is false, causes all commands except for other condition
 * commands to be ignored.  The <<boolean>> may be specified as a number
 * (in which case even numbers are true, odd numbers are false), or one
 * of "on", "off", "true", and "false".
 */
static const char *
ccondition(cident, args)
	enum cident cident;
	const char *args;
{
	/* ENDPARSE; */

	if (cident == CONDITION_TOGGLE) {
		condition_eval = !condition_eval;
		return args;
	}

	switch (gettog(&args, 4, "off", "on", "false", "true")) {
	case 0: case 2:
		condition_eval = 0;
		break;
	case 1: case 3:
		condition_eval = 1;
		break;
	case -1:
		return NULL;
	}
	if (cident == CONDITION_N)
		condition_eval = !condition_eval;
	return args;
}

/*
 * usercom
 *
 * Accept a direct command from the user's terminal.
 */
static const char *
cusercom(cident, args)
	enum cident cident;
	const char *args;
{
	char buf[125];  /* XXX should avoid static buffer... */

	ENDPARSE;
	getinput("Command: ", buf, sizeof(buf));
	if (command(buf))
		return NULL;

	return args;
}

/*
 * readrc <<filename>>
 *
 * Read-in rc commands from the named file.
 */
static const char *
creadrc(cident, args)
	enum cident cident;
	const char *args;
{
	const char *file;
	FILE *fd;

	ARGSTR(file);
	ENDPARSE;

	if (!*file)
		return args;
	/*
	 * Should perhaps warn user if file perms or ownership look suspicious.
	 */
	fd = fopen(file, "r");
	if (!fd) {
		SETERRSTR (E_NULL, "could not open file ``%s''", file);
		return NULL;
	}
	readrc(fd);
	fclose(fd);

	return args;
}

/*
 * quit
 *
 * Performs as advertised.
 */
static const char *
cquit(cident, args)
	enum cident cident;
	const char *args;
{
	ENDPARSE;
	quit();
	return NULL;  /* oh boy... */
}

/*
 * help
 *
 * Doesn't do much.
 */
static const char *
chelp(cident, args)
	enum cident cident;
	const char *args;
{
	extern int ac, curr_ac;
	extern char **av;

	ENDPARSE;
	if (ac > 0 && !strcmp(_PATH_HELPFILE, av[curr_ac])) {
		SETERRSTR(E_NULL, "already viewing help");
		return NULL;
	}
	help();
	return args;
}

/*
 * flush
 * repaint
 *
 * Flushes the file buffer, provided we are not reading from a pipe.
 * Frees any other memory that I can get my hands on from here.
 */
static const char *
cflush(cident, args)
	enum cident cident;
	const char *args;
{
	extern int ispipe;

	ENDPARSE;
	if (cident == FLUSH && !ispipe) {
		ch_init(0, 0);  /* XXX should this be ch_init(ctags,0) */
		clr_linenum();
	}
	repaint();

	return args;
}

/*
 * forw_scroll <<n>>
 * back_scroll <<n>>
 * forw <<n>>
 * back <<n>>
 *
 * Move forward number n lines.  The _scroll variants force a scroll, the
 * others may scroll or may just redraw the screen at the appropriate location,
 * whichever is faster.
 */
static const char *
cscroll(cident, args)
	enum cident cident;
	char *args;
{
	long n;
	char *retr;

	ARGNUM(n);  /* number of lines to move by */
	ENDPARSE;

	switch (cident) {
	case FORW_SCROLL:
		forward(n, 0);
		break;
	case BACK_SCROLL:
		backward(n, 0);
		break;
	case FORW:
		forward(n, 1);
		break;
	case BACK:
		backward(n, 1);
		break;
	}

	return args;
}

/*
 * rscroll <<n>>
 * lscroll <<n>>
 *
 * Scroll left or right by n lines.
 */
static const char *
chscroll(cident, args)
	enum cident cident;
	const char *args;
{
	long n;
	char *retr;
	extern int horiz_off, wraplines;

	ARGNUM(n);  /* Number of columns to scroll by */
	ENDPARSE;

	if (n == 0)
		return args;

	switch (cident) {
	case RSCROLL:
		if (wraplines) {
			wraplines = 0;
			assert (horiz_off == 0);
		} else {
			horiz_off += n;
			if (horiz_off < 0)
				horiz_off = INT_MAX;  /* disaster control */
		}
		break;
	case LSCROLL:
		if (horiz_off != 0) {
			horiz_off -= n;
			if (horiz_off < 0)
				horiz_off = 0;
		} else
			wraplines = 1;
		break;
	}
	repaint();  /* screen_trashed = 1 */
	setvari("_curhscroll", (long) horiz_off);
	settog("_wraplines", wraplines, 2, "off", "on");

	return args;
}

/*
 * goline <<line>>
 * gopercent <<percent>>
 *
 * Goto the line numbered <<line>>, if possible.  Goto <<percent>> percent of
 * the file.  Whole-numbered percents only, of course.
 */
static const char *
cgomisc(cident, args)
	enum cident cident;
	const char *args;
{
	long n;

	ARGNUM(n);  /* number N */
	ENDPARSE;

	switch (cident) {
	case GOLINE:
		jump_back(n);
		break;
	case GOPERCENT:
		if (n > 100)
			n = 100;
		jump_percent(n);
		break;
	}
	return args;
}

/*
 * goend
 *
 * Goto the end of the file.  Future variation should include the GNU less(1)-
 * style follow a-la tail(1).
 */
static const char *
cgoend(cident, args)
	enum cident cident;
	const char *args;
{
	ENDPARSE;
	jump_forw();
	return args;
}

/*
 * edit
 *
 * Edits the current file with a word editor.  This command is just begging
 * to be extended to allow the user to specify an editor.  Additionally, this
 * would require some kind of getenv command or similar change.
 */
static const char *
cedit(cident, args)
	enum cident cident;
	const char *args;
{
	extern ispipe;

	ENDPARSE;
	if (ispipe) {
		SETERRSTR(E_NULL, "cannot edit standard input");
		return NULL;
	}
	editfile();
	/*
	 * XXX less-than brilliant things happen if the user while editing
	 * deletes a large section at the end of the file where we think we
	 * are currently viewing...
	 */
	ch_init(0, 0);  /* Clear the internal file buffer */
	clr_linenum();
	return args;
}

/*
 * askfile
 *
 * Loads a new file.  Queries the user for the name of the new file.
 */
static const char *
caskfile(cident, args)
	enum cident cident;
	const char *args;
{
	char buf[MAXPATHLEN + 1];

	ENDPARSE;
	getinput("Examine: ", buf, sizeof(buf));
	/* XXX should modify this() or edit() to handle lists of file, ie.
	 * the type of lists that I get if I try to glob("*") */
	(void)edit(glob(buf));

	return args;
}

/*
 * file <<next|previous>> <<N>>
 *
 * Loads the N'th next or previous file, typically from the list of files
 * given on the command line.
 */
static const char *
cfile(cident, args)
	enum cident cident;
	const char *args;
{
	enum { FORW=0, BACK=1 } direction;
	long N;

	ARGTOG(direction, 10, "next", "previous", "forward", "backward",
	    "forwards", "backwards", "next", "prev", "forw", "back");
	ARGNUM(N);
	ENDPARSE;
	direction %= 2;

	/* next_file() and prev_file() call error() directly (bad) */
	switch (direction) {
	case FORW:
		next_file(N);
		break;
	case BACK:
		prev_file(N);
		break;
	}
	settog("_file_direction", direction, 2, "next", "previous");
	return args;
}

/*
 * file_list
 *
 * Lists the files the "file next" and "file prev" are moving around in.
 */
static const char *
cfile_list(cident, args)
	enum cident cident;
	const char *args;
{
	ENDPARSE;
	showlist();
	repaint();  /* screen_trashed = 1; */
	return args;
}

/*
 * stat <<on|off>>
 *
 * Display the detailed statistics as part of the prompt.  The toggle option
 * variable is called _stat (giving ${_stat_s} and ${_stat_n}).
 */
static const char *
cstat(cident, args)
	enum cident cident;
	const char *args;
{
	int onoff;

	ARGTOG(onoff, 2, "on", "off");
	ENDPARSE;
	statprompt(onoff);
	settog("_stat", onoff, 2, "on", "off");
	return args;
}

/*
 * magicasksearch <<forw|back>> <<n>>
 * search <<forw|back>> <<n>> <<<noinvert|invert>> <searchstring>>
 * research <<forw|back>> <<n>>
 * 
 * Arguments specifying an option (ie. <<forw|back>> and <<noinvert|invert>>
 * may be specified either as text (eg. "forw"), or as a number, in which case
 * even numbers specify the former setting and odd numbers the latter setting.
 *
 * The magicasksearch will ask the user for a regexp and intuit whether they
 * want to invert the sense of matching or not: if the first character of the
 * regexp is a '!', it is removed and the sense is inverted.  If the regexp
 * entered is null, then we will use ${_ls_regexp} (error if not set).
 *
 * The toggle options are called _ls_direction and _ls_sense.  In addition,
 * ${_ls_regexp} is set to the regexp used.  These variables are only set
 * when the search and magicsearch commands are used.
 */
static const char *
csearch(cident, args)
	enum cident cident;
	const char *args;
{
	char buf[100], *str;
	enum { FORW=0, BACK=1 } direction;
	static enum { NOINVERT=0, INVERT=1 } sense;
	long N;

	ARGTOG(direction, 6, "forw", "back", "forward", "backward",
	    "forwards", "backwards");
	ARGNUM(N);
	if (cident == SEARCH) {
		ARGTOG(sense, 2, "noinvert", "invert");
		ARGSTR(str);
	}
	ENDPARSE;
	direction %= 2;

	/* Get the search string, one way or another */
	switch (cident) {
	case MAGICASKSEARCH:
		biggetinputhack();  /* It's magic, boys */
		if (direction == FORW)
			getinput("Search: /", buf, 2);
		else
			getinput("Search: ?", buf, 2);
		switch (*buf) {
		case '!':
			/* Magic */
			if (direction == FORW)
				getinput("Search: !/", buf, sizeof(buf));
			else
				getinput("Search: !?", buf, sizeof(buf));
			sense = INVERT;
			break;
		default:
			/* No magic */
			ungetcc(*buf);
			if (direction == FORW)
				getinput("Search: /", buf, sizeof(buf));
			else
				getinput("Search: ?", buf, sizeof(buf));
		case '\0':
			sense = NOINVERT;
			break;
		}
		str = buf;
		break;
	case SEARCH:
		break;
	case RESEARCH:
		str = NULL;
		break;
	}

	if (cident == SEARCH || cident == MAGICASKSEARCH) {
		settog("_ls_direction", direction, 2, "forw", "back");
		settog("_ls_sense", sense, 2, "noinvert", "invert");
		if (*str)
			setvar("_ls_regexp", str);
	}
	/*
	 * XXX Currently search() contains magic to deal with (*str=='\0').
	 * This magic should be moved into this function so that we can work
	 * as described in the function comment header.
	 */
	search(!direction, str, N, !sense);
	return args;
}

/*
 * setmark <<character>>
 * gomark <<character>>
 *
 * Set a marker at the current position, or goto a previously set marker.
 * Character may be a-z, or '?' to ask the user to enter a character.  The
 * special mark '\'' may not be set, but may be the target of a goto.
 */
static const char *
cmark(cident, args)
	enum cident cident;
	const char *args;
{
	char smark[2];
	const char *mark;

	ARGSTR(mark);
	ENDPARSE;

	/* gomark() and setmark() will further check mark's validity */
	if (!*mark || mark[1]) {
		SETERRSTR(E_NULL, "bad mark character");
		return NULL;
	}

	if (*mark == '?') {
		biggetinputhack();  /* so getinput() returns after one char */
		switch (cident) {
		case GOMARK:
			getinput("goto mark: ", smark, sizeof smark);
			break;
		case SETMARK:
			getinput("set mark: ", smark, sizeof smark);
			break;
		}
		if (!*smark)
			return args;
		mark = smark;
	}

	switch (cident) {
	case GOMARK:
		gomark(*mark);
		break;
	case SETMARK:
		setmark(*mark);
		break;
	}
	return args;
}

/*
 * asktag
 * nexttag <<number>>
 * prevtag <<number>>
 *
 * Asks the user for a tag, or moves around the tag queue.
 */
static const char *
ctags(cident, args)
	enum cident cident;
	const char *args;
{
	extern char *tagfile;  /* XXX No reason for this to be a global... */
	long n;

	if (cident != ASKFTAG)
		ARGNUM(n);
	ENDPARSE;

	if (cident == ASKFTAG) {
		char buf[100];  /* XXX should do something else... */
		getinput("Tag: ", buf, sizeof(buf));
		if (!*buf)
			return args;
		findtag(buf);
	} else {
		switch (cident) {
		case NEXTFTAG:
			nexttag(n);
			break;
		case PREVFTAG:
			prevtag(n);
			break;
		}
	}

	/* Load the tagfile and position ourselves. */
	if (tagfile == NULL)
		return NULL;
	if (edit(tagfile))
		tagsearch();
	return args;  /* tag stuff still calls error() on its own */
}
