/* $Header: /src/pub/tcsh/tw.parse.c,v 3.92 2002/06/25 19:02:12 christos Exp $ */
/*
 * tw.parse.c: Everyone has taken a shot in this futile effort to
 *	       lexically analyze a csh line... Well we cannot good
 *	       a job as good as sh.lex.c; but we try. Amazing that
 *	       it works considering how many hands have touched this code
 */
/*-
 * Copyright (c) 1980, 1991 The Regents of the University of California.
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
#include "sh.h"

RCSID("$Id: tw.parse.c,v 3.92 2002/06/25 19:02:12 christos Exp $")

#include "tw.h"
#include "ed.h"
#include "tc.h"

#ifdef WINNT_NATIVE
#include "nt.const.h"
#endif /* WINNT_NATIVE */
#define EVEN(x) (((x) & 1) != 1)

#define DOT_NONE	0	/* Don't display dot files		*/
#define DOT_NOT		1	/* Don't display dot or dot-dot		*/
#define DOT_ALL		2	/* Display all dot files		*/

/*  TW_NONE,	       TW_COMMAND,     TW_VARIABLE,    TW_LOGNAME,	*/
/*  TW_FILE,	       TW_DIRECTORY,   TW_VARLIST,     TW_USER,		*/
/*  TW_COMPLETION,     TW_ALIAS,       TW_SHELLVAR,    TW_ENVVAR,	*/
/*  TW_BINDING,        TW_WORDLIST,    TW_LIMIT,       TW_SIGNAL	*/
/*  TW_JOB,	       TW_EXPLAIN,     TW_TEXT,	       TW_GRPNAME	*/
static void (*tw_start_entry[]) __P((DIR *, Char *)) = {
    tw_file_start,     tw_cmd_start,   tw_var_start,   tw_logname_start, 
    tw_file_start,     tw_file_start,  tw_vl_start,    tw_logname_start, 
    tw_complete_start, tw_alias_start, tw_var_start,   tw_var_start,     
    tw_bind_start,     tw_wl_start,    tw_limit_start, tw_sig_start,
    tw_job_start,      tw_file_start,  tw_file_start,  tw_grpname_start
};

static Char * (*tw_next_entry[]) __P((Char *, int *)) = {
    tw_file_next,      tw_cmd_next,    tw_var_next,    tw_logname_next,  
    tw_file_next,      tw_file_next,   tw_var_next,    tw_logname_next,  
    tw_var_next,       tw_var_next,    tw_shvar_next,  tw_envvar_next,   
    tw_bind_next,      tw_wl_next,     tw_limit_next,  tw_sig_next,
    tw_job_next,       tw_file_next,   tw_file_next,   tw_grpname_next
};

static void (*tw_end_entry[]) __P((void)) = {
    tw_dir_end,        tw_dir_end,     tw_dir_end,    tw_logname_end,
    tw_dir_end,        tw_dir_end,     tw_dir_end,    tw_logname_end, 
    tw_dir_end,        tw_dir_end,     tw_dir_end,    tw_dir_end,
    tw_dir_end,        tw_dir_end,     tw_dir_end,    tw_dir_end,
    tw_dir_end,	       tw_dir_end,     tw_dir_end,    tw_grpname_end
};

/* #define TDEBUG */

/* Set to TRUE if recexact is set and an exact match is found
 * along with other, longer, matches.
 */

int curchoice = -1;

int match_unique_match = FALSE;
int non_unique_match = FALSE;
static bool SearchNoDirErr = 0;	/* t_search returns -2 if dir is unreadable */

/* state so if a completion is interrupted, the input line doesn't get
   nuked */
int InsideCompletion = 0;

/* do the expand or list on the command line -- SHOULD BE REPLACED */

extern Char NeedsRedraw;	/* from ed.h */
extern int Tty_raw_mode;
extern int TermH;		/* from the editor routines */
extern int lbuffed;		/* from sh.print.c */

static	void	 extract_dir_and_name	__P((Char *, Char *, Char *));
static	int	 insert_meta		__P((Char *, Char *, Char *, bool));
static	Char	*tilde			__P((Char *, Char *));
#ifndef __MVS__
static  int      expand_dir		__P((Char *, Char *, DIR  **, COMMAND));
#endif
static	bool	 nostat			__P((Char *));
static	Char	 filetype		__P((Char *, Char *));
static	int	 t_glob			__P((Char ***, int));
static	int	 c_glob			__P((Char ***));
static	int	 is_prefix		__P((Char *, Char *));
static	int	 is_prefixmatch		__P((Char *, Char *, int));
static	int	 is_suffix		__P((Char *, Char *));
static	int	 recognize		__P((Char *, Char *, int, int, int));
static	int	 ignored		__P((Char *));
static	int	 isadirectory		__P((Char *, Char *));
#ifndef __MVS__
static  int      tw_collect_items	__P((COMMAND, int, Char *, Char *, 
					     Char *, Char *, int));
static  int      tw_collect		__P((COMMAND, int, Char *, Char *, 
					     Char **, Char *, int, DIR *));
#endif
static	Char 	 tw_suffix		__P((int, Char *, Char *, Char *, 
					     Char *));
static	void 	 tw_fixword		__P((int, Char *, Char *, Char *, int));
static	void	 tw_list_items		__P((int, int, int));
static 	void	 add_scroll_tab		__P((Char *));
static 	void 	 choose_scroll_tab	__P((Char **, int));
static	void	 free_scroll_tab	__P((void));
static	int	 find_rows		__P((Char *[], int, int));

#ifdef notdef
/*
 * If we find a set command, then we break a=b to a= and word becomes
 * b else, we don't break a=b. [don't use that; splits words badly and
 * messes up tw_complete()]
 */
#define isaset(c, w) ((w)[-1] == '=' && \
		      ((c)[0] == 's' && (c)[1] == 'e' && (c)[2] == 't' && \
		       ((c[3] == ' ' || (c)[3] == '\t'))))
#endif

#define QLINESIZE (INBUFSIZE + 1)

/* TRUE if character must be quoted */
#define tricky(w) (cmap(w, _META | _DOL | _QF | _QB | _ESC | _GLOB) && w != '#')
/* TRUE if double quotes don't protect character */
#define tricky_dq(w) (cmap(w, _DOL | _QB))

/* tenematch():
 *	Return:
 *		> 1:    No. of items found
 *		= 1:    Exactly one match / spelling corrected
 *		= 0:    No match / spelling was correct
 *		< 0:    Error (incl spelling correction impossible)
 */
int
tenematch(inputline, num_read, command)
    Char   *inputline;		/* match string prefix */
    int     num_read;		/* # actually in inputline */
    COMMAND command;		/* LIST or RECOGNIZE or PRINT_HELP */

