/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Kenneth Almquist.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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

#include <sys/param.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <paths.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
/*
 * Editline and history functions (and glue).
 */
#include "alias.h"
#include "exec.h"
#include "shell.h"
#include "parser.h"
#include "var.h"
#include "options.h"
#include "main.h"
#include "output.h"
#include "mystring.h"
#include "builtins.h"
#ifndef NO_HISTORY
#include "myhistedit.h"
#include "error.h"
#include "eval.h"
#include "memalloc.h"

#define MAXHISTLOOPS	4	/* max recursions through fc */
#define DEFEDITOR	"ed"	/* default editor *should* be $EDITOR */

History *hist;	/* history cookie */
EditLine *el;	/* editline cookie */
int displayhist;
static int savehist;
static FILE *el_in, *el_out;
static bool in_command_completion;

static char *fc_replace(const char *, char *, char *);
static int not_fcnumber(const char *);
static int str_to_event(const char *, int);
static int comparator(const void *, const void *, void *);
static char **sh_matches(const char *, int, int);
static const char *append_char_function(const char *);
static unsigned char sh_complete(EditLine *, int);

static const char *
get_histfile(void)
{
	const char *histfile;

	/* don't try to save if the history size is 0 */
	if (hist == NULL || !strcmp(histsizeval(), "0"))
		return (NULL);
	histfile = expandstr("${HISTFILE-${HOME-}/.sh_history}");

	if (histfile[0] == '\0')
		return (NULL);
	return (histfile);
}

void
histsave(void)
{
	HistEvent he;
	char *histtmpname = NULL;
	const char *histfile;
	int fd;
	FILE *f;

	if (!savehist || (histfile = get_histfile()) == NULL)
		return;
	INTOFF;
	asprintf(&histtmpname, "%s.XXXXXXXXXX", histfile);
	if (histtmpname == NULL) {
		INTON;
		return;
	}
	fd = mkstemp(histtmpname);
	if (fd == -1 || (f = fdopen(fd, "w")) == NULL) {
		free(histtmpname);
		INTON;
		return;
	}
	if (history(hist, &he, H_SAVE_FP, f) < 1 ||
	    rename(histtmpname, histfile) == -1)
		unlink(histtmpname);
	fclose(f);
	free(histtmpname);
	INTON;

}

void
histload(void)
{
	const char *histfile;
	HistEvent he;

	if ((histfile = get_histfile()) == NULL)
		return;
	errno = 0;
	if (history(hist, &he, H_LOAD, histfile) != -1 || errno == ENOENT)
		savehist = 1;
}

/*
 * Set history and editing status.  Called whenever the status may
 * have changed (figures out what to do).
 */
void
histedit(void)
{

#define editing (Eflag || Vflag)

	if (iflag) {
		if (!hist) {
			/*
			 * turn history on
			 */
			INTOFF;
			hist = history_init();
			INTON;

			if (hist != NULL)
				sethistsize(histsizeval());
			else
				out2fmt_flush("sh: can't initialize history\n");
		}
		if (editing && !el && isatty(0)) { /* && isatty(2) ??? */
			/*
			 * turn editing on
			 */
			char *term;

			INTOFF;
			if (el_in == NULL)
				el_in = fdopen(0, "r");
			if (el_out == NULL)
				el_out = fdopen(2, "w");
			if (el_in == NULL || el_out == NULL)
				goto bad;
			term = lookupvar("TERM");
			if (term)
				setenv("TERM", term, 1);
			else
				unsetenv("TERM");
			el = el_init(arg0, el_in, el_out, el_out);
			if (el != NULL) {
				if (hist)
					el_set(el, EL_HIST, history, hist);
				el_set(el, EL_PROMPT_ESC, getprompt, '\001');
				el_set(el, EL_ADDFN, "sh-complete",
				    "Filename completion",
				    sh_complete);
			} else {
bad:
				out2fmt_flush("sh: can't initialize editing\n");
			}
			INTON;
		} else if (!editing && el) {
			INTOFF;
			el_end(el);
			el = NULL;
			INTON;
		}
		if (el) {
			if (Vflag)
				el_set(el, EL_EDITOR, "vi");
			else if (Eflag) {
				el_set(el, EL_EDITOR, "emacs");
			}
			el_set(el, EL_BIND, "^I", "sh-complete", NULL);
			el_source(el, NULL);
		}
	} else {
		INTOFF;
		if (el) {	/* no editing if not interactive */
			el_end(el);
			el = NULL;
		}
		if (hist) {
			history_end(hist);
			hist = NULL;
		}
		INTON;
	}
}


