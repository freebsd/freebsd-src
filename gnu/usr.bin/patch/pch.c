/* $FreeBSD$
 *
 * $Log: pch.c,v $
 * Revision 2.0.2.0  90/05/01  22:17:51  davison
 * patch12u: unidiff support added
 *
 * Revision 2.0.1.7  88/06/03  15:13:28  lwall
 * patch10: Can now find patches in shar scripts.
 * patch10: Hunks that swapped and then swapped back could core dump.
 *
 * Revision 2.0.1.6  87/06/04  16:18:13  lwall
 * pch_swap didn't swap p_bfake and p_efake.
 *
 * Revision 2.0.1.5  87/01/30  22:47:42  lwall
 * Improved responses to mangled patches.
 *
 * Revision 2.0.1.4  87/01/05  16:59:53  lwall
 * New-style context diffs caused double call to free().
 *
 * Revision 2.0.1.3  86/11/14  10:08:33  lwall
 * Fixed problem where a long pattern wouldn't grow the hunk.
 * Also restored p_input_line when backtracking so error messages are right.
 *
 * Revision 2.0.1.2  86/11/03  17:49:52  lwall
 * New-style delete triggers spurious assertion error.
 *
 * Revision 2.0.1.1  86/10/29  15:52:08  lwall
 * Could falsely report new-style context diff.
 *
 * Revision 2.0  86/09/17  15:39:37  lwall
 * Baseline for netwide release.
 *
 */

#include "EXTERN.h"
#include "common.h"
#include "util.h"
#include "INTERN.h"
#include "pch.h"

/* Patch (diff listing) abstract type. */

static long p_filesize;			/* size of the patch file */
static LINENUM p_first;			/* 1st line number */
static LINENUM p_newfirst;		/* 1st line number of replacement */
static LINENUM p_ptrn_lines;		/* # lines in pattern */
static LINENUM p_repl_lines;		/* # lines in replacement text */
static LINENUM p_end = -1;		/* last line in hunk */
static LINENUM p_max;			/* max allowed value of p_end */
static LINENUM p_context = 3;		/* # of context lines */
static LINENUM p_input_line = 0;	/* current line # from patch file */
static char **p_line = Null(char**);	/* the text of the hunk */
static short *p_len = Null(short*);	/* length of each line */
static char *p_Char = Nullch;		/* +, -, and ! */
static int hunkmax = INITHUNKMAX;	/* size of above arrays to begin with */
static int p_indent;			/* indent to patch */
static LINENUM p_base;			/* where to intuit this time */
static LINENUM p_bline;			/* line # of p_base */
static LINENUM p_start;			/* where intuit found a patch */
static LINENUM p_sline;			/* and the line number for it */
static LINENUM p_hunk_beg;		/* line number of current hunk */
static LINENUM p_efake = -1;		/* end of faked up lines--don't free */
static LINENUM p_bfake = -1;		/* beg of faked up lines */

/* Prepare to look for the next patch in the patch file. */

void
re_patch()
{
    p_first = Nulline;
    p_newfirst = Nulline;
    p_ptrn_lines = Nulline;
    p_repl_lines = Nulline;
    p_end = (LINENUM)-1;
    p_max = Nulline;
    p_indent = 0;
}

/* Open the patch file at the beginning of time. */

void
open_patch_file(filename)
char *filename;
{
    if (filename == Nullch || !*filename || strEQ(filename, "-")) {
	pfp = fopen(TMPPATNAME, "w");
	if (pfp == Nullfp)
	    pfatal2("can't create %s", TMPPATNAME);
	while (fgets(buf, sizeof buf, stdin) != Nullch)
	    fputs(buf, pfp);
	Fclose(pfp);
	filename = TMPPATNAME;
    }
    pfp = fopen(filename, "r");
    if (pfp == Nullfp)
	pfatal2("patch file %s not found", filename);
    Fstat(fileno(pfp), &filestat);
    p_filesize = filestat.st_size;
    next_intuit_at(0L,1L);			/* start at the beginning */
    set_hunkmax();
}

/* Make sure our dynamically realloced tables are malloced to begin with. */

void
set_hunkmax()
{
#ifndef lint
    if (p_line == Null(char**))
	p_line = (char**) malloc((MEM)hunkmax * sizeof(char *));
    if (p_len == Null(short*))
	p_len  = (short*) malloc((MEM)hunkmax * sizeof(short));
#endif
    if (p_Char == Nullch)
	p_Char = (char*)  malloc((MEM)hunkmax * sizeof(char));
}

/* Enlarge the arrays containing the current hunk of patch. */

void
grow_hunkmax()
{
    hunkmax *= 2;
    /*
     * Note that on most systems, only the p_line array ever gets fresh memory
     * since p_len can move into p_line's old space, and p_Char can move into
     * p_len's old space.  Not on PDP-11's however.  But it doesn't matter.
     */
    assert(p_line != Null(char**) && p_len != Null(short*) && p_Char != Nullch);
#ifndef lint
    p_line = (char**) realloc((char*)p_line, (MEM)hunkmax * sizeof(char *));
    p_len  = (short*) realloc((char*)p_len,  (MEM)hunkmax * sizeof(short));
    p_Char = (char*)  realloc((char*)p_Char, (MEM)hunkmax * sizeof(char));
#endif
    if (p_line != Null(char**) && p_len != Null(short*) && p_Char != Nullch)
	return;
    if (!using_plan_a)
	fatal1("out of memory\n");
    out_of_mem = TRUE;		/* whatever is null will be allocated again */
				/* from within plan_a(), of all places */
}