{
    Char    qline[QLINESIZE];
    Char    qu = 0, *pat = STRNULL;
    Char   *str_end, *cp, *wp, *wordp;
    Char   *cmd_start, *word_start, *word;
    Char   *ocmd_start = NULL, *oword_start = NULL, *oword = NULL;
    int	    suf = 0;
    int     space_left;
    int     looking;		/* what we are looking for		*/
    int     search_ret;		/* what search returned for debugging 	*/
    int     backq = 0;

    if (num_read > QLINESIZE - 1)
	return -1;
    str_end = &inputline[num_read];

    word_start = inputline;
    word = cmd_start = wp = qline;
    for (cp = inputline; cp < str_end; cp++) {
        if (!cmap(qu, _ESC)) {
	    if (cmap(*cp, _QF|_ESC)) {
		if (qu == 0 || qu == *cp) {
		    qu ^= *cp;
		    continue;
		}
	    }
	    if (qu != '\'' && cmap(*cp, _QB)) {
		if ((backq ^= 1) != 0) {
		    ocmd_start = cmd_start;
		    oword_start = word_start;
		    oword = word;
		    word_start = cp + 1;
		    word = cmd_start = wp + 1;
		}
		else {
		    cmd_start = ocmd_start;
		    word_start = oword_start;
		    word = oword;
		}
		*wp++ = *cp;
		continue;
	    }
	}
	if (iscmdmeta(*cp))
	    cmd_start = wp + 1;

	/* Don't quote '/' to make the recognize stuff work easily */
	/* Don't quote '$' in double quotes */

	if (cmap(*cp, _ESC) && cp < str_end - 1 && cp[1] == HIST)
	  *wp = *++cp | QUOTE;
	else if (qu && (tricky(*cp) || *cp == '~') && !(qu == '\"' && tricky_dq(*cp)))
	  *wp = *cp | QUOTE;
	else
	  *wp = *cp;
	if (ismetahash(*wp) /* || isaset(cmd_start, wp + 1) */)
	    word = wp + 1, word_start = cp + 1;
	wp++;
	if (cmap(qu, _ESC))
	    qu = 0;
      }
    *wp = 0;

#ifdef masscomp
    /*
     * Avoid a nasty message from the RTU 4.1A & RTU 5.0 compiler concerning
     * the "overuse of registers". According to the compiler release notes,
     * incorrect code may be produced unless the offending expression is
     * rewritten. Therefore, we can't just ignore it, DAS DEC-90.
     */
    space_left = QLINESIZE - 1;
    space_left -= word - qline;
#else
    space_left = QLINESIZE - 1 - (int) (word - qline);
#endif

    /*
     *  SPECIAL HARDCODED COMPLETIONS:
     *    first word of command       -> TW_COMMAND
     *    everything else             -> TW_ZERO
     *
     */
    looking = starting_a_command(word - 1, qline) ? 
	TW_COMMAND : TW_ZERO;

    wordp = word;

#ifdef TDEBUG
    xprintf(CGETS(30, 1, "starting_a_command %d\n"), looking);
    xprintf("\ncmd_start:%S:\n", cmd_start);
    xprintf("qline:%S:\n", qline);
    xprintf("qline:");
    for (wp = qline; *wp; wp++)
	xprintf("%c", *wp & QUOTE ? '-' : ' ');
    xprintf(":\n");
    xprintf("word:%S:\n", word);
    xprintf("word:");
    /* Must be last, so wp is still pointing to the end of word */
    for (wp = word; *wp; wp++)
	xprintf("%c", *wp & QUOTE ? '-' : ' ');
    xprintf(":\n");
#endif

    if ((looking == TW_COMMAND || looking == TW_ZERO) &&
        (command == RECOGNIZE || command == LIST || command == SPELL ||
	 command == RECOGNIZE_SCROLL)) {
#ifdef TDEBUG
	xprintf(CGETS(30, 2, "complete %d "), looking);
#endif
	looking = tw_complete(cmd_start, &wordp, &pat, looking, &suf);
#ifdef TDEBUG
	xprintf(CGETS(30, 3, "complete %d %S\n"), looking, pat);
#endif
    }

    switch (command) {
	Char    buffer[FILSIZ + 1], *bptr;
	Char   *slshp;
	Char   *items[2], **ptr;
	int     i, count;

    case RECOGNIZE:
    case RECOGNIZE_SCROLL:
    case RECOGNIZE_ALL:
	if (adrof(STRautocorrect)) {
	    if ((slshp = Strrchr(wordp, '/')) != NULL && slshp[1] != '\0') {
		SearchNoDirErr = 1;
		for (bptr = wordp; bptr < slshp; bptr++) {
		    /*
		     * do not try to correct spelling of words containing
		     * globbing characters
		     */
		    if (isglob(*bptr)) {
			SearchNoDirErr = 0;
			break;
		    }
		}
	    }
	}
	else
	    slshp = STRNULL;
	search_ret = t_search(wordp, wp, command, space_left, looking, 1, 
			      pat, suf);
	SearchNoDirErr = 0;

	if (search_ret == -2) {
	    Char    rword[FILSIZ + 1];

	    (void) Strcpy(rword, slshp);
	    if (slshp != STRNULL)
		*slshp = '\0';
	    search_ret = spell_me(wordp, QLINESIZE - (wordp - qline), looking,
				  pat, suf);
	    if (search_ret == 1) {
		(void) Strcat(wordp, rword);
		wp = wordp + (int) Strlen(wordp);
		search_ret = t_search(wordp, wp, command, space_left,
				      looking, 1, pat, suf);
	    }
	}
	if (*wp && insert_meta(word_start, str_end, word, !qu) < 0)
	    return -1;		/* error inserting */
	return search_ret;

    case SPELL:
	for (bptr = word_start; bptr < str_end; bptr++) {
	    /*
	     * do not try to correct spelling of words containing globbing
	     * characters
	     */
	    if (isglob(*bptr))
		return 0;
	}
	search_ret = spell_me(wordp, QLINESIZE - (wordp - qline), looking,
			      pat, suf);
	if (search_ret == 1) {
	    if (insert_meta(word_start, str_end, word, !qu) < 0)
		return -1;		/* error inserting */
	}
	return search_ret;

    case PRINT_HELP:
	do_help(cmd_start);
	return 1;

    case GLOB:
    case GLOB_EXPAND:
	(void) Strncpy(buffer, wordp, FILSIZ + 1);
	items[0] = buffer;
	items[1] = NULL;
	ptr = items;
	count = (looking == TW_COMMAND && Strchr(wordp, '/') == 0) ? 
		c_glob(&ptr) : 
		t_glob(&ptr, looking == TW_COMMAND);
	if (count > 0) {
	    if (command == GLOB)
		print_by_column(STRNULL, ptr, count, 0);
	    else {
		DeleteBack(str_end - word_start);/* get rid of old word */
		for (i = 0; i < count; i++)
		    if (ptr[i] && *ptr[i]) {
			(void) quote(ptr[i]);
			if (insert_meta(0, 0, ptr[i], 0) < 0 ||
			    InsertStr(STRspace) < 0) {
			    blkfree(ptr);
			    return -1;		/* error inserting */
			}
		    }
	    }
	    blkfree(ptr);
	}
	return count;

    case VARS_EXPAND:
	if (dollar(buffer, word)) {
	    if (insert_meta(word_start, str_end, buffer, !qu) < 0)
		return -1;		/* error inserting */
	    return 1;
	}
	return 0;

    case PATH_NORMALIZE:
	if ((bptr = dnormalize(wordp, symlinks == SYM_IGNORE ||
				      symlinks == SYM_EXPAND)) != NULL) {
	    (void) Strcpy(buffer, bptr);
	    xfree((ptr_t) bptr);
	    if (insert_meta(word_start, str_end, buffer, !qu) < 0)
		return -1;		/* error inserting */
	    return 1;
	}
	return 0;

    case COMMAND_NORMALIZE:
	if (!cmd_expand(wordp, buffer))
	    return 0;
	if (insert_meta(word_start, str_end, buffer, !qu) < 0)
	    return -1;		/* error inserting */
	return 1;

    case LIST:
    case LIST_ALL:
	search_ret = t_search(wordp, wp, LIST, space_left, looking, 1, 
			      pat, suf);
	return search_ret;

    default:
	xprintf(CGETS(30, 4, "%s: Internal match error.\n"), progname);
	return 1;

    }
} /* end tenematch */


/* t_glob():
 * 	Return a list of files that match the pattern
 */
static int
t_glob(v, cmd)
    register Char ***v;
    int cmd;
{
    jmp_buf_t osetexit;

    if (**v == 0)
	return (0);
    gflag = 0, tglob(*v);
    if (gflag) {
	getexit(osetexit);	/* make sure to come back here */
	if (setexit() == 0)
	    *v = globall(*v);
	resexit(osetexit);
	gargv = 0;
	if (haderr) {
	    haderr = 0;
	    NeedsRedraw = 1;
	    return (-1);
	}
	if (*v == 0)
	    return (0);
    }
    else
	return (0);

    if (cmd) {
	Char **av = *v, *p;
	int fwd, i, ac = gargc;

	for (i = 0, fwd = 0; i < ac; i++) 
	    if (!executable(NULL, av[i], 0)) {
		fwd++;		
		p = av[i];
		av[i] = NULL;
		xfree((ptr_t) p);
	    }
	    else if (fwd) 
		av[i - fwd] = av[i];

	if (fwd)
	    av[i - fwd] = av[i];
	gargc -= fwd;
	av[gargc] = NULL;
    }

    return (gargc);
} /* end t_glob */


/* c_glob():
 * 	Return a list of commands that match the pattern
 */
static int
c_glob(v)
    register Char ***v;
{
    Char *pat = **v, *cmd, **av;
    Char dir[MAXPATHLEN+1];
    int flag, at, ac;

    if (pat == NULL)
	return (0);

    ac = 0;
    at = 10;
    av = (Char **) xmalloc((size_t) (at * sizeof(Char *)));
    av[ac] = NULL;

    tw_cmd_start(NULL, NULL);
    while ((cmd = tw_cmd_next(dir, &flag)) != NULL) 
	if (Gmatch(cmd, pat)) {
	    if (ac + 1 >= at) {
		at += 10;
		av = (Char **) xrealloc((ptr_t) av, 
					(size_t) (at * sizeof(Char *)));
	    }
	    av[ac++] = Strsave(cmd);
	    av[ac] = NULL;
	}
    tw_dir_end();
    *v = av;

    return (ac);
} /* end c_glob */