void
sethistsize(const char *hs)
{
	int histsize;
	HistEvent he;

	if (hist != NULL) {
		if (hs == NULL || !is_number(hs))
			histsize = 100;
		else
			histsize = atoi(hs);
		history(hist, &he, H_SETSIZE, histsize);
		history(hist, &he, H_SETUNIQUE, 1);
	}
}

void
setterm(const char *term)
{
	if (rootshell && el != NULL && term != NULL)
		el_set(el, EL_TERMINAL, term);
}

int
histcmd(int argc, char **argv __unused)
{
	const char *editor = NULL;
	HistEvent he;
	int lflg = 0, nflg = 0, rflg = 0, sflg = 0;
	int i, retval;
	const char *firststr, *laststr;
	int first, last, direction;
	char *pat = NULL, *repl = NULL;
	static int active = 0;
	struct jmploc jmploc;
	struct jmploc *savehandler;
	char editfilestr[PATH_MAX];
	char *volatile editfile;
	FILE *efp = NULL;
	int oldhistnum;

	if (hist == NULL)
		error("history not active");

	if (argc == 1)
		error("missing history argument");

	while (not_fcnumber(*argptr))
		do {
			switch (nextopt("e:lnrs")) {
			case 'e':
				editor = shoptarg;
				break;
			case 'l':
				lflg = 1;
				break;
			case 'n':
				nflg = 1;
				break;
			case 'r':
				rflg = 1;
				break;
			case 's':
				sflg = 1;
				break;
			case '\0':
				goto operands;
			}
		} while (nextopt_optptr != NULL);
operands:
	savehandler = handler;
	/*
	 * If executing...
	 */
	if (lflg == 0 || editor || sflg) {
		lflg = 0;	/* ignore */
		editfile = NULL;
		/*
		 * Catch interrupts to reset active counter and
		 * cleanup temp files.
		 */
		if (setjmp(jmploc.loc)) {
			active = 0;
			if (editfile)
				unlink(editfile);
			handler = savehandler;
			longjmp(handler->loc, 1);
		}
		handler = &jmploc;
		if (++active > MAXHISTLOOPS) {
			active = 0;
			displayhist = 0;
			error("called recursively too many times");
		}
		/*
		 * Set editor.
		 */
		if (sflg == 0) {
			if (editor == NULL &&
			    (editor = bltinlookup("FCEDIT", 1)) == NULL &&
			    (editor = bltinlookup("EDITOR", 1)) == NULL)
				editor = DEFEDITOR;
			if (editor[0] == '-' && editor[1] == '\0') {
				sflg = 1;	/* no edit */
				editor = NULL;
			}
		}
	}

	/*
	 * If executing, parse [old=new] now
	 */
	if (lflg == 0 && *argptr != NULL &&
	     ((repl = strchr(*argptr, '=')) != NULL)) {
		pat = *argptr;
		*repl++ = '\0';
		argptr++;
	}
	/*
	 * determine [first] and [last]
	 */
	if (*argptr == NULL) {
		firststr = lflg ? "-16" : "-1";
		laststr = "-1";
	} else if (argptr[1] == NULL) {
		firststr = argptr[0];
		laststr = lflg ? "-1" : argptr[0];
	} else if (argptr[2] == NULL) {
		firststr = argptr[0];
		laststr = argptr[1];
	} else
		error("too many arguments");
	/*
	 * Turn into event numbers.
	 */
	first = str_to_event(firststr, 0);
	last = str_to_event(laststr, 1);

	if (rflg) {
		i = last;
		last = first;
		first = i;
	}
	/*
	 * XXX - this should not depend on the event numbers
	 * always increasing.  Add sequence numbers or offset
	 * to the history element in next (diskbased) release.
	 */
	direction = first < last ? H_PREV : H_NEXT;

	/*
	 * If editing, grab a temp file.
	 */
	if (editor) {
		int fd;
		INTOFF;		/* easier */
		sprintf(editfilestr, "%s/_shXXXXXX", _PATH_TMP);
		if ((fd = mkstemp(editfilestr)) < 0)
			error("can't create temporary file %s", editfile);
		editfile = editfilestr;
		if ((efp = fdopen(fd, "w")) == NULL) {
			close(fd);
			error("Out of space");
		}
	}

	/*
	 * Loop through selected history events.  If listing or executing,
	 * do it now.  Otherwise, put into temp file and call the editor
	 * after.
	 *
	 * The history interface needs rethinking, as the following
	 * convolutions will demonstrate.
	 */
	history(hist, &he, H_FIRST);
	retval = history(hist, &he, H_NEXT_EVENT, first);
	for (;retval != -1; retval = history(hist, &he, direction)) {
		if (lflg) {
			if (!nflg)
				out1fmt("%5d ", he.num);
			out1str(he.str);
		} else {
			const char *s = pat ?
			   fc_replace(he.str, pat, repl) : he.str;

			if (sflg) {
				if (displayhist) {
					out2str(s);
					flushout(out2);
				}
				evalstring(s, 0);
				if (displayhist && hist) {
					/*
					 *  XXX what about recursive and
					 *  relative histnums.
					 */
					oldhistnum = he.num;
					history(hist, &he, H_ENTER, s);
					/*
					 * XXX H_ENTER moves the internal
					 * cursor, set it back to the current
					 * entry.
					 */
					history(hist, &he,
					    H_NEXT_EVENT, oldhistnum);
				}
			} else
				fputs(s, efp);
		}
		/*
		 * At end?  (if we were to lose last, we'd sure be
		 * messed up).
		 */
		if (he.num == last)
			break;
	}
	if (editor) {
		char *editcmd;

		fclose(efp);
		INTON;
		editcmd = stalloc(strlen(editor) + strlen(editfile) + 2);
		sprintf(editcmd, "%s %s", editor, editfile);
		evalstring(editcmd, 0);	/* XXX - should use no JC command */
		readcmdfile(editfile, 0 /* verify */);	/* XXX - should read back - quick tst */
		unlink(editfile);
	}

	if (lflg == 0 && active > 0)
		--active;
	if (displayhist)
		displayhist = 0;
	handler = savehandler;
	return 0;
}