/* True if the remainder of the patch file contains a diff of some sort. */

bool
there_is_another_patch()
{
    if (p_base != 0L && p_base >= p_filesize) {
	if (verbose)
	    say1("done\n");
	return FALSE;
    }
    if (verbose)
	say1("Hmm...");
    diff_type = intuit_diff_type();
    if (!diff_type) {
	if (p_base != 0L) {
	    if (verbose)
		say1("  Ignoring the trailing garbage.\ndone\n");
	}
	else
	    say1("  I can't seem to find a patch in there anywhere.\n");
	return FALSE;
    }
    if (verbose)
	say3("  %sooks like %s to me...\n",
	    (p_base == 0L ? "L" : "The next patch l"),
	    diff_type == UNI_DIFF ? "a unified diff" :
	    diff_type == CONTEXT_DIFF ? "a context diff" :
	    diff_type == NEW_CONTEXT_DIFF ? "a new-style context diff" :
	    diff_type == NORMAL_DIFF ? "a normal diff" :
	    "an ed script" );
    if (p_indent && verbose)
	say3("(Patch is indented %d space%s.)\n", p_indent, p_indent==1?"":"s");
    skip_to(p_start,p_sline);
    while (filearg[0] == Nullch) {
	if (force || batch || skip_rest_of_patch) {
	    say1("No file to patch.  Skipping...\n");
	    filearg[0] = savestr(bestguess);
	    skip_rest_of_patch = TRUE;
	    return TRUE;
	}
	(void) ask1("File to patch: ");
	if (*buf != '\n') {
	    if (bestguess)
		free(bestguess);
	    bestguess = savestr(buf);
	    filearg[0] = fetchname(buf, 0, FALSE);
	}
	if (filearg[0] == Nullch) {
	    if (ask1("No file found--skip this patch? [n] ")) {
	    	if (*buf != 'y') {
		    continue;
	    	}
	    }
	    if (verbose)
		say1("Skipping patch...\n");
	    filearg[0] = fetchname(bestguess, 0, TRUE);
	    skip_rest_of_patch = TRUE;
	    return TRUE;
	}
    }
    return TRUE;
}

/* Determine what kind of diff is in the remaining part of the patch file. */