/* insert_meta():
 *      change the word before the cursor.
 *        cp must point to the start of the unquoted word.
 *        cpend to the end of it.
 *        word is the text that has to be substituted.
 *      strategy:
 *        try to keep all the quote characters of the user's input.
 *        change quote type only if necessary.
 */
static int
insert_meta(cp, cpend, word, closequotes)
    Char   *cp;
    Char   *cpend;
    Char   *word;
    bool    closequotes;
{
    Char buffer[2 * FILSIZ + 1], *bptr, *wptr;
    int in_sync = (cp != NULL);
    int qu = 0;
    int ndel = (int) (cp ? cpend - cp : 0);
    Char w, wq;
#ifdef DSPMBYTE
    int mbytepos = 1;
#endif /* DSPMBYTE */

    for (bptr = buffer, wptr = word;;) {
	if (bptr > buffer + 2 * FILSIZ - 5)
	    break;
	  
	if (cp >= cpend)
	    in_sync = 0;
#ifdef DSPMBYTE
	if (mbytepos == 1)
#endif /* DSPMBYTE */
	if (in_sync && !cmap(qu, _ESC) && cmap(*cp, _QF|_ESC))
	    if (qu == 0 || qu == *cp) {
		qu ^= *cp;
		*bptr++ = *cp++;
		continue;
	    }
	w = *wptr;
	if (w == 0)
	    break;

	wq = w & QUOTE;
	w &= ~QUOTE;

#ifdef DSPMBYTE
	if (mbytepos == 2)
	  goto mbyteskip;
#endif /* DSPMBYTE */
	if (cmap(w, _ESC | _QF))
	    wq = QUOTE;		/* quotes are always quoted */

	if (!wq && qu && tricky(w) && !(qu == '\"' && tricky_dq(w))) {
	    /* We have to unquote the character */
	    in_sync = 0;
	    if (cmap(qu, _ESC))
		bptr[-1] = w;
	    else {
		*bptr++ = (Char) qu;
		*bptr++ = w;
		if (wptr[1] == 0)
		    qu = 0;
		else
		    *bptr++ = (Char) qu;
	    }
	} else if (qu && w == qu) {
	    in_sync = 0;
	    if (bptr > buffer && bptr[-1] == qu) {
		/* User misunderstanding :) */
		bptr[-1] = '\\';
		*bptr++ = w;
		qu = 0;
	    } else {
		*bptr++ = (Char) qu;
		*bptr++ = '\\';
		*bptr++ = w;
		*bptr++ = (Char) qu;
	    }
	}
	else if (wq && qu == '\"' && tricky_dq(w)) {
	    in_sync = 0;
	    *bptr++ = (Char) qu;
	    *bptr++ = '\\';
	    *bptr++ = w;
	    *bptr++ = (Char) qu;
	} else if (wq && ((!qu && (tricky(w) || (w == HISTSUB && bptr == buffer))) || (!cmap(qu, _ESC) && w == HIST))) {
	    in_sync = 0;
	    *bptr++ = '\\';
	    *bptr++ = w;
	} else {
#ifdef DSPMBYTE
	  mbyteskip:
#endif /* DSPMBYTE */
	    if (in_sync && *cp++ != w)
		in_sync = 0;
	    *bptr++ = w;
#ifdef DSPMBYTE
	    if (mbytepos == 1 && Ismbyte1(w))
	      mbytepos = 2;
	    else
	      mbytepos = 1;
#endif /* DSPMBYTE */
	}
	wptr++;
	if (cmap(qu, _ESC))
	    qu = 0;
    }
    if (closequotes && qu && !cmap(qu, _ESC))
	*bptr++ = (Char) qu;
    *bptr = '\0';
    if (ndel)
	DeleteBack(ndel);
    return InsertStr(buffer);
} /* end insert_meta */



/* is_prefix():
 *	return true if check matches initial chars in template
 *	This differs from PWB imatch in that if check is null
 *	it matches anything
 */
static int
is_prefix(check, template)
    register Char *check, *template;
{
    for (; *check; check++, template++)
	if ((*check & TRIM) != (*template & TRIM))
	    return (FALSE);
    return (TRUE);
} /* end is_prefix */


/* is_prefixmatch():
 *	return true if check matches initial chars in template
 *	This differs from PWB imatch in that if check is null
 *	it matches anything
 * and matches on shortening of commands
 */
static int
is_prefixmatch(check, template, igncase)
    Char *check, *template;
    int igncase;
{
    Char MCH1, MCH2;

    for (; *check; check++, template++) {
	if ((*check & TRIM) != (*template & TRIM)) {
            MCH1 = (*check & TRIM);
            MCH2 = (*template & TRIM);
            MCH1 = Isupper(MCH1) ? Tolower(MCH1) : MCH1;
            MCH2 = Isupper(MCH2) ? Tolower(MCH2) : MCH2;
            if (MCH1 != MCH2) {
                if (!igncase && ((*check & TRIM) == '-' || 
				 (*check & TRIM) == '.' ||
				 (*check & TRIM) == '_')) {
                    MCH1 = MCH2 = (*check & TRIM);
                    if (MCH1 == '_') {
                        MCH2 = '-';
                    } else if (MCH1 == '-') {
                        MCH2 = '_';
                    }
                    for (;*template && (*template & TRIM) != MCH1 &&
				       (*template & TRIM) != MCH2; template++)
			continue;
                    if (!*template) {
	                return (FALSE);
                    }
                } else {
	            return (FALSE);
                }
            }
        }
    }
    return (TRUE);
} /* end is_prefixmatch */


/* is_suffix():
 *	Return true if the chars in template appear at the
 *	end of check, I.e., are it's suffix.
 */
static int
is_suffix(check, template)
    register Char *check, *template;
{
    register Char *t, *c;

    for (t = template; *t++;)
	continue;
    for (c = check; *c++;)
	continue;
    for (;;) {
	if (t == template)
	    return 1;
	if (c == check || (*--t & TRIM) != (*--c & TRIM))
	    return 0;
    }
} /* end is_suffix */


/* ignored():
 *	Return true if this is an ignored item
 */
static int
ignored(item)
    register Char *item;
{
    struct varent *vp;
    register Char **cp;

    if ((vp = adrof(STRfignore)) == NULL || (cp = vp->vec) == NULL)
	return (FALSE);
    for (; *cp != NULL; cp++)
	if (is_suffix(item, *cp))
	    return (TRUE);
    return (FALSE);
} /* end ignored */



/* starting_a_command():
 *	return true if the command starting at wordstart is a command
 */
int
starting_a_command(wordstart, inputline)
    register Char *wordstart, *inputline;
{
    register Char *ptr, *ncmdstart;
    int     count;
    static  Char
            cmdstart[] = {'`', ';', '&', '(', '|', '\0'},
            cmdalive[] = {' ', '\t', '\'', '"', '<', '>', '\0'};

    /*
     * Find if the number of backquotes is odd or even.
     */
    for (ptr = wordstart, count = 0;
	 ptr >= inputline;
	 count += (*ptr-- == '`'))
	continue;
    /*
     * if the number of backquotes is even don't include the backquote char in
     * the list of command starting delimiters [if it is zero, then it does not
     * matter]
     */
    ncmdstart = cmdstart + EVEN(count);

    /*
     * look for the characters previous to this word if we find a command
     * starting delimiter we break. if we find whitespace and another previous
     * word then we are not a command
     * 
     * count is our state machine: 0 looking for anything 1 found white-space
     * looking for non-ws
     */
    for (count = 0; wordstart >= inputline; wordstart--) {
	if (*wordstart == '\0')
	    continue;
	if (Strchr(ncmdstart, *wordstart))
	    break;
	/*
	 * found white space
	 */
	if ((ptr = Strchr(cmdalive, *wordstart)) != NULL)
	    count = 1;
	if (count == 1 && !ptr)
	    return (FALSE);
    }

    if (wordstart > inputline)
	switch (*wordstart) {
	case '&':		/* Look for >& */
	    while (wordstart > inputline &&
		   (*--wordstart == ' ' || *wordstart == '\t'))
		continue;
	    if (*wordstart == '>')
		return (FALSE);
	    break;
	case '(':		/* check for foreach, if etc. */
	    while (wordstart > inputline &&
		   (*--wordstart == ' ' || *wordstart == '\t'))
		continue;
	    if (!iscmdmeta(*wordstart) &&
		(*wordstart != ' ' && *wordstart != '\t'))
		return (FALSE);
	    break;
	default:
	    break;
	}
    return (TRUE);
} /* end starting_a_command */