static char *
fc_replace(const char *s, char *p, char *r)
{
	char *dest;
	int plen = strlen(p);

	STARTSTACKSTR(dest);
	while (*s) {
		if (*s == *p && strncmp(s, p, plen) == 0) {
			STPUTS(r, dest);
			s += plen;
			*p = '\0';	/* so no more matches */
		} else
			STPUTC(*s++, dest);
	}
	STPUTC('\0', dest);
	dest = grabstackstr(dest);

	return (dest);
}

static int
not_fcnumber(const char *s)
{
	if (s == NULL)
		return (0);
	if (*s == '-')
		s++;
	return (!is_number(s));
}

static int
str_to_event(const char *str, int last)
{
	HistEvent he;
	const char *s = str;
	int relative = 0;
	int i, retval;

	retval = history(hist, &he, H_FIRST);
	switch (*s) {
	case '-':
		relative = 1;
		/*FALLTHROUGH*/
	case '+':
		s++;
	}
	if (is_number(s)) {
		i = atoi(s);
		if (relative) {
			while (retval != -1 && i--) {
				retval = history(hist, &he, H_NEXT);
			}
			if (retval == -1)
				retval = history(hist, &he, H_LAST);
		} else {
			retval = history(hist, &he, H_NEXT_EVENT, i);
			if (retval == -1) {
				/*
				 * the notion of first and last is
				 * backwards to that of the history package
				 */
				retval = history(hist, &he, last ? H_FIRST : H_LAST);
			}
		}
		if (retval == -1)
			error("history number %s not found (internal error)",
			       str);
	} else {
		/*
		 * pattern
		 */
		retval = history(hist, &he, H_PREV_STR, str);
		if (retval == -1)
			error("history pattern not found: %s", str);
	}
	return (he.num);
}