int
intuit_diff_type()
{
    Reg4 long this_line = 0;
    Reg5 long previous_line;
    Reg6 long first_command_line = -1;
    long fcl_line;
    Reg7 bool last_line_was_command = FALSE;
    Reg8 bool this_is_a_command = FALSE;
    Reg9 bool stars_last_line = FALSE;
    Reg10 bool stars_this_line = FALSE;
    Reg3 int indent;
    Reg1 char *s;
    Reg2 char *t;
    char *indtmp = Nullch;
    char *oldtmp = Nullch;
    char *newtmp = Nullch;
    char *indname = Nullch;
    char *oldname = Nullch;
    char *newname = Nullch;
    Reg11 int retval;
    bool no_filearg = (filearg[0] == Nullch);
    extern int index_first;

    ok_to_create_file = FALSE;
    Fseek(pfp, p_base, 0);
    p_input_line = p_bline - 1;
    for (;;) {
	previous_line = this_line;
	last_line_was_command = this_is_a_command;
	stars_last_line = stars_this_line;
	this_line = ftell(pfp);
	indent = 0;
	p_input_line++;
	if (fgets(buf, sizeof buf, pfp) == Nullch) {
	    if (first_command_line >= 0L) {
					/* nothing but deletes!? */
		p_start = first_command_line;
		p_sline = fcl_line;
		retval = ED_DIFF;
		goto scan_exit;
	    }
	    else {
		p_start = this_line;
		p_sline = p_input_line;
		retval = 0;
		goto scan_exit;
	    }
	}
	for (s = buf; *s == ' ' || *s == '\t' || *s == 'X'; s++) {
	    if (*s == '\t')
		indent += 8 - (indent % 8);
	    else
		indent++;
	}
	for (t=s; isdigit((unsigned char)*t) || *t == ','; t++) ;
	this_is_a_command = (isdigit((unsigned char)*s) &&
	  (*t == 'd' || *t == 'c' || *t == 'a') );
	if (first_command_line < 0L && this_is_a_command) {
	    first_command_line = this_line;
	    fcl_line = p_input_line;
	    p_indent = indent;		/* assume this for now */
	}
	if (!stars_last_line && strnEQ(s, "*** ", 4))
	    oldtmp = savestr(s+4);
	else if (strnEQ(s, "--- ", 4))
	    newtmp = savestr(s+4);
	else if (strnEQ(s, "+++ ", 4))
	    oldtmp = savestr(s+4);	/* pretend it is the old name */
	else if (strnEQ(s, "Index:", 6))
	    indtmp = savestr(s+6);
	else if (strnEQ(s, "Prereq:", 7)) {
	    for (t=s+7; isspace((unsigned char)*t); t++) ;
	    revision = savestr(t);
	    for (t=revision; *t && !isspace((unsigned char)*t); t++) ;
	    *t = '\0';
	    if (!*revision) {
		free(revision);
		revision = Nullch;
	    }
	}
	if ((!diff_type || diff_type == ED_DIFF) &&
	  first_command_line >= 0L &&
	  strEQ(s, ".\n") ) {
	    p_indent = indent;
	    p_start = first_command_line;
	    p_sline = fcl_line;
	    retval = ED_DIFF;
	    goto scan_exit;
	}
	if ((!diff_type || diff_type == UNI_DIFF) && strnEQ(s, "@@ -", 4)) {
	    if (!atol(s+3))
		ok_to_create_file = TRUE;
	    p_indent = indent;
	    p_start = this_line;
	    p_sline = p_input_line;
	    retval = UNI_DIFF;
	    goto scan_exit;
	}
	stars_this_line = strnEQ(s, "********", 8);
	if ((!diff_type || diff_type == CONTEXT_DIFF) && stars_last_line &&
		 strnEQ(s, "*** ", 4)) {
	    if (!atol(s+4))
		ok_to_create_file = TRUE;
	    /* if this is a new context diff the character just before */
	    /* the newline is a '*'. */
	    while (*s != '\n')
		s++;
	    p_indent = indent;
	    p_start = previous_line;
	    p_sline = p_input_line - 1;
	    retval = (*(s-1) == '*' ? NEW_CONTEXT_DIFF : CONTEXT_DIFF);
	    goto scan_exit;
	}
	if ((!diff_type || diff_type == NORMAL_DIFF) &&
	  last_line_was_command &&
	  (strnEQ(s, "< ", 2) || strnEQ(s, "> ", 2)) ) {
	    p_start = previous_line;
	    p_sline = p_input_line - 1;
	    p_indent = indent;
	    retval = NORMAL_DIFF;
	    goto scan_exit;
	}
    }
  scan_exit:
    if (no_filearg) {
	if (indtmp != Nullch)
	    indname = fetchname(indtmp, strippath, ok_to_create_file);
	if (oldtmp != Nullch)
	    oldname = fetchname(oldtmp, strippath, ok_to_create_file);
	if (newtmp != Nullch)
	    newname = fetchname(newtmp, strippath, ok_to_create_file);
	if (index_first && indname)
	    filearg[0] = savestr(indname);
	else if (oldname && newname) {
	    if (strlen(oldname) < strlen(newname))
		filearg[0] = savestr(oldname);
	    else
		filearg[0] = savestr(newname);
	} else if (indname)
	    filearg[0] = savestr(indname);
	else if (oldname)
	    filearg[0] = savestr(oldname);
	else if (newname)
	    filearg[0] = savestr(newname);
    }
    if (bestguess) {
	free(bestguess);
	bestguess = Nullch;
    }
    if (filearg[0] != Nullch)
	bestguess = savestr(filearg[0]);
    else if (indtmp != Nullch)
	bestguess = fetchname(indtmp, strippath, TRUE);
    else {
	if (oldtmp != Nullch)
	    oldname = fetchname(oldtmp, strippath, TRUE);
	if (newtmp != Nullch)
	    newname = fetchname(newtmp, strippath, TRUE);
	if (oldname && newname) {
	    if (strlen(oldname) < strlen(newname))
		bestguess = savestr(oldname);
	    else
		bestguess = savestr(newname);
	}
	else if (oldname)
	    bestguess = savestr(oldname);
	else if (newname)
	    bestguess = savestr(newname);
    }
    if (indtmp != Nullch)
	free(indtmp);
    if (oldtmp != Nullch)
	free(oldtmp);
    if (newtmp != Nullch)
	free(newtmp);
    if (indname != Nullch)
	free(indname);
    if (oldname != Nullch)
	free(oldname);
    if (newname != Nullch)
	free(newname);
    return retval;
}

/* Remember where this patch ends so we know where to start up again. */

void
next_intuit_at(file_pos,file_line)
long file_pos;
long file_line;
{
    p_base = file_pos;
    p_bline = file_line;
}

/* Basically a verbose fseek() to the actual diff listing. */

void
skip_to(file_pos,file_line)
long file_pos;
long file_line;
{
    char *ret;

    assert(p_base <= file_pos);
    if (verbose && p_base < file_pos) {
	Fseek(pfp, p_base, 0);
	say1("The text leading up to this was:\n--------------------------\n");
	while (ftell(pfp) < file_pos) {
	    ret = fgets(buf, sizeof buf, pfp);
	    assert(ret != Nullch);
	    say2("|%s", buf);
	}
	say1("--------------------------\n");
    }
    else
	Fseek(pfp, file_pos, 0);
    p_input_line = file_line - 1;
}

/* Make this a function for better debugging.  */
static void
malformed ()
{
    fatal3("malformed patch at line %ld: %s", p_input_line, buf);
		/* about as informative as "Syntax error" in C */
}

/* True if there is more of the current diff listing to process. */

