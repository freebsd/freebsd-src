/* $Header: /home/ncvs/src/gnu/usr.bin/patch/inp.c,v 1.5 1997/03/17 01:44:40 jmg Exp $
 *
 * $Log: inp.c,v $
 * Revision 1.5  1997/03/17 01:44:40  jmg
 * fix compilation warnings in patch... (with slight modification)
 *
 * also remove -Wall that I acidentally committed last time I was here...
 *
 * Submitted-by: Philippe Charnier
 *
 * Closes PR#2998
 *
 * Revision 1.4  1997/02/13 21:10:39  jmg
 * Fix a problem with patch in that is will always default, even when the
 * controlling terminal is closed.  Now the function ask() will return 1 when th
 * input is known to come from a file or terminal, or it will return 0 when ther
 * was a read error.
 *
 * Modified the question "Skip patch?" so that on an error from ask it will skip
 * the patch instead of looping.
 *
 * Closes PR#777
 *
 * 2.2 candidate
 *
 * Revision 1.3  1995/05/30 05:02:31  rgrimes
 * Remove trailing whitespace.
 *
 * Revision 1.2  1995/01/12  22:09:39  hsu
 * Fix bug that created new files even when running in -C check mode.
 * Reviewed by: phk
 *
 * Revision 1.1.1.1  1993/06/19  14:21:52  paul
 * b-maked patch-2.10
 *
 * Revision 2.0.1.1  88/06/03  15:06:13  lwall
 * patch10: made a little smarter about sccs files
 *
 * Revision 2.0  86/09/17  15:37:02  lwall
 * Baseline for netwide release.
 *
 */

#include "EXTERN.h"
#include "common.h"
#include "util.h"
#include "pch.h"
#include "INTERN.h"
#include "inp.h"

/* Input-file-with-indexable-lines abstract type */

static long i_size;			/* size of the input file */
static char *i_womp;			/* plan a buffer for entire file */
static char **i_ptr;			/* pointers to lines in i_womp */

static int tifd = -1;			/* plan b virtual string array */
static char *tibuf[2];			/* plan b buffers */
static LINENUM tiline[2] = {-1, -1};	/* 1st line in each buffer */
static LINENUM lines_per_buf;		/* how many lines per buffer */
static int tireclen;			/* length of records in tmp file */

/* New patch--prepare to edit another file. */

void
re_input()
{
    if (using_plan_a) {
	i_size = 0;
#ifndef lint
	if (i_ptr != Null(char**))
	    free((char *)i_ptr);
#endif
	if (i_womp != Nullch)
	    free(i_womp);
	i_womp = Nullch;
	i_ptr = Null(char **);
    }
    else {
	using_plan_a = TRUE;		/* maybe the next one is smaller */
	Close(tifd);
	tifd = -1;
	free(tibuf[0]);
	free(tibuf[1]);
	tibuf[0] = tibuf[1] = Nullch;
	tiline[0] = tiline[1] = -1;
	tireclen = 0;
    }
}

/* Constuct the line index, somehow or other. */

void
scan_input(filename)
char *filename;
{
    if (!plan_a(filename))
	plan_b(filename);
    if (verbose) {
	say3("Patching file %s using Plan %s...\n", filename,
	  (using_plan_a ? "A" : "B") );
    }
}

/* Try keeping everything in memory. */