/* recognize():
 *	Object: extend what user typed up to an ambiguity.
 *	Algorithm:
 *	On first match, copy full item (assume it'll be the only match)
 *	On subsequent matches, shorten exp_name to the first
 *	character mismatch between exp_name and item.
 *	If we shorten it back to the prefix length, stop searching.
 */
static int
recognize(exp_name, item, name_length, numitems, enhanced)
    Char   *exp_name, *item;
    int     name_length, numitems, enhanced;
{
    Char MCH1, MCH2;
    register Char *x, *ent;
    register int len = 0;
#ifdef WINNT_NATIVE
    struct varent *vp;
    int igncase;
    igncase = (vp = adrof(STRcomplete)) != NULL && vp->vec != NULL &&
	Strcmp(*(vp->vec), STRigncase) == 0;
#endif /* WINNT_NATIVE */

    if (numitems == 1) {	/* 1st match */
	copyn(exp_name, item, MAXNAMLEN);
	return (0);
    }
    if (!enhanced
#ifdef WINNT_NATIVE
	&& !igncase
#endif /* WINNT_NATIVE */
    ) {
	for (x = exp_name, ent = item; *x && (*x & TRIM) == (*ent & TRIM); x++, ent++)
	    len++;
    } else {
	for (x = exp_name, ent = item; *x; x++, ent++) {
	    MCH1 = *x & TRIM;
	    MCH2 = *ent & TRIM;
            MCH1 = Isupper(MCH1) ? Tolower(MCH1) : MCH1;
            MCH2 = Isupper(MCH2) ? Tolower(MCH2) : MCH2;
	    if (MCH1 != MCH2)
		break;
	    len++;
	}
	if (*x || !*ent)	/* Shorter or exact match */
	    copyn(exp_name, item, MAXNAMLEN);
    }
    *x = '\0';		/* Shorten at 1st char diff */
    if (!(match_unique_match || is_set(STRrecexact) || (enhanced && *ent)) && len == name_length)	/* Ambiguous to prefix? */
	return (-1);	/* So stop now and save time */
    return (0);
} /* end recognize */


/* tw_collect_items():
 *	Collect items that match target.
 *	SPELL command:
 *		Returns the spelling distance of the closest match.
 *	else
 *		Returns the number of items found.
 *		If none found, but some ignored items were found,
 *		It returns the -number of ignored items.
 */
static int
tw_collect_items(command, looking, exp_dir, exp_name, target, pat, flags)
    COMMAND command;
    int looking;
    Char *exp_dir, *exp_name, *target, *pat;
    int flags;

{
    int done = FALSE;			 /* Search is done */
    int showdots;			 /* Style to show dot files */
    int nignored = 0;			 /* Number of fignored items */
    int numitems = 0;			 /* Number of matched items */
    int name_length = (int) Strlen(target); /* Length of prefix (file name) */
    int exec_check = flags & TW_EXEC_CHK;/* need to check executability	*/
    int dir_check  = flags & TW_DIR_CHK; /* Need to check for directories */
    int text_check = flags & TW_TEXT_CHK;/* Need to check for non-directories */
    int dir_ok     = flags & TW_DIR_OK;  /* Ignore directories? */
    int gpat       = flags & TW_PAT_OK;	 /* Match against a pattern */
    int ignoring   = flags & TW_IGN_OK;	 /* Use fignore? */
    int d = 4, nd;			 /* Spelling distance */
    Char *item, *ptr;
    Char buf[MAXPATHLEN+1];
    struct varent *vp;
    int len, enhanced;
    int cnt = 0;
    int igncase = 0;


    flags = 0;

    showdots = DOT_NONE;
    if ((ptr = varval(STRlistflags)) != STRNULL)
	while (*ptr) 
	    switch (*ptr++) {
	    case 'a':
		showdots = DOT_ALL;
		break;
	    case 'A':
		showdots = DOT_NOT;
		break;
	    default:
		break;
	    }

    while (!done && (item = (*tw_next_entry[looking])(exp_dir, &flags))) {
#ifdef TDEBUG
	xprintf("item = %S\n", item);
#endif
	switch (looking) {
	case TW_FILE:
	case TW_DIRECTORY:
	case TW_TEXT:
	    /*
	     * Don't match . files on null prefix match
	     */
	    if (showdots == DOT_NOT && (ISDOT(item) || ISDOTDOT(item)))
		done = TRUE;
	    if (name_length == 0 && item[0] == '.' && showdots == DOT_NONE)
		done = TRUE;
	    break;

	case TW_COMMAND:
	    exec_check = flags & TW_EXEC_CHK;
	    dir_ok = flags & TW_DIR_OK;
	    break;

	default:
	    break;
	}

	if (done) {
	    done = FALSE;
	    continue;
	}

	switch (command) {

	case SPELL:		/* correct the spelling of the last bit */
	    if (name_length == 0) {/* zero-length word can't be misspelled */
		exp_name[0] = '\0';/* (not trying is important for ~) */
		d = 0;
		done = TRUE;
		break;
	    }
	    if (gpat && !Gmatch(item, pat))
		break;
	    /*
	     * Swapped the order of the spdist() arguments as suggested
	     * by eeide@asylum.cs.utah.edu (Eric Eide)
	     */
	    nd = spdist(target, item);	/* test the item against original */
	    if (nd <= d && nd != 4) {
		if (!(exec_check && !executable(exp_dir, item, dir_ok))) {
		    (void) Strcpy(exp_name, item);
		    d = nd;
		    if (d == 0)	/* if found it exactly */
			done = TRUE;
		}
	    }
	    else if (nd == 4) {
		if (spdir(exp_name, exp_dir, item, target)) {
		    if (exec_check && !executable(exp_dir, exp_name, dir_ok)) 
			break;
#ifdef notdef
		    /*
		     * We don't want to stop immediately, because
		     * we might find an exact/better match later.
		     */
		    d = 0;
		    done = TRUE;
#endif
		    d = 3;
		}
	    }
	    break;

	case LIST:
	case RECOGNIZE:
	case RECOGNIZE_ALL:
	case RECOGNIZE_SCROLL:

#ifdef WINNT_NATIVE
 	    igncase = (vp = adrof(STRcomplete)) != NULL && vp->vec != NULL &&
		Strcmp(*(vp->vec), STRigncase) == 0;
#endif /* WINNT_NATIVE */
	    enhanced = (vp = adrof(STRcomplete)) != NULL && !Strcmp(*(vp->vec),STRenhance);
	    if (enhanced || igncase) {
	        if (!is_prefixmatch(target, item, igncase)) 
		    break;
     	    } else {
	        if (!is_prefix(target, item)) 
		    break;
	    }

	    if (exec_check && !executable(exp_dir, item, dir_ok))
		break;

	    if (dir_check && !isadirectory(exp_dir, item))
		break;

	    if (text_check && isadirectory(exp_dir, item))
		break;

	    /*
	     * Only pattern match directories if we're checking
	     * for directories.
	     */
	    if (gpat && !Gmatch(item, pat) &&
		(dir_check || !isadirectory(exp_dir, item)))
		    break;

	    /*
	     * Remove duplicates in command listing and completion
             * AFEB added code for TW_LOGNAME and TW_USER cases
	     */
	    if (looking == TW_COMMAND || looking == TW_LOGNAME
		|| looking == TW_USER || command == LIST) {
		copyn(buf, item, MAXPATHLEN);
		len = (int) Strlen(buf);
		switch (looking) {
		case TW_COMMAND:
		    if (!(dir_ok && exec_check))
			break;
		    if (filetype(exp_dir, item) == '/') {
			buf[len++] = '/';
			buf[len] = '\0';
		    }
		    break;

		case TW_FILE:
		case TW_DIRECTORY:
		    buf[len++] = filetype(exp_dir, item);
		    buf[len] = '\0';
		    break;

		default:
		    break;
		}
		if ((looking == TW_COMMAND || looking == TW_USER
                     || looking == TW_LOGNAME) && tw_item_find(buf))
		    break;
		else {
		    /* maximum length 1 (NULL) + 1 (~ or $) + 1 (filetype) */
		    ptr = tw_item_add(len + 3);
		    copyn(ptr, buf, MAXPATHLEN);
		    if (command == LIST)
			numitems++;
		}
	    }
		    
	    if (command == RECOGNIZE || command == RECOGNIZE_ALL ||
		command == RECOGNIZE_SCROLL) {
		if (ignoring && ignored(item)) {
		    nignored++;
		    break;
		} 
		else if (command == RECOGNIZE_SCROLL) {
		    add_scroll_tab(item);
		    cnt++;
		}
		
		if (match_unique_match || is_set(STRrecexact)) {
		    if (StrQcmp(target, item) == 0) {	/* EXACT match */
			copyn(exp_name, item, MAXNAMLEN);
			numitems = 1;	/* fake into expanding */
			non_unique_match = TRUE;
			done = TRUE;
			break;
		    }
		}
		if (recognize(exp_name, item, name_length, ++numitems, enhanced)) 
		    if (command != RECOGNIZE_SCROLL)
			done = TRUE;
		if (enhanced && (int)Strlen(exp_name) < name_length)
		    copyn(exp_name, target, MAXNAMLEN);
	    }
	    break;

	default:
	    break;
	}
#ifdef TDEBUG
	xprintf("done item = %S\n", item);
#endif
    }


    if (command == RECOGNIZE_SCROLL) {
	if ((cnt <= curchoice) || (curchoice == -1)) {
	    curchoice = -1;
	    nignored = 0;
	    numitems = 0;
	} else if (numitems > 1) {
	    if (curchoice < -1)
		curchoice = cnt - 1;
	    choose_scroll_tab(&exp_name, cnt);
	    numitems = 1;
	}
    }
    free_scroll_tab();

    if (command == SPELL)
	return d;
    else {
	if (ignoring && numitems == 0 && nignored > 0) 
	    return -nignored;
	else
	    return numitems;
    }
}