bool
another_hunk()
{
    Reg1 char *s;
    Reg8 char *ret;
    Reg2 int context = 0;

    while (p_end >= 0) {
	if (p_end == p_efake)
	    p_end = p_bfake;		/* don't free twice */
	else
	    free(p_line[p_end]);
	p_end--;
    }
    assert(p_end == -1);
    p_efake = -1;

    p_max = hunkmax;			/* gets reduced when --- found */
    if (diff_type == CONTEXT_DIFF || diff_type == NEW_CONTEXT_DIFF) {
	long line_beginning = ftell(pfp);
					/* file pos of the current line */
	LINENUM repl_beginning = 0;	/* index of --- line */
	Reg4 LINENUM fillcnt = 0;	/* #lines of missing ptrn or repl */
	Reg5 LINENUM fillsrc;		/* index of first line to copy */
	Reg6 LINENUM filldst;		/* index of first missing line */
	bool ptrn_spaces_eaten = FALSE;	/* ptrn was slightly misformed */
	Reg9 bool repl_could_be_missing = TRUE;
					/* no + or ! lines in this hunk */
	bool repl_missing = FALSE;	/* we are now backtracking */
	long repl_backtrack_position = 0;
					/* file pos of first repl line */
	LINENUM repl_patch_line;	/* input line number for same */
	Reg7 LINENUM ptrn_copiable = 0;
					/* # of copiable lines in ptrn */

	ret = pgets(buf, sizeof buf, pfp);
	p_input_line++;
	if (ret == Nullch || strnNE(buf, "********", 8)) {
	    next_intuit_at(line_beginning,p_input_line);
	    return FALSE;
	}
	p_context = 100;
	p_hunk_beg = p_input_line + 1;
	while (p_end < p_max) {
	    line_beginning = ftell(pfp);
	    ret = pgets(buf, sizeof buf, pfp);
	    p_input_line++;
	    if (ret == Nullch) {
		if (p_max - p_end < 4)
		    Strcpy(buf, "  \n");  /* assume blank lines got chopped */
		else {
		    if (repl_beginning && repl_could_be_missing) {
			repl_missing = TRUE;
			goto hunk_done;
		    }
		    fatal1("unexpected end of file in patch\n");
		}
	    }
	    p_end++;
	    assert(p_end < hunkmax);
	    p_Char[p_end] = *buf;
#ifdef zilog
	    p_line[(short)p_end] = Nullch;
#else
	    p_line[p_end] = Nullch;
#endif
	    switch (*buf) {
	    case '*':
		if (strnEQ(buf, "********", 8)) {
		    if (repl_beginning && repl_could_be_missing) {
			repl_missing = TRUE;
			goto hunk_done;
		    }
		    else
			fatal2("unexpected end of hunk at line %ld\n",
			    p_input_line);
		}
		if (p_end != 0) {
		    if (repl_beginning && repl_could_be_missing) {
			repl_missing = TRUE;
			goto hunk_done;
		    }
		    fatal3("unexpected *** at line %ld: %s", p_input_line, buf);
		}
		context = 0;
		p_line[p_end] = savestr(buf);
		if (out_of_mem) {
		    p_end--;
		    return FALSE;
		}
		for (s=buf; *s && !isdigit((unsigned char)*s); s++) ;
		if (!*s)
		    malformed ();
		if (strnEQ(s,"0,0",3))
		    strcpy(s,s+2);
		p_first = (LINENUM) atol(s);
		while (isdigit((unsigned char)*s)) s++;
		if (*s == ',') {
		    for (; *s && !isdigit((unsigned char)*s); s++) ;
		    if (!*s)
			malformed ();
		    p_ptrn_lines = ((LINENUM)atol(s)) - p_first + 1;
		}
		else if (p_first)
		    p_ptrn_lines = 1;
		else {
		    p_ptrn_lines = 0;
		    p_first = 1;
		}
		p_max = p_ptrn_lines + 6;	/* we need this much at least */
		while (p_max >= hunkmax)
		    grow_hunkmax();
		p_max = hunkmax;
		break;
	    case '-':
		if (buf[1] == '-') {
		    if (repl_beginning ||
			(p_end != p_ptrn_lines + 1 + (p_Char[p_end-1] == '\n')))
		    {
			if (p_end == 1) {
			    /* `old' lines were omitted - set up to fill */
			    /* them in from 'new' context lines. */
			    p_end = p_ptrn_lines + 1;
			    fillsrc = p_end + 1;
			    filldst = 1;
			    fillcnt = p_ptrn_lines;
			}
			else {
			    if (repl_beginning) {
				if (repl_could_be_missing){
				    repl_missing = TRUE;
				    goto hunk_done;
				}
				fatal3(
"duplicate \"---\" at line %ld--check line numbers at line %ld\n",
				    p_input_line, p_hunk_beg + repl_beginning);
			    }
			    else {
				fatal4(
"%s \"---\" at line %ld--check line numbers at line %ld\n",
				    (p_end <= p_ptrn_lines
					? "Premature"
					: "Overdue" ),
				    p_input_line, p_hunk_beg);
			    }
			}
		    }
		    repl_beginning = p_end;
		    repl_backtrack_position = ftell(pfp);
		    repl_patch_line = p_input_line;
		    p_line[p_end] = savestr(buf);
		    if (out_of_mem) {
			p_end--;
			return FALSE;
		    }
		    p_Char[p_end] = '=';
		    for (s=buf; *s && !isdigit((unsigned char)*s); s++) ;
		    if (!*s)
			malformed ();
		    p_newfirst = (LINENUM) atol(s);
		    while (isdigit((unsigned char)*s)) s++;
		    if (*s == ',') {
			for (; *s && !isdigit((unsigned char)*s); s++) ;
			if (!*s)
			    malformed ();
			p_repl_lines = ((LINENUM)atol(s)) - p_newfirst + 1;
		    }
		    else if (p_newfirst)
			p_repl_lines = 1;
		    else {
			p_repl_lines = 0;
			p_newfirst = 1;
		    }
		    p_max = p_repl_lines + p_end;
		    if (p_max > MAXHUNKSIZE)
			fatal4("hunk too large (%ld lines) at line %ld: %s",
			      p_max, p_input_line, buf);
		    while (p_max >= hunkmax)
			grow_hunkmax();
		    if (p_repl_lines != ptrn_copiable
		     && (p_context != 0 || p_repl_lines != 1))
			repl_could_be_missing = FALSE;
		    break;
		}
		goto change_line;
	    case '+':  case '!':
		repl_could_be_missing = FALSE;
	      change_line:
		if (buf[1] == '\n' && canonicalize)
		    strcpy(buf+1," \n");
		if (!isspace((unsigned char)buf[1]) && buf[1] != '>' && buf[1] != '<' &&
		  repl_beginning && repl_could_be_missing) {
		    repl_missing = TRUE;
		    goto hunk_done;
		}
		if (context >= 0) {
		    if (context < p_context)
			p_context = context;
		    context = -1000;
		}
		p_line[p_end] = savestr(buf+2);
		if (out_of_mem) {
		    p_end--;
		    return FALSE;
		}
		break;
	    case '\t': case '\n':	/* assume the 2 spaces got eaten */
		if (repl_beginning && repl_could_be_missing &&
		  (!ptrn_spaces_eaten || diff_type == NEW_CONTEXT_DIFF) ) {
		    repl_missing = TRUE;
		    goto hunk_done;
		}
		p_line[p_end] = savestr(buf);
		if (out_of_mem) {
		    p_end--;
		    return FALSE;
		}
		if (p_end != p_ptrn_lines + 1) {
		    ptrn_spaces_eaten |= (repl_beginning != 0);
		    context++;
		    if (!repl_beginning)
			ptrn_copiable++;
		    p_Char[p_end] = ' ';
		}
		break;
	    case ' ':
		if (!isspace((unsigned char)buf[1]) &&
		  repl_beginning && repl_could_be_missing) {
		    repl_missing = TRUE;
		    goto hunk_done;
		}
		context++;
		if (!repl_beginning)
		    ptrn_copiable++;
		p_line[p_end] = savestr(buf+2);
		if (out_of_mem) {
		    p_end--;
		    return FALSE;
		}
		break;
	    default:
		if (repl_beginning && repl_could_be_missing) {
		    repl_missing = TRUE;
		    goto hunk_done;
		}
		malformed ();
	    }
	    /* set up p_len for strncmp() so we don't have to */
	    /* assume null termination */
	    if (p_line[p_end])
		p_len[p_end] = strlen(p_line[p_end]);
	    else
		p_len[p_end] = 0;
	}

    hunk_done:
	if (p_end >=0 && !repl_beginning)
	    fatal2("no --- found in patch at line %ld\n", pch_hunk_beg());

	if (repl_missing) {

	    /* reset state back to just after --- */
	    p_input_line = repl_patch_line;
	    for (p_end--; p_end > repl_beginning; p_end--)
		free(p_line[p_end]);
	    Fseek(pfp, repl_backtrack_position, 0);

	    /* redundant 'new' context lines were omitted - set */
	    /* up to fill them in from the old file context */
	    if (!p_context && p_repl_lines == 1) {
		p_repl_lines = 0;
		p_max--;
	    }
	    fillsrc = 1;
	    filldst = repl_beginning+1;
	    fillcnt = p_repl_lines;
	    p_end = p_max;
	}
	else if (!p_context && fillcnt == 1) {
	    /* the first hunk was a null hunk with no context */
	    /* and we were expecting one line -- fix it up. */
	    while (filldst < p_end) {
		p_line[filldst] = p_line[filldst+1];
		p_Char[filldst] = p_Char[filldst+1];
		p_len[filldst] = p_len[filldst+1];
		filldst++;
	    }
#if 0
	    repl_beginning--;		/* this doesn't need to be fixed */
#endif
	    p_end--;
	    p_first++;			/* do append rather than insert */
	    fillcnt = 0;
	    p_ptrn_lines = 0;
	}

	if (diff_type == CONTEXT_DIFF &&
	  (fillcnt || (p_first > 1 && ptrn_copiable > 2*p_context)) ) {
	    if (verbose)
		say4("%s\n%s\n%s\n",
"(Fascinating--this is really a new-style context diff but without",
"the telltale extra asterisks on the *** line that usually indicate",
"the new style...)");
	    diff_type = NEW_CONTEXT_DIFF;
	}

	/* if there were omitted context lines, fill them in now */
	if (fillcnt) {
	    p_bfake = filldst;		/* remember where not to free() */
	    p_efake = filldst + fillcnt - 1;
	    while (fillcnt-- > 0) {
		while (fillsrc <= p_end && p_Char[fillsrc] != ' ')
		    fillsrc++;
		if (fillsrc > p_end)
		    fatal2("replacement text or line numbers mangled in hunk at line %ld\n",
			p_hunk_beg);
		p_line[filldst] = p_line[fillsrc];
		p_Char[filldst] = p_Char[fillsrc];
		p_len[filldst] = p_len[fillsrc];
		fillsrc++; filldst++;
	    }
	    while (fillsrc <= p_end && fillsrc != repl_beginning &&
	      p_Char[fillsrc] != ' ')
		fillsrc++;
#ifdef DEBUGGING
	    if (debug & 64)
		printf("fillsrc %ld, filldst %ld, rb %ld, e+1 %ld\n",
		    fillsrc,filldst,repl_beginning,p_end+1);
#endif
	    assert(fillsrc==p_end+1 || fillsrc==repl_beginning);
	    assert(filldst==p_end+1 || filldst==repl_beginning);
	}
    }
    else if (diff_type == UNI_DIFF) {
	long line_beginning = ftell(pfp);
					/* file pos of the current line */
	Reg4 LINENUM fillsrc;		/* index of old lines */
	Reg5 LINENUM filldst;		/* index of new lines */
	char ch;

	ret = pgets(buf, sizeof buf, pfp);
	p_input_line++;
	if (ret == Nullch || strnNE(buf, "@@ -", 4)) {
	    next_intuit_at(line_beginning,p_input_line);
	    return FALSE;
	}
	s = buf+4;
	if (!*s)
	    malformed ();
	p_first = (LINENUM) atol(s);
	while (isdigit((unsigned char)*s)) s++;
	if (*s == ',') {
	    p_ptrn_lines = (LINENUM) atol(++s);
	    while (isdigit((unsigned char)*s)) s++;
	} else
	    p_ptrn_lines = 1;
	if (*s == ' ') s++;
	if (*s != '+' || !*++s)
	    malformed ();
	p_newfirst = (LINENUM) atol(s);
	while (isdigit((unsigned char)*s)) s++;
	if (*s == ',') {
	    p_repl_lines = (LINENUM) atol(++s);
	    while (isdigit((unsigned char)*s)) s++;
	} else
	    p_repl_lines = 1;
	if (*s == ' ') s++;
	if (*s != '@')
	    malformed ();
	if (!p_ptrn_lines)
	    p_first++;			/* do append rather than insert */
	p_max = p_ptrn_lines + p_repl_lines + 1;
	while (p_max >= hunkmax)
	    grow_hunkmax();
	fillsrc = 1;
	filldst = fillsrc + p_ptrn_lines;
	p_end = filldst + p_repl_lines;
	Sprintf(buf,"*** %ld,%ld ****\n",p_first,p_first + p_ptrn_lines - 1);
	p_line[0] = savestr(buf);
	if (out_of_mem) {
	    p_end = -1;
	    return FALSE;
	}
	p_Char[0] = '*';
        Sprintf(buf,"--- %ld,%ld ----\n",p_newfirst,p_newfirst+p_repl_lines-1);
	p_line[filldst] = savestr(buf);
	if (out_of_mem) {
	    p_end = 0;
	    return FALSE;
	}
	p_Char[filldst++] = '=';
	p_context = 100;
	context = 0;
	p_hunk_beg = p_input_line + 1;
	while (fillsrc <= p_ptrn_lines || filldst <= p_end) {
	    line_beginning = ftell(pfp);
	    ret = pgets(buf, sizeof buf, pfp);
	    p_input_line++;
	    if (ret == Nullch) {
		if (p_max - filldst < 3)
		    Strcpy(buf, " \n");  /* assume blank lines got chopped */
		else {
		    fatal1("unexpected end of file in patch\n");
		}
	    }
	    if (*buf == '\t' || *buf == '\n') {
		ch = ' ';		/* assume the space got eaten */
		s = savestr(buf);
	    }
	    else {
		ch = *buf;
		s = savestr(buf+1);
	    }
	    if (out_of_mem) {
		while (--filldst > p_ptrn_lines)
		    free(p_line[filldst]);
		p_end = fillsrc-1;
		return FALSE;
	    }
	    switch (ch) {
	    case '-':
		if (fillsrc > p_ptrn_lines) {
		    free(s);
		    p_end = filldst-1;
		    malformed ();
		}
		p_Char[fillsrc] = ch;
		p_line[fillsrc] = s;
		p_len[fillsrc++] = strlen(s);
		break;
	    case '=':
		ch = ' ';
		/* FALL THROUGH */
	    case ' ':
		if (fillsrc > p_ptrn_lines) {
		    free(s);
		    while (--filldst > p_ptrn_lines)
			free(p_line[filldst]);
		    p_end = fillsrc-1;
		    malformed ();
		}
		context++;
		p_Char[fillsrc] = ch;
		p_line[fillsrc] = s;
		p_len[fillsrc++] = strlen(s);
		s = savestr(s);
		if (out_of_mem) {
		    while (--filldst > p_ptrn_lines)
			free(p_line[filldst]);
		    p_end = fillsrc-1;
		    return FALSE;
		}
		/* FALL THROUGH */
	    case '+':
		if (filldst > p_end) {
		    free(s);
		    while (--filldst > p_ptrn_lines)
			free(p_line[filldst]);
		    p_end = fillsrc-1;
		    malformed ();
		}
		p_Char[filldst] = ch;
		p_line[filldst] = s;
		p_len[filldst++] = strlen(s);
		break;
	    default:
		p_end = filldst;
		malformed ();
	    }
	    if (ch != ' ' && context > 0) {
		if (context < p_context)
		    p_context = context;
		context = -1000;
	    }
	}/* while */
    }
    else {				/* normal diff--fake it up */
	char hunk_type;
	Reg3 int i;
	LINENUM min, max;
	long line_beginning = ftell(pfp);

	p_context = 0;
	ret = pgets(buf, sizeof buf, pfp);
	p_input_line++;
	if (ret == Nullch || !isdigit((unsigned char)*buf)) {
	    next_intuit_at(line_beginning,p_input_line);
	    return FALSE;
	}
	p_first = (LINENUM)atol(buf);
	for (s=buf; isdigit((unsigned char)*s); s++) ;
	if (*s == ',') {
	    p_ptrn_lines = (LINENUM)atol(++s) - p_first + 1;
	    while (isdigit((unsigned char)*s)) s++;
	}
	else
	    p_ptrn_lines = (*s != 'a');
	hunk_type = *s;
	if (hunk_type == 'a')
	    p_first++;			/* do append rather than insert */
	min = (LINENUM)atol(++s);
	for (; isdigit((unsigned char)*s); s++) ;
	if (*s == ',')
	    max = (LINENUM)atol(++s);
	else
	    max = min;
	if (hunk_type == 'd')
	    min++;
	p_end = p_ptrn_lines + 1 + max - min + 1;
	if (p_end > MAXHUNKSIZE)
	    fatal4("hunk too large (%ld lines) at line %ld: %s",
		  p_end, p_input_line, buf);
	while (p_end >= hunkmax)
	    grow_hunkmax();
	p_newfirst = min;
	p_repl_lines = max - min + 1;
	Sprintf(buf, "*** %ld,%ld\n", p_first, p_first + p_ptrn_lines - 1);
	p_line[0] = savestr(buf);
	if (out_of_mem) {
	    p_end = -1;
	    return FALSE;
	}
	p_Char[0] = '*';
	for (i=1; i<=p_ptrn_lines; i++) {
	    ret = pgets(buf, sizeof buf, pfp);
	    p_input_line++;
	    if (ret == Nullch)
		fatal2("unexpected end of file in patch at line %ld\n",
		  p_input_line);
	    if (*buf != '<')
		fatal2("< expected at line %ld of patch\n", p_input_line);
	    p_line[i] = savestr(buf+2);
	    if (out_of_mem) {
		p_end = i-1;
		return FALSE;
	    }
	    p_len[i] = strlen(p_line[i]);
	    p_Char[i] = '-';
	}
	if (hunk_type == 'c') {
	    ret = pgets(buf, sizeof buf, pfp);
	    p_input_line++;
	    if (ret == Nullch)
		fatal2("unexpected end of file in patch at line %ld\n",
		    p_input_line);
	    if (*buf != '-')
		fatal2("--- expected at line %ld of patch\n", p_input_line);
	}
	Sprintf(buf, "--- %ld,%ld\n", min, max);
	p_line[i] = savestr(buf);
	if (out_of_mem) {
	    p_end = i-1;
	    return FALSE;
	}
	p_Char[i] = '=';
	for (i++; i<=p_end; i++) {
	    ret = pgets(buf, sizeof buf, pfp);
	    p_input_line++;
	    if (ret == Nullch)
		fatal2("unexpected end of file in patch at line %ld\n",
		    p_input_line);
	    if (*buf != '>')
		fatal2("> expected at line %ld of patch\n", p_input_line);
	    p_line[i] = savestr(buf+2);
	    if (out_of_mem) {
		p_end = i-1;
		return FALSE;
	    }
	    p_len[i] = strlen(p_line[i]);
	    p_Char[i] = '+';
	}
    }
    if (reverse)			/* backwards patch? */
	if (!pch_swap())
	    say1("Not enough memory to swap next hunk!\n");
#ifdef DEBUGGING
    if (debug & 2) {
	int i;
	char special;

	for (i=0; i <= p_end; i++) {
	    if (i == p_ptrn_lines)
		special = '^';
	    else
		special = ' ';
	    fprintf(stderr, "%3d %c %c %s", i, p_Char[i], special, p_line[i]);
	    Fflush(stderr);
	}
    }
#endif
    if (p_end+1 < hunkmax)	/* paranoia reigns supreme... */
	p_Char[p_end+1] = '^';  /* add a stopper for apply_hunk */
    return TRUE;
}