bool
plan_a(filename)
char *filename;
{
    int ifd, statfailed;
    Reg1 char *s;
    Reg2 LINENUM iline;
    char lbuf[MAXLINELEN];
    int output_elsewhere = strcmp(filename, outname);
    extern int check_patch;

    statfailed = stat(filename, &filestat);
    if (statfailed && ok_to_create_file) {
	if (verbose)
	    say2("(Creating file %s...)\n",filename);
	if (check_patch)
	  return TRUE;
	makedirs(filename, TRUE);
	close(creat(filename, 0666));
	statfailed = stat(filename, &filestat);
    }
    if (statfailed && check_patch) {
	fatal2("%s not found and in check_patch mode.", filename);
    }
    /* For nonexistent or read-only files, look for RCS or SCCS versions.  */
    if (statfailed
	|| (! output_elsewhere
	    && (/* No one can write to it.  */
		(filestat.st_mode & 0222) == 0
		/* I can't write to it.  */
		|| ((filestat.st_mode & 0022) == 0
		    && filestat.st_uid != myuid)))) {
	struct stat cstat;
	char *cs = Nullch;
	char *filebase;
	int pathlen;

	filebase = basename(filename);
	pathlen = filebase - filename;

	/* Put any leading path into `s'.
	   Leave room in lbuf for the diff command.  */
	s = lbuf + 20;
	strncpy(s, filename, pathlen);

#define try(f,a1,a2) (Sprintf(s + pathlen, f, a1, a2), stat(s, &cstat) == 0)
	if ((   try("RCS/%s%s", filebase, RCSSUFFIX)
	     || try("RCS/%s%s", filebase,        "")
	     || try(    "%s%s", filebase, RCSSUFFIX))
	    &&
	    /* Check that RCS file is not working file.
	       Some hosts don't report file name length errors.  */
	    (statfailed
	     || (  (filestat.st_dev ^ cstat.st_dev)
		 | (filestat.st_ino ^ cstat.st_ino)))) {
	    Sprintf(buf, output_elsewhere?CHECKOUT:CHECKOUT_LOCKED, filename);
	    Sprintf(lbuf, RCSDIFF, filename);
	    cs = "RCS";
	} else if (   try("SCCS/%s%s", SCCSPREFIX, filebase)
		   || try(     "%s%s", SCCSPREFIX, filebase)) {
	    Sprintf(buf, output_elsewhere?GET:GET_LOCKED, s);
	    Sprintf(lbuf, SCCSDIFF, s, filename);
	    cs = "SCCS";
	} else if (statfailed)
	    fatal2("can't find %s\n", filename);
	/* else we can't write to it but it's not under a version
	   control system, so just proceed.  */
	if (cs) {
	    if (!statfailed) {
		if ((filestat.st_mode & 0222) != 0)
		    /* The owner can write to it.  */
		    fatal3("file %s seems to be locked by somebody else under %s\n",
			   filename, cs);
		/* It might be checked out unlocked.  See if it's safe to
		   check out the default version locked.  */
		if (verbose)
		    say3("Comparing file %s to default %s version...\n",
			 filename, cs);
		if (system(lbuf))
		    fatal3("can't check out file %s: differs from default %s version\n",
			   filename, cs);
	    }
	    if (verbose)
		say3("Checking out file %s from %s...\n", filename, cs);
	    if (system(buf) || stat(filename, &filestat))
		fatal3("can't check out file %s from %s\n", filename, cs);
	}
    }
    filemode = filestat.st_mode;
    if (!S_ISREG(filemode))
	fatal2("%s is not a normal file--can't patch\n", filename);
    i_size = filestat.st_size;
    if (out_of_mem) {
	set_hunkmax();		/* make sure dynamic arrays are allocated */
	out_of_mem = FALSE;
	return FALSE;			/* force plan b because plan a bombed */
    }
#ifdef lint
    i_womp = Nullch;
#else
    i_womp = malloc((MEM)(i_size+2));	/* lint says this may alloc less than */
					/* i_size, but that's okay, I think. */
#endif
    if (i_womp == Nullch)
	return FALSE;
    if ((ifd = open(filename, 0)) < 0)
	pfatal2("can't open file %s", filename);
#ifndef lint
    if (read(ifd, i_womp, (int)i_size) != i_size) {
	Close(ifd);	/* probably means i_size > 15 or 16 bits worth */
	free(i_womp);	/* at this point it doesn't matter if i_womp was */
	return FALSE;	/*   undersized. */
    }
#endif
    Close(ifd);
    if (i_size && i_womp[i_size-1] != '\n')
	i_womp[i_size++] = '\n';
    i_womp[i_size] = '\0';

    /* count the lines in the buffer so we know how many pointers we need */

    iline = 0;
    for (s=i_womp; *s; s++) {
	if (*s == '\n')
	    iline++;
    }
#ifdef lint
    i_ptr = Null(char**);
#else
    i_ptr = (char **)malloc((MEM)((iline + 2) * sizeof(char *)));
#endif
    if (i_ptr == Null(char **)) {	/* shucks, it was a near thing */
	free((char *)i_womp);
	return FALSE;
    }

    /* now scan the buffer and build pointer array */

    iline = 1;
    i_ptr[iline] = i_womp;
    for (s=i_womp; *s; s++) {
	if (*s == '\n')
	    i_ptr[++iline] = s+1;	/* these are NOT null terminated */
    }
    input_lines = iline - 1;

    /* now check for revision, if any */

    if (revision != Nullch) {
	if (!rev_in_string(i_womp)) {
	    if (force) {
		if (verbose)
		    say2(
"Warning: this file doesn't appear to be the %s version--patching anyway.\n",
			revision);
	    }
	    else if (batch) {
		fatal2(
"this file doesn't appear to be the %s version--aborting.\n", revision);
	    }
	    else {
		(void) ask2(
"This file doesn't appear to be the %s version--patch anyway? [n] ",
		    revision);
	    if (*buf != 'y')
		fatal1("aborted\n");
	    }
	}
	else if (verbose)
	    say2("Good.  This file appears to be the %s version.\n",
		revision);
    }
    return TRUE;			/* plan a will work */
}