int
bindcmd(int argc, char **argv)
{
	int ret;
	FILE *old;
	FILE *out;

	if (el == NULL)
		error("line editing is disabled");

	INTOFF;

	out = out1fp();
	if (out == NULL)
		error("Out of space");

	el_get(el, EL_GETFP, 1, &old);
	el_set(el, EL_SETFP, 1, out);

	ret = el_parse(el, argc, __DECONST(const char **, argv));

	el_set(el, EL_SETFP, 1, old);

	fclose(out);

	if (argc > 1 && argv[1][0] == '-' &&
	    memchr("ve", argv[1][1], 2) != NULL) {
		Vflag = argv[1][1] == 'v';
		Eflag = !Vflag;
		histedit();
	}

	INTON;

	return ret;
}

/*
 * Comparator function for qsort(). The use of curpos here is to skip
 * characters that we already know to compare equal (common prefix).
 */
static int
comparator(const void *a, const void *b, void *thunk)
{
	size_t curpos = (intptr_t)thunk;

	return (strcmp(*(char *const *)a + curpos,
		*(char *const *)b + curpos));
}

static char
**add_match(char **matches, size_t i, size_t *size, char *match_copy)
{
	if (match_copy == NULL)
		return (NULL);
	matches[i] = match_copy;
	if (i >= *size - 1) {
		*size *= 2;
		matches = reallocarray(matches, *size, sizeof(matches[0]));
	}

	return (matches);
}

/*
 * This function is passed to libedit's fn_complete2(). The library will use
 * it instead of its standard function that finds matching files in current
 * directory. If we're at the start of the line, we want to look for
 * available commands from all paths in $PATH.
 */