/* Input a line from the patch file, worrying about indentation. */

char *
pgets(bf,sz,fp)
char *bf;
int sz;
FILE *fp;
{
    char *ret = fgets(bf, sz, fp);
    Reg1 char *s;
    Reg2 int indent = 0;

    if (p_indent && ret != Nullch) {
	for (s=buf;
	  indent < p_indent && (*s == ' ' || *s == '\t' || *s == 'X'); s++) {
	    if (*s == '\t')
		indent += 8 - (indent % 7);
	    else
		indent++;
	}
	if (buf != s)
	    Strcpy(buf, s);
    }
    return ret;
}

/* Reverse the old and new portions of the current hunk. */

bool
pch_swap()
{
    char **tp_line;		/* the text of the hunk */
    short *tp_len;		/* length of each line */
    char *tp_char;		/* +, -, and ! */
    Reg1 LINENUM i;
    Reg2 LINENUM n;
    bool blankline = FALSE;
    Reg3 char *s;

    i = p_first;
    p_first = p_newfirst;
    p_newfirst = i;

    /* make a scratch copy */

    tp_line = p_line;
    tp_len = p_len;
    tp_char = p_Char;
    p_line = Null(char**);	/* force set_hunkmax to allocate again */
    p_len = Null(short*);
    p_Char = Nullch;
    set_hunkmax();
    if (p_line == Null(char**) || p_len == Null(short*) || p_Char == Nullch) {
#ifndef lint
	if (p_line == Null(char**))
	    free((char*)p_line);
	p_line = tp_line;
	if (p_len == Null(short*))
	    free((char*)p_len);
	p_len = tp_len;
#endif
	if (p_Char == Nullch)
	    free((char*)p_Char);
	p_Char = tp_char;
	return FALSE;		/* not enough memory to swap hunk! */
    }

    /* now turn the new into the old */

    i = p_ptrn_lines + 1;
    if (tp_char[i] == '\n') {		/* account for possible blank line */
	blankline = TRUE;
	i++;
    }
    if (p_efake >= 0) {			/* fix non-freeable ptr range */
	if (p_efake <= i)
	    n = p_end - i + 1;
	else
	    n = -i;
	p_efake += n;
	p_bfake += n;
    }
    for (n=0; i <= p_end; i++,n++) {
	p_line[n] = tp_line[i];
	p_Char[n] = tp_char[i];
	if (p_Char[n] == '+')
	    p_Char[n] = '-';
	p_len[n] = tp_len[i];
    }
    if (blankline) {
	i = p_ptrn_lines + 1;
	p_line[n] = tp_line[i];
	p_Char[n] = tp_char[i];
	p_len[n] = tp_len[i];
	n++;
    }
    assert(p_Char[0] == '=');
    p_Char[0] = '*';
    for (s=p_line[0]; *s; s++)
	if (*s == '-')
	    *s = '*';

    /* now turn the old into the new */

    assert(tp_char[0] == '*');
    tp_char[0] = '=';
    for (s=tp_line[0]; *s; s++)
	if (*s == '*')
	    *s = '-';
    for (i=0; n <= p_end; i++,n++) {
	p_line[n] = tp_line[i];
	p_Char[n] = tp_char[i];
	if (p_Char[n] == '-')
	    p_Char[n] = '+';
	p_len[n] = tp_len[i];
    }
    assert(i == p_ptrn_lines + 1);
    i = p_ptrn_lines;
    p_ptrn_lines = p_repl_lines;
    p_repl_lines = i;
#ifndef lint
    if (tp_line == Null(char**))
	free((char*)tp_line);
    if (tp_len == Null(short*))
	free((char*)tp_len);
#endif
    if (tp_char == Nullch)
	free((char*)tp_char);
    return TRUE;
}