/* Keep (virtually) nothing in memory. */

void
plan_b(filename)
char *filename;
{
    Reg3 FILE *ifp;
    Reg1 int i = 0;
    Reg2 int maxlen = 1;
    Reg4 bool found_revision = (revision == Nullch);

    using_plan_a = FALSE;
    if ((ifp = fopen(filename, "r")) == Nullfp)
	pfatal2("can't open file %s", filename);
    if ((tifd = creat(TMPINNAME, 0666)) < 0)
	pfatal2("can't open file %s", TMPINNAME);
    while (fgets(buf, sizeof buf, ifp) != Nullch) {
	if (revision != Nullch && !found_revision && rev_in_string(buf))
	    found_revision = TRUE;
	if ((i = strlen(buf)) > maxlen)
	    maxlen = i;			/* find longest line */
    }
    if (revision != Nullch) {
	if (!found_revision) {
	    if (force) {
		if (verbose)
		    say2(
"Warning: this file doesn't appear to be the %s version--patching anyway.\n",
			revision);
	    }
	    else if (batch) {
		fatal2(
"this file doesn't appear to be the %s version--aborting.\n", revision);
	    }
	    else {
		(void) ask2(
"This file doesn't appear to be the %s version--patch anyway? [n] ",
		    revision);
		if (*buf != 'y')
		    fatal1("aborted\n");
	    }
	}
	else if (verbose)
	    say2("Good.  This file appears to be the %s version.\n",
		revision);
    }
    Fseek(ifp, 0L, 0);		/* rewind file */
    lines_per_buf = BUFFERSIZE / maxlen;
    tireclen = maxlen;
    tibuf[0] = malloc((MEM)(BUFFERSIZE + 1));
    tibuf[1] = malloc((MEM)(BUFFERSIZE + 1));
    if (tibuf[1] == Nullch)
	fatal1("out of memory\n");
    for (i=1; ; i++) {
	if (! (i % lines_per_buf))	/* new block */
	    if (write(tifd, tibuf[0], BUFFERSIZE) < BUFFERSIZE)
		pfatal1("can't write temp file");
	if (fgets(tibuf[0] + maxlen * (i%lines_per_buf), maxlen + 1, ifp)
	  == Nullch) {
	    input_lines = i - 1;
	    if (i % lines_per_buf)
		if (write(tifd, tibuf[0], BUFFERSIZE) < BUFFERSIZE)
		    pfatal1("can't write temp file");
	    break;
	}
    }
    Fclose(ifp);
    Close(tifd);
    if ((tifd = open(TMPINNAME, 0)) < 0) {
	pfatal2("can't reopen file %s", TMPINNAME);
    }
}

/* Fetch a line from the input file, \n terminated, not necessarily \0. */

char *
ifetch(line,whichbuf)
Reg1 LINENUM line;
int whichbuf;				/* ignored when file in memory */
{
    if (line < 1 || line > input_lines)
	return "";
    if (using_plan_a)
	return i_ptr[line];
    else {
	LINENUM offline = line % lines_per_buf;
	LINENUM baseline = line - offline;

	if (tiline[0] == baseline)
	    whichbuf = 0;
	else if (tiline[1] == baseline)
	    whichbuf = 1;
	else {
	    tiline[whichbuf] = baseline;
#ifndef lint		/* complains of long accuracy */
	    Lseek(tifd, (long)baseline / lines_per_buf * BUFFERSIZE, 0);
#endif
	    if (read(tifd, tibuf[whichbuf], BUFFERSIZE) < 0)
		pfatal2("error reading tmp file %s", TMPINNAME);
	}
	return tibuf[whichbuf] + (tireclen*offline);
    }
}

/* True if the string argument contains the revision number we want. */

bool
rev_in_string(string)
char *string;
{
    Reg1 char *s;
    Reg2 int patlen;

    if (revision == Nullch)
	return TRUE;
    patlen = strlen(revision);
    if (strnEQ(string,revision,patlen) && isspace((unsigned char)string[patlen]))
	return TRUE;
    for (s = string; *s; s++) {
	if (isspace((unsigned char)*s) && strnEQ(s+1, revision, patlen) &&
		isspace((unsigned char)s[patlen+1] )) {
	    return TRUE;
	}
    }
    return FALSE;
}