/* tw_suffix():
 *	Find and return the appropriate suffix character
 */
/*ARGSUSED*/
static Char 
tw_suffix(looking, exp_dir, exp_name, target, name)
    int looking;
    Char *exp_dir, *exp_name, *target, *name;
{    
    Char *ptr;
    struct varent *vp;

    USE(name);
    (void) strip(exp_name);

    switch (looking) {

    case TW_LOGNAME:
	return '/';

    case TW_VARIABLE:
	/*
	 * Don't consider array variables or empty variables
	 */
	if ((vp = adrof(exp_name)) != NULL && vp->vec != NULL) {
	    if ((ptr = vp->vec[0]) == NULL || *ptr == '\0' ||
		vp->vec[1] != NULL) 
		return ' ';
	}
	else if ((ptr = tgetenv(exp_name)) == NULL || *ptr == '\0')
	    return ' ';

	*--target = '\0';

	return isadirectory(exp_dir, ptr) ? '/' : ' ';


    case TW_DIRECTORY:
	return '/';

    case TW_COMMAND:
    case TW_FILE:
	return isadirectory(exp_dir, exp_name) ? '/' : ' ';

    case TW_ALIAS:
    case TW_VARLIST:
    case TW_WORDLIST:
    case TW_SHELLVAR:
    case TW_ENVVAR:
    case TW_USER:
    case TW_BINDING:
    case TW_LIMIT:
    case TW_SIGNAL:
    case TW_JOB:
    case TW_COMPLETION:
    case TW_TEXT:
    case TW_GRPNAME:
	return ' ';

    default:
	return '\0';
    }
} /* end tw_suffix */


/* tw_fixword():
 *	Repair a word after a spalling or a recognizwe
 */
static void
tw_fixword(looking, word, dir, exp_name, max_word_length)
    int looking;
    Char *word, *dir, *exp_name;
    int max_word_length;
{
    Char *ptr;

    switch (looking) {
    case TW_LOGNAME:
	copyn(word, STRtilde, 1);
	break;
    
    case TW_VARIABLE:
	if ((ptr = Strrchr(word, '$')) != NULL)
	    *++ptr = '\0';	/* Delete after the dollar */
	else
	    word[0] = '\0';
	break;

    case TW_DIRECTORY:
    case TW_FILE:
    case TW_TEXT:
	copyn(word, dir, max_word_length);	/* put back dir part */
	break;

    default:
	word[0] = '\0';
	break;
    }

    (void) quote(exp_name);
    catn(word, exp_name, max_word_length);	/* add extended name */
} /* end tw_fixword */


/* tw_collect():
 *	Collect items. Return -1 in case we were interrupted or
 *	the return value of tw_collect
 *	This is really a wrapper for tw_collect_items, serving two
 *	purposes:
 *		1. Handles interrupt cleanups.
 *		2. Retries if we had no matches, but there were ignored matches
 */
static int
tw_collect(command, looking, exp_dir, exp_name, target, pat, flags, dir_fd)
    COMMAND command;
    int looking;
    Char *exp_dir, *exp_name, **target, *pat;
    int flags;
    DIR *dir_fd;
{
    static int ni;	/* static so we don't get clobbered */
    jmp_buf_t osetexit;

#ifdef TDEBUG
    xprintf("target = %S\n", *target);
#endif
    ni = 0;
    getexit(osetexit);
    for (;;) {
	(*tw_start_entry[looking])(dir_fd, pat);
	InsideCompletion = 1;
	if (setexit()) {
	    /* interrupted, clean up */
	    resexit(osetexit);
	    InsideCompletion = 0;
	    haderr = 0;

#if defined(SOLARIS2) && defined(i386) && !defined(__GNUC__)
	    /* Compiler bug? (from PWP) */
	    if ((looking == TW_LOGNAME) || (looking == TW_USER))
		tw_logname_end();
	    else
		if (looking == TW_GRPNAME)
		   tw_grpname_end();
		else
		    tw_dir_end();
#else /* !(SOLARIS2 && i386 && !__GNUC__) */
	    (*tw_end_entry[looking])();
#endif /* !(SOLARIS2 && i386 && !__GNUC__) */

	    /* flag error */
	    return(-1);
	}
        if ((ni = tw_collect_items(command, looking, exp_dir, exp_name,
			           *target, pat, 
				   ni >= 0 ? flags : 
					flags & ~TW_IGN_OK)) >= 0) {
	    resexit(osetexit);
	    InsideCompletion = 0;

#if defined(SOLARIS2) && defined(i386) && !defined(__GNUC__)
	    /* Compiler bug? (from PWP) */
	    if ((looking == TW_LOGNAME) || (looking == TW_USER))
		tw_logname_end();
	    else
		if (looking == TW_GRPNAME)
		   tw_grpname_end();
		else
		    tw_dir_end();
#else /* !(SOLARIS2 && i386 && !__GNUC__) */
	    (*tw_end_entry[looking])();
#endif /* !(SOLARIS2 && i386 && !__GNUC__) */

	    return(ni);
	}
    }
} /* end tw_collect */


/* tw_list_items():
 *	List the items that were found
 *
 *	NOTE instead of looking at numerical vars listmax and listmaxrows
 *	we can look at numerical var listmax, and have a string value
 *	listmaxtype (or similar) than can have values 'items' and 'rows'
 *	(by default interpreted as 'items', for backwards compatibility)
 */
static void
tw_list_items(looking, numitems, list_max)
    int looking, numitems, list_max;
{
    Char *ptr;
    int max_items = 0;
    int max_rows = 0;

    if (numitems == 0)
	return;

    if ((ptr = varval(STRlistmax)) != STRNULL) {
	while (*ptr) {
	    if (!Isdigit(*ptr)) {
		max_items = 0;
		break;
	    }
	    max_items = max_items * 10 + *ptr++ - '0';
	}
	if ((max_items > 0) && (numitems > max_items) && list_max)
	    max_items = numitems;
	else
	    max_items = 0;
    }

    if (max_items == 0 && (ptr = varval(STRlistmaxrows)) != STRNULL) {
	int rows;

	while (*ptr) {
	    if (!Isdigit(*ptr)) {
		max_rows = 0;
		break;
	    }
	    max_rows = max_rows * 10 + *ptr++ - '0';
	}
	if (max_rows != 0 && looking != TW_JOB)
	    rows = find_rows(tw_item_get(), numitems, TRUE);
	else
	    rows = numitems; /* underestimate for lines wider than the termH */
	if ((max_rows > 0) && (rows > max_rows) && list_max)
	    max_rows = rows;
	else
	    max_rows = 0;
    }


    if (max_items || max_rows) {
	char    	 tc;
	const char	*name;
	int maxs;

	if (max_items) {
	    name = CGETS(30, 5, "items");
	    maxs = max_items;
	}
	else {
	    name = CGETS(30, 6, "rows");
	    maxs = max_rows;
	}

	xprintf(CGETS(30, 7, "There are %d %s, list them anyway? [n/y] "),
		maxs, name);
	flush();
	/* We should be in Rawmode here, so no \n to catch */
	(void) read(SHIN, &tc, 1);
	xprintf("%c\r\n", tc);	/* echo the char, do a newline */
	/* 
	 * Perhaps we should use the yesexpr from the
	 * actual locale
	 */
	if (strchr(CGETS(30, 13, "Yy"), tc) == NULL)
	    return;
    }

    if (looking != TW_SIGNAL)
	qsort((ptr_t) tw_item_get(), (size_t) numitems, sizeof(Char *), 
	      (int (*) __P((const void *, const void *))) fcompare);
    if (looking != TW_JOB)
	print_by_column(STRNULL, tw_item_get(), numitems, TRUE);
    else {
	/*
	 * print one item on every line because jobs can have spaces
	 * and it is confusing.
	 */
	int i;
	Char **w = tw_item_get();

	for (i = 0; i < numitems; i++) {
	    xprintf("%S", w[i]);
	    if (Tty_raw_mode)
		xputchar('\r');
	    xputchar('\n');
	}
    }
} /* end tw_list_items */