static char
**sh_matches(const char *text, int start, int end)
{
	char *free_path = NULL, *path;
	const char *dirname;
	char **matches = NULL, **rmatches;
	size_t i = 0, size = 16, uniq;
	size_t curpos = end - start, lcstring = -1;
	struct cmdentry e;

	in_command_completion = false;
	if (start > 0 || memchr("/.~", text[0], 3) != NULL)
		return (NULL);
	in_command_completion = true;
	if ((free_path = path = strdup(pathval())) == NULL)
		goto out;
	if ((matches = malloc(size * sizeof(matches[0]))) == NULL)
		goto out;
	while ((dirname = strsep(&path, ":")) != NULL) {
		struct dirent *entry;
		DIR *dir;
		int dfd;

		dir = opendir(dirname[0] == '\0' ? "." : dirname);
		if (dir == NULL)
			continue;
		if ((dfd = dirfd(dir)) == -1) {
			closedir(dir);
			continue;
		}
		while ((entry = readdir(dir)) != NULL) {
			struct stat statb;

			if (strncmp(entry->d_name, text, curpos) != 0)
				continue;
			if (entry->d_type == DT_UNKNOWN || entry->d_type == DT_LNK) {
				if (fstatat(dfd, entry->d_name, &statb, 0) == -1)
					continue;
				if (!S_ISREG(statb.st_mode))
					continue;
			} else if (entry->d_type != DT_REG)
				continue;
			rmatches = add_match(matches, ++i, &size,
				strdup(entry->d_name));
			if (rmatches == NULL) {
				closedir(dir);
				goto out;
			}
			matches = rmatches;
		}
		closedir(dir);
	}
	for (const unsigned char *bp = builtincmd; *bp != 0; bp += 2 + bp[0]) {
		if (curpos > bp[0] || memcmp(bp + 2, text, curpos) != 0)
			continue;
		rmatches = add_match(matches, ++i, &size, strndup(bp + 2, bp[0]));
		if (rmatches == NULL)
			goto out;
		matches = rmatches;
	}
	for (const struct alias *ap = NULL; (ap = iteralias(ap)) != NULL;) {
		if (strncmp(ap->name, text, curpos) != 0)
			continue;
		rmatches = add_match(matches, ++i, &size, strdup(ap->name));
		if (rmatches == NULL)
			goto out;
		matches = rmatches;
	}
	for (const void *a = NULL; (a = itercmd(a, &e)) != NULL;) {
		if (e.cmdtype != CMDFUNCTION)
			continue;
		if (strncmp(e.cmdname, text, curpos) != 0)
			continue;
		rmatches = add_match(matches, ++i, &size, strdup(e.cmdname));
		if (rmatches == NULL)
			goto out;
		matches = rmatches;
	}
out:
	free(free_path);
	if (i == 0) {
		free(matches);
		return (NULL);
	}
	uniq = 1;
	if (i > 1) {
		qsort_s(matches + 1, i, sizeof(matches[0]), comparator,
			(void *)(intptr_t)curpos);
		for (size_t k = 2; k <= i; k++) {
			const char *l = matches[uniq] + curpos;
			const char *r = matches[k] + curpos;
			size_t common = 0;

			while (*l != '\0' && *r != '\0' && *l == *r)
				(void)l++, r++, common++;
			if (common < lcstring)
				lcstring = common;
			if (*l == *r)
				free(matches[k]);
			else
				matches[++uniq] = matches[k];
		}
	}
	matches[uniq + 1] = NULL;
	/*
	 * matches[0] is special: it's not a real matching file name but
	 * a common prefix for all matching names. It can't be null, unlike
	 * any other element of the array. When strings matches[0] and
	 * matches[1] compare equal and matches[2] is null that means to
	 * libedit that there is only a single match. It will then replace
	 * user input with possibly escaped string in matches[0] which is the
	 * reason to copy the full name of the only match.
	 */
	if (uniq == 1)
		matches[0] = strdup(matches[1]);
	else if (lcstring != (size_t)-1)
		matches[0] = strndup(matches[1], curpos + lcstring);
	else
		matches[0] = strdup(text);
	if (matches[0] == NULL) {
		for (size_t k = 1; k <= uniq; k++)
			free(matches[k]);
		free(matches);
		return (NULL);
	}
	return (matches);
}

/*
 * If we don't specify this function as app_func in the call to fn_complete2,
 * libedit will use the default one, which adds a " " to plain files and
 * a "/" to directories regardless of whether it's a command name or a plain
 * path (relative or absolute). We never want to add "/" to commands.
 *
 * For example, after I did "mkdir rmdir", "rmdi" would be autocompleted to
 * "rmdir/" instead of "rmdir ".
 */
static const char *
append_char_function(const char *name)
{
	struct stat stbuf;
	char *expname = name[0] == '~' ? fn_tilde_expand(name) : NULL;
	const char *rs;

	if (!in_command_completion &&
	    stat(expname ? expname : name, &stbuf) == 0 &&
	    S_ISDIR(stbuf.st_mode))
		rs = "/";
	else
		rs = " ";
	free(expname);
	return (rs);
}

/*
 * This is passed to el_set(el, EL_ADDFN, ...) so that it's possible to
 * bind a key (tab by default) to execute the function.
 */
unsigned char
sh_complete(EditLine *sel, int ch __unused)
{
	return (unsigned char)fn_complete2(sel, NULL, sh_matches,
		L" \t\n\"\\'`@$><=;|&{(", NULL, append_char_function,
		(size_t)100, NULL, &((int) {0}), NULL, NULL, FN_QUOTE_MATCH);
}

#else
#include "error.h"

int
histcmd(int argc __unused, char **argv __unused)
{

	error("not compiled with history support");
	/*NOTREACHED*/
	return (0);
}

int
bindcmd(int argc __unused, char **argv __unused)
{

	error("not compiled with line editing support");
	return (0);
}
#endif
