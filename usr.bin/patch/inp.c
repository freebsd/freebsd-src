/* $Header: inp.c,v 2.0 86/09/17 15:37:02 lwall Exp $
 *
 * $Log:	inp.c,v $
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
    int ifd;
    Reg1 char *s;
    Reg2 LINENUM iline;

    if (ok_to_create_file && stat(filename, &filestat) < 0) {
	if (verbose)
	    say2("(Creating file %s...)\n",filename);
	makedirs(filename, TRUE);
	close(creat(filename, 0666));
    }
    if (stat(filename, &filestat) < 0) {
	Sprintf(buf, "RCS/%s%s", filename, RCSSUFFIX);
	if (stat(buf, &filestat) >= 0 || stat(buf+4, &filestat) >= 0) {
	    Sprintf(buf, CHECKOUT, filename);
	    if (verbose)
		say2("Can't find %s--attempting to check it out from RCS.\n",
		    filename);
	    if (system(buf) || stat(filename, &filestat))
		fatal2("Can't check out %s.\n", filename);
	}
	else {
	    Sprintf(buf, "SCCS/%s%s", SCCSPREFIX, filename);
	    if (stat(buf, &filestat) >= 0 || stat(buf+5, &filestat) >= 0) {
		Sprintf(buf, GET, filename);
		if (verbose)
		    say2("Can't find %s--attempting to get it from SCCS.\n",
			filename);
		if (system(buf) || stat(filename, &filestat))
		    fatal2("Can't get %s.\n", filename);
	    }
	    else
		fatal2("Can't find %s.\n", filename);
	}
    }
    filemode = filestat.st_mode;
    if ((filemode & S_IFMT) & ~S_IFREG)
	fatal2("%s is not a normal file--can't patch.\n", filename);
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
	fatal2("Can't open file %s\n", filename);
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
		    say2("\
Warning: this file doesn't appear to be the %s version--patching anyway.\n",
			revision);
	    }
	    else {
		ask2("\
This file doesn't appear to be the %s version--patch anyway? [n] ",
		    revision);
	    if (*buf != 'y')
		fatal1("Aborted.\n");
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
	fatal2("Can't open file %s\n", filename);
    if ((tifd = creat(TMPINNAME, 0666)) < 0)
	fatal2("Can't open file %s\n", TMPINNAME);
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
		    say2("\
Warning: this file doesn't appear to be the %s version--patching anyway.\n",
			revision);
	    }
	    else {
		ask2("\
This file doesn't appear to be the %s version--patch anyway? [n] ",
		    revision);
		if (*buf != 'y')
		    fatal1("Aborted.\n");
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
	fatal1("Can't seem to get enough memory.\n");
    for (i=1; ; i++) {
	if (! (i % lines_per_buf))	/* new block */
	    if (write(tifd, tibuf[0], BUFFERSIZE) < BUFFERSIZE)
		fatal1("patch: can't write temp file.\n");
	if (fgets(tibuf[0] + maxlen * (i%lines_per_buf), maxlen + 1, ifp)
	  == Nullch) {
	    input_lines = i - 1;
	    if (i % lines_per_buf)
		if (write(tifd, tibuf[0], BUFFERSIZE) < BUFFERSIZE)
		    fatal1("patch: can't write temp file.\n");
	    break;
	}
    }
    Fclose(ifp);
    Close(tifd);
    if ((tifd = open(TMPINNAME, 0)) < 0) {
	fatal2("Can't reopen file %s\n", TMPINNAME);
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
	    Lseek(tifd, (off_t)baseline / lines_per_buf * BUFFERSIZE, 0);
#endif
	    if (read(tifd, tibuf[whichbuf], BUFFERSIZE) < 0)
		fatal2("Error reading tmp file %s.\n", TMPINNAME);
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
    for (s = string; *s; s++) {
	if (isspace(*s) && strnEQ(s+1, revision, patlen) && 
		isspace(s[patlen+1] )) {
	    return TRUE;
	}
    }
    return FALSE;
}