/* t_search():
 *	Perform a RECOGNIZE, LIST or SPELL command on string "word".
 *
 *	Return value:
 *		>= 0:   SPELL command: "distance" (see spdist())
 *		                other: No. of items found
 *  		 < 0:   Error (message or beep is output)
 */
/*ARGSUSED*/
int
t_search(word, wp, command, max_word_length, looking, list_max, pat, suf)
    Char   *word, *wp;		/* original end-of-word */
    COMMAND command;
    int     max_word_length, looking, list_max;
    Char   *pat;
    int     suf;
{
    int     numitems,			/* Number of items matched */
	    flags = 0,			/* search flags */
	    gpat = pat[0] != '\0',	/* Glob pattern search */
	    nd;				/* Normalized directory return */
    Char    exp_dir[FILSIZ + 1],	/* dir after ~ expansion */
            dir[FILSIZ + 1],		/* /x/y/z/ part in /x/y/z/f */
            exp_name[MAXNAMLEN + 1],	/* the recognized (extended) */
            name[MAXNAMLEN + 1],	/* f part in /d/d/d/f name */
           *target;			/* Target to expand/correct/list */
    DIR    *dir_fd = NULL;	

    USE(wp);

    /*
     * bugfix by Marty Grossman (grossman@CC5.BBN.COM): directory listing can
     * dump core when interrupted
     */
    tw_item_free();

    non_unique_match = FALSE;	/* See the recexact code below */

    extract_dir_and_name(word, dir, name);

    /*
     *  SPECIAL HARDCODED COMPLETIONS:
     *    foo$variable                -> TW_VARIABLE
     *    ~user                       -> TW_LOGNAME
     *
     */
    if ((*word == '~') && (Strchr(word, '/') == NULL)) {
	looking = TW_LOGNAME;
	target = name;
	gpat = 0;	/* Override pattern mechanism */
    }
    else if ((target = Strrchr(name, '$')) != 0 && 
	     (Strchr(name, '/') == NULL)) {
	target++;
	looking = TW_VARIABLE;
	gpat = 0;	/* Override pattern mechanism */
    }
    else
	target = name;

    /*
     * Try to figure out what we should be looking for
     */
    if (looking & TW_PATH) {
	gpat = 0;	/* pattern holds the pathname to be used */
	copyn(exp_dir, pat, MAXNAMLEN);
	if (exp_dir[Strlen(exp_dir) - 1] != '/')
	    catn(exp_dir, STRslash, MAXNAMLEN);
	catn(exp_dir, dir, MAXNAMLEN);
    }
    else
	exp_dir[0] = '\0';

    switch (looking & ~TW_PATH) {
    case TW_NONE:
	return -1;

    case TW_ZERO:
	looking = TW_FILE;
	break;

    case TW_COMMAND:
	if (Strchr(word, '/') || (looking & TW_PATH)) {
	    looking = TW_FILE;
	    flags |= TW_EXEC_CHK;
	    flags |= TW_DIR_OK;
	}
#ifdef notdef
	/* PWP: don't even bother when doing ALL of the commands */
	if (looking == TW_COMMAND && (*word == '\0')) 
	    return (-1);
#endif
	break;


    case TW_VARLIST:
    case TW_WORDLIST:
	gpat = 0;	/* pattern holds the name of the variable */
	break;

    case TW_EXPLAIN:
	if (command == LIST && pat != NULL) {
	    xprintf("%S", pat);
	    if (Tty_raw_mode)
		xputchar('\r');
	    xputchar('\n');
	}
	return 2;

    default:
	break;
    }

    /*
     * let fignore work only when we are not using a pattern
     */
    flags |= (gpat == 0) ? TW_IGN_OK : TW_PAT_OK;

#ifdef TDEBUG
    xprintf(CGETS(30, 8, "looking = %d\n"), looking);
#endif

    switch (looking) {
    case TW_ALIAS:
    case TW_SHELLVAR:
    case TW_ENVVAR:
    case TW_BINDING:
    case TW_LIMIT:
    case TW_SIGNAL:
    case TW_JOB:
    case TW_COMPLETION:
    case TW_GRPNAME:
	break;


    case TW_VARIABLE:
	if ((nd = expand_dir(dir, exp_dir, &dir_fd, command)) != 0)
	    return nd;
	break;

    case TW_DIRECTORY:
	flags |= TW_DIR_CHK;

#ifdef notyet
	/*
	 * This is supposed to expand the directory stack.
	 * Problems:
	 * 1. Slow
	 * 2. directories with the same name
	 */
	flags |= TW_DIR_OK;
#endif
#ifdef notyet
	/*
	 * Supposed to do delayed expansion, but it is inconsistent
	 * from a user-interface point of view, since it does not
	 * immediately obey addsuffix
	 */
	if ((nd = expand_dir(dir, exp_dir, &dir_fd, command)) != 0)
	    return nd;
	if (isadirectory(exp_dir, name)) {
	    if (exp_dir[0] != '\0' || name[0] != '\0') {
		catn(dir, name, MAXNAMLEN);
		if (dir[Strlen(dir) - 1] != '/')
		    catn(dir, STRslash, MAXNAMLEN);
		if ((nd = expand_dir(dir, exp_dir, &dir_fd, command)) != 0)
		    return nd;
		if (word[Strlen(word) - 1] != '/')
		    catn(word, STRslash, MAXNAMLEN);
		name[0] = '\0';
	    }
	}
#endif
	if ((nd = expand_dir(dir, exp_dir, &dir_fd, command)) != 0)
	    return nd;
	break;

    case TW_TEXT:
	flags |= TW_TEXT_CHK;
	/*FALLTHROUGH*/
    case TW_FILE:
	if ((nd = expand_dir(dir, exp_dir, &dir_fd, command)) != 0)
	    return nd;
	break;

    case TW_PATH | TW_TEXT:
    case TW_PATH | TW_FILE:
    case TW_PATH | TW_DIRECTORY:
    case TW_PATH | TW_COMMAND:
	if ((dir_fd = opendir(short2str(exp_dir))) == NULL) {
 	    if (command == RECOGNIZE)
 		xprintf("\n");
 	    xprintf("%S: %s", exp_dir, strerror(errno));
 	    if (command != RECOGNIZE)
 		xprintf("\n");
 	    NeedsRedraw = 1;
	    return -1;
	}
	if (exp_dir[Strlen(exp_dir) - 1] != '/')
	    catn(exp_dir, STRslash, MAXNAMLEN);

	looking &= ~TW_PATH;

	switch (looking) {
	case TW_TEXT:
	    flags |= TW_TEXT_CHK;
	    break;

	case TW_FILE:
	    break;

	case TW_DIRECTORY:
	    flags |= TW_DIR_CHK;
	    break;

	case TW_COMMAND:
	    copyn(target, word, MAXNAMLEN);	/* so it can match things */
	    break;

	default:
	    abort();	/* Cannot happen */
	    break;
	}
	break;

    case TW_LOGNAME:
	word++;
	/*FALLTHROUGH*/
    case TW_USER:
	/*
	 * Check if the spelling was already correct
	 * From: Rob McMahon <cudcv@cu.warwick.ac.uk>
	 */
	if (command == SPELL && getpwnam(short2str(word)) != NULL) {
#ifdef YPBUGS
	    fix_yp_bugs();
#endif /* YPBUGS */
	    return (0);
	}
	copyn(name, word, MAXNAMLEN);	/* name sans ~ */
	if (looking == TW_LOGNAME)
	    word--;
	break;

    case TW_COMMAND:
    case TW_VARLIST:
    case TW_WORDLIST:
	copyn(target, word, MAXNAMLEN);	/* so it can match things */
	break;

    default:
	xprintf(CGETS(30, 9,
		"\n%s internal error: I don't know what I'm looking for!\n"),
		progname);
	NeedsRedraw = 1;
	return (-1);
    }

    numitems = tw_collect(command, looking, exp_dir, exp_name, 
			  &target, pat, flags, dir_fd);
    if (numitems == -1)
	return -1;

    switch (command) {
    case RECOGNIZE:
    case RECOGNIZE_ALL:
    case RECOGNIZE_SCROLL:
	if (numitems <= 0) 
	    return (numitems);

	tw_fixword(looking, word, dir, exp_name, max_word_length);

	if (!match_unique_match && is_set(STRaddsuffix) && numitems == 1) {
	    Char suffix[2];

	    suffix[1] = '\0';
	    switch (suf) {
	    case 0: 	/* Automatic suffix */
		suffix[0] = tw_suffix(looking, exp_dir, exp_name, target, name);
		break;

	    case -1:	/* No suffix */
		return numitems;

	    default:	/* completion specified suffix */
		suffix[0] = (Char) suf;
		break;
	    }
	    catn(word, suffix, max_word_length);
	}
	return numitems;

    case LIST:
	tw_list_items(looking, numitems, list_max);
	tw_item_free();
	return (numitems);

    case SPELL:
	tw_fixword(looking, word, dir, exp_name, max_word_length);
	return (numitems);

    default:
	xprintf("Bad tw_command\n");
	return (0);
    }
} /* end t_search */