/* Return the specified line position in the old file of the old context. */

LINENUM
pch_first()
{
    return p_first;
}

/* Return the number of lines of old context. */

LINENUM
pch_ptrn_lines()
{
    return p_ptrn_lines;
}

/* Return the probable line position in the new file of the first line. */

LINENUM
pch_newfirst()
{
    return p_newfirst;
}

/* Return the number of lines in the replacement text including context. */

LINENUM
pch_repl_lines()
{
    return p_repl_lines;
}

/* Return the number of lines in the whole hunk. */

LINENUM
pch_end()
{
    return p_end;
}

/* Return the number of context lines before the first changed line. */

LINENUM
pch_context()
{
    return p_context;
}

/* Return the length of a particular patch line. */

short
pch_line_len(line)
LINENUM line;
{
    return p_len[line];
}

/* Return the control character (+, -, *, !, etc) for a patch line. */

char
pch_char(line)
LINENUM line;
{
    return p_Char[line];
}

/* Return a pointer to a particular patch line. */

char *
pfetch(line)
LINENUM line;
{
    return p_line[line];
}

/* Return where in the patch file this hunk began, for error messages. */

LINENUM
pch_hunk_beg()
{
    return p_hunk_beg;
}

/* Apply an ed script by feeding ed itself. */

void
do_ed_script()
{
    Reg1 char *t;
    Reg2 long beginning_of_this_line;
    Reg3 bool this_line_is_command = FALSE;
    Reg4 FILE *pipefp;

    if (!skip_rest_of_patch) {
	Unlink(TMPOUTNAME);
	copy_file(filearg[0], TMPOUTNAME);
	if (verbose)
	    Sprintf(buf, "/bin/ed %s", TMPOUTNAME);
	else
	    Sprintf(buf, "/bin/ed - %s", TMPOUTNAME);
	pipefp = popen(buf, "w");
    }
    for (;;) {
	beginning_of_this_line = ftell(pfp);
	if (pgets(buf, sizeof buf, pfp) == Nullch) {
	    next_intuit_at(beginning_of_this_line,p_input_line);
	    break;
	}
	p_input_line++;
	for (t=buf; isdigit((unsigned char)*t) || *t == ','; t++) ;
	this_line_is_command = (isdigit((unsigned char)*buf) &&
	  (*t == 'd' || *t == 'c' || *t == 'a') );
	if (this_line_is_command) {
	    if (!skip_rest_of_patch)
		fputs(buf, pipefp);
	    if (*t != 'd') {
		while (pgets(buf, sizeof buf, pfp) != Nullch) {
		    p_input_line++;
		    if (!skip_rest_of_patch)
			fputs(buf, pipefp);
		    if (strEQ(buf, ".\n"))
			break;
		}
	    }
	}
	else {
	    next_intuit_at(beginning_of_this_line,p_input_line);
	    break;
	}
    }
    if (skip_rest_of_patch)
	return;
    fprintf(pipefp, "w\n");
    fprintf(pipefp, "q\n");
    Fflush(pipefp);
    Pclose(pipefp);
    ignore_signals();
    if (move_file(TMPOUTNAME, outname) < 0) {
	toutkeep = TRUE;
	chmod(TMPOUTNAME, filemode);
    }
    else
	chmod(outname, filemode);
    set_signals(1);
}