/* extract_dir_and_name():
 * 	parse full path in file into 2 parts: directory and file names
 * 	Should leave final slash (/) at end of dir.
 */
static void
extract_dir_and_name(path, dir, name)
    Char   *path, *dir, *name;
{
    register Char *p;

    p = Strrchr(path, '/');
#ifdef WINNT_NATIVE
    if (p == NULL)
	p = Strrchr(path, ':');
#endif /* WINNT_NATIVE */
    if (p == NULL) {
	copyn(name, path, MAXNAMLEN);
	dir[0] = '\0';
    }
    else {
	p++;
	copyn(name, p, MAXNAMLEN);
	copyn(dir, path, p - path);
    }
} /* end extract_dir_and_name */


/* dollar():
 * 	expand "/$old1/$old2/old3/"
 * 	to "/value_of_old1/value_of_old2/old3/"
 */
Char *
dollar(new, old)
    Char   *new;
    const Char *old;
{
    Char    *p;
    size_t   space;

    for (space = FILSIZ, p = new; *old && space > 0;)
	if (*old != '$') {
	    *p++ = *old++;
	    space--;
	}
	else {
	    if (expdollar(&p, &old, &space, QUOTE) == NULL)
		return NULL;
	}
    *p = '\0';
    return (new);
} /* end dollar */


/* tilde():
 * 	expand ~person/foo to home_directory_of_person/foo
 *	or =<stack-entry> to <dir in stack entry>
 */
static Char *
tilde(new, old)
    Char   *new, *old;
{
    register Char *o, *p;

    switch (old[0]) {
    case '~':
	for (p = new, o = &old[1]; *o && *o != '/'; *p++ = *o++) 
	    continue;
	*p = '\0';
	if (gethdir(new)) {
	    new[0] = '\0';
	    return NULL;
	}
#ifdef apollo
	/* Special case: if the home directory expands to "/", we do
	 * not want to create "//" by appending a slash from o.
	 */
	if (new[0] == '/' && new[1] == '\0' && *o == '/')
	    ++o;
#endif /* apollo */
	(void) Strcat(new, o);
	return new;

    case '=':
	if ((p = globequal(new, old)) == NULL) {
	    *new = '\0';
	    return NULL;
	}
	if (p == new)
	    return new;
	/*FALLTHROUGH*/

    default:
	(void) Strcpy(new, old);
	return new;
    }
} /* end tilde */


/* expand_dir():
 *	Open the directory given, expanding ~user and $var
 *	Optionally normalize the path given
 */
static int
expand_dir(dir, edir, dfd, cmd)
    Char   *dir, *edir;
    DIR   **dfd;
    COMMAND cmd;
{
    Char   *nd = NULL;
    Char    tdir[MAXPATHLEN + 1];

    if ((dollar(tdir, dir) == 0) ||
	(tilde(edir, tdir) == 0) ||
	!(nd = dnormalize(*edir ? edir : STRdot, symlinks == SYM_IGNORE ||
						 symlinks == SYM_EXPAND)) ||
	((*dfd = opendir(short2str(nd))) == NULL)) {
	xfree((ptr_t) nd);
	if (cmd == SPELL || SearchNoDirErr)
	    return (-2);
	/*
	 * From: Amos Shapira <amoss@cs.huji.ac.il>
	 * Print a better message when completion fails
	 */
	xprintf("\n%S %s\n",
		*edir ? edir :
		(*tdir ? tdir : dir),
		(errno == ENOTDIR ? CGETS(30, 10, "not a directory") :
		(errno == ENOENT ? CGETS(30, 11, "not found") :
		 CGETS(30, 12, "unreadable"))));
	NeedsRedraw = 1;
	return (-1);
    }
    if (nd) {
	if (*dir != '\0') {
	    Char   *s, *d, *p;

	    /*
	     * Copy and append a / if there was one
	     */
	    for (p = edir; *p; p++)
		continue;
	    if (*--p == '/') {
		for (p = nd; *p; p++)
		    continue;
		if (*--p != '/')
		    p = NULL;
	    }
	    for (d = edir, s = nd; (*d++ = *s++) != '\0';)
		continue;
	    if (!p) {
		*d-- = '\0';
		*d = '/';
	    }
	}
	xfree((ptr_t) nd);
    }
    return 0;
} /* end expand_dir */


/* nostat():
 *	Returns true if the directory should not be stat'd,
 *	false otherwise.
 *	This way, things won't grind to a halt when you complete in /afs
 *	or very large directories.
 */
static bool
nostat(dir)
     Char *dir;
{
    struct varent *vp;
    register Char **cp;

    if ((vp = adrof(STRnostat)) == NULL || (cp = vp->vec) == NULL)
	return FALSE;
    for (; *cp != NULL; cp++) {
	if (Strcmp(*cp, STRstar) == 0)
	    return TRUE;
	if (Gmatch(dir, *cp))
	    return TRUE;
    }
    return FALSE;
} /* end nostat */


/* filetype():
 *	Return a character that signifies a filetype
 *	symbology from 4.3 ls command.
 */
static  Char
filetype(dir, file)
    Char   *dir, *file;
{
    if (dir) {
	Char    path[512];
	char   *ptr;
	struct stat statb;
#ifdef S_ISCDF
	/* 
	 * From: veals@crchh84d.bnr.ca (Percy Veals)
	 * An extra stat is required for HPUX CDF files.
	 */
	struct stat hpstatb;
#endif /* S_ISCDF */

	if (nostat(dir)) return(' ');

	(void) Strcpy(path, dir);
	catn(path, file, (int) (sizeof(path) / sizeof(Char)));

	if (lstat(ptr = short2str(path), &statb) != -1)
	    /* see above #define of lstat */
	{
#ifdef S_ISLNK
	    if (S_ISLNK(statb.st_mode)) {	/* Symbolic link */
		if (adrof(STRlistlinks)) {
		    if (stat(ptr, &statb) == -1)
			return ('&');
		    else if (S_ISDIR(statb.st_mode))
			return ('>');
		    else
			return ('@');
		}
		else
		    return ('@');
	    }
#endif
#ifdef S_ISSOCK
	    if (S_ISSOCK(statb.st_mode))	/* Socket */
		return ('=');
#endif
#ifdef S_ISFIFO
	    if (S_ISFIFO(statb.st_mode)) /* Named Pipe */
		return ('|');
#endif
#ifdef S_ISHIDDEN
	    if (S_ISHIDDEN(statb.st_mode)) /* Hidden Directory [aix] */
		return ('+');
#endif
#ifdef S_ISCDF	
	    (void) strcat(ptr, "+");	/* Must append a '+' and re-stat(). */
	    if ((stat(ptr, &hpstatb) != -1) && S_ISCDF(hpstatb.st_mode))
	    	return ('+');		/* Context Dependent Files [hpux] */
#endif 
#ifdef S_ISNWK
	    if (S_ISNWK(statb.st_mode)) /* Network Special [hpux] */
		return (':');
#endif
#ifdef S_ISCHR
	    if (S_ISCHR(statb.st_mode))	/* char device */
		return ('%');
#endif
#ifdef S_ISBLK
	    if (S_ISBLK(statb.st_mode))	/* block device */
		return ('#');
#endif
#ifdef S_ISDIR
	    if (S_ISDIR(statb.st_mode))	/* normal Directory */
		return ('/');
#endif
	    if (statb.st_mode & (S_IXUSR|S_IXGRP|S_IXOTH))
		return ('*');
	}
    }
    return (' ');
} /* end filetype */


/* isadirectory():
 *	Return trus if the file is a directory
 */
static int
isadirectory(dir, file)		/* return 1 if dir/file is a directory */
    Char   *dir, *file;		/* uses stat rather than lstat to get dest. */
{
    if (dir) {
	Char    path[MAXPATHLEN];
	struct stat statb;

	(void) Strcpy(path, dir);
	catn(path, file, (int) (sizeof(path) / sizeof(Char)));
	if (stat(short2str(path), &statb) >= 0) {	/* resolve through
							 * symlink */
#ifdef S_ISSOCK
	    if (S_ISSOCK(statb.st_mode))	/* Socket */
		return 0;
#endif
#ifdef S_ISFIFO
	    if (S_ISFIFO(statb.st_mode))	/* Named Pipe */
		return 0;
#endif
	    if (S_ISDIR(statb.st_mode))	/* normal Directory */
		return 1;
	}
    }
    return 0;
} /* end isadirectory */



/* find_rows():
 * 	Return how many rows needed to print sorted down columns
 */
static int
find_rows(items, count, no_file_suffix)
    Char *items[];
    int     count, no_file_suffix;
{
    register int i, columns, rows;
    unsigned int maxwidth = 0;

    for (i = 0; i < count; i++)	/* find widest string */
	maxwidth = max(maxwidth, (unsigned int) Strlen(items[i]));

    maxwidth += no_file_suffix ? 1 : 2;	/* for the file tag and space */
    columns = (TermH + 1) / maxwidth;	/* PWP: terminal size change */
    if (!columns)
	columns = 1;
    rows = (count + (columns - 1)) / columns;

    return rows;
} /* end rows_needed_by_print_by_column */


/* print_by_column():
 * 	Print sorted down columns or across columns when the first
 *	word of $listflags shell variable contains 'x'.
 *
 */
void
print_by_column(dir, items, count, no_file_suffix)
    register Char *dir, *items[];
    int     count, no_file_suffix;
{
    register int i, r, c, columns, rows;
    unsigned int w, maxwidth = 0;
    Char *val;
    bool across;

    lbuffed = 0;		/* turn off line buffering */

    
    across = ((val = varval(STRlistflags)) != STRNULL) && 
	     (Strchr(val, 'x') != NULL);

    for (i = 0; i < count; i++)	/* find widest string */
	maxwidth = max(maxwidth, (unsigned int) Strlen(items[i]));

    maxwidth += no_file_suffix ? 1 : 2;	/* for the file tag and space */
    columns = TermH / maxwidth;		/* PWP: terminal size change */
    if (!columns || !isatty(didfds ? 1 : SHOUT))
	columns = 1;
    rows = (count + (columns - 1)) / columns;

    i = -1;
    for (r = 0; r < rows; r++) {
	for (c = 0; c < columns; c++) {
	    i = across ? (i + 1) : (c * rows + r);

	    if (i < count) {
		w = (unsigned int) Strlen(items[i]);

#ifdef COLOR_LS_F
		if (no_file_suffix) {
		    /* Print the command name */
		    Char f = items[i][w - 1];
		    items[i][w - 1] = 0;
		    print_with_color(items[i], w - 1, f);
		}
		else {
		    /* Print filename followed by '/' or '*' or ' ' */
		    print_with_color(items[i], w, filetype(dir, items[i]));
		    w++;
		}
#else /* ifndef COLOR_LS_F */
		if (no_file_suffix) {
		    /* Print the command name */
		    xprintf("%S", items[i]);
		}
		else {
		    /* Print filename followed by '/' or '*' or ' ' */
		    xprintf("%S%c", items[i],
			    filetype(dir, items[i]));
		    w++;
		}
#endif /* COLOR_LS_F */

		if (c < (columns - 1))	/* Not last column? */
		    for (; w < maxwidth; w++)
			xputchar(' ');
	    }
	    else if (across)
		break;
	}
	if (Tty_raw_mode)
	    xputchar('\r');
	xputchar('\n');
    }

    lbuffed = 1;		/* turn back on line buffering */
    flush();
} /* end print_by_column */


/* StrQcmp():
 *	Compare strings ignoring the quoting chars
 */
int
StrQcmp(str1, str2)
    register Char *str1, *str2;
{
    for (; *str1 && samecase(*str1 & TRIM) == samecase(*str2 & TRIM); 
	 str1++, str2++)
	continue;
    /*
     * The following case analysis is necessary so that characters which look
     * negative collate low against normal characters but high against the
     * end-of-string NUL.
     */
    if (*str1 == '\0' && *str2 == '\0')
	return (0);
    else if (*str1 == '\0')
	return (-1);
    else if (*str2 == '\0')
	return (1);
    else
	return ((*str1 & TRIM) - (*str2 & TRIM));
} /* end StrQcmp */


/* fcompare():
 * 	Comparison routine for qsort
 */
int
fcompare(file1, file2)
    Char  **file1, **file2;
{
    return (int) collate(*file1, *file2);
} /* end fcompare */


/* catn():
 *	Concatenate src onto tail of des.
 *	Des is a string whose maximum length is count.
 *	Always null terminate.
 */
void
catn(des, src, count)
    register Char *des, *src;
    int count;
{
    while (--count >= 0 && *des)
	des++;
    while (--count >= 0)
	if ((*des++ = *src++) == 0)
	    return;
    *des = '\0';
} /* end catn */


/* copyn():
 *	 like strncpy but always leave room for trailing \0
 *	 and always null terminate.
 */
void
copyn(des, src, count)
    register Char *des, *src;
    int count;
{
    while (--count >= 0)
	if ((*des++ = *src++) == 0)
	    return;
    *des = '\0';
} /* end copyn */


/* tgetenv():
 *	like it's normal string counter-part
 *	[apollo uses that in tc.os.c, so it cannot be static]
 */
Char *
tgetenv(str)
    Char   *str;
{
    Char  **var;
    int     len, res;

    len = (int) Strlen(str);
    /* Search the STR_environ for the entry matching str. */
    for (var = STR_environ; var != NULL && *var != NULL; var++)
	if (Strlen(*var) >= len && (*var)[len] == '=') {
	  /* Temporarily terminate the string so we can copy the variable
	     name. */
	    (*var)[len] = '\0';
	    res = StrQcmp(*var, str);
	    /* Restore the '=' and return a pointer to the value of the
	       environment variable. */
	    (*var)[len] = '=';
	    if (res == 0)
		return (&((*var)[len + 1]));
	}
    return (NULL);
} /* end tgetenv */


struct scroll_tab_list *scroll_tab = 0;

static void
add_scroll_tab(item)
    Char *item;
{
    struct scroll_tab_list *new_scroll;

    new_scroll = (struct scroll_tab_list *) xmalloc((size_t)
	    sizeof(struct scroll_tab_list));
    new_scroll->element = Strsave(item);
    new_scroll->next = scroll_tab;
    scroll_tab = new_scroll;
}

static void
choose_scroll_tab(exp_name, cnt)
    Char **exp_name;
    int cnt;
{
    struct scroll_tab_list *loop;
    int tmp = cnt;
    Char **ptr;

    ptr = (Char **) xmalloc((size_t) sizeof(Char *) * cnt);

    for(loop = scroll_tab; loop && (tmp >= 0); loop = loop->next)
	ptr[--tmp] = loop->element;

    qsort((ptr_t) ptr, (size_t) cnt, sizeof(Char *), 
	  (int (*) __P((const void *, const void *))) fcompare);
	    
    copyn(*exp_name, ptr[curchoice], (int) Strlen(ptr[curchoice]));	
    xfree((ptr_t) ptr);
}

static void
free_scroll_tab()
{
    struct scroll_tab_list *loop;

    while(scroll_tab) {
	loop = scroll_tab;
	scroll_tab = scroll_tab->next;
	xfree((ptr_t) loop->element);
	xfree((ptr_t) loop);
    }
}
