/* inputting files to be patched */

/* $Id: inp.c,v 1.18 1997/07/21 17:59:46 eggert Exp $ */

/*
Copyright 1986, 1988 Larry Wall
Copyright 1991, 1992, 1993, 1997 Free Software Foundation, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; see the file COPYING.
If not, write to the Free Software Foundation,
59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#define XTERN extern
#include <common.h>
#include <backupfile.h>
#include <pch.h>
#include <util.h>
#undef XTERN
#define XTERN
#include <inp.h>

/* Input-file-with-indexable-lines abstract type */

static char *i_buffer;			/* plan A buffer */
static char const **i_ptr;		/* pointers to lines in plan A buffer */

static size_t tibufsize;		/* size of plan b buffers */
#ifndef TIBUFSIZE_MINIMUM
#define TIBUFSIZE_MINIMUM (8 * 1024)	/* minimum value for tibufsize */
#endif
static int tifd = -1;			/* plan b virtual string array */
static char *tibuf[2];			/* plan b buffers */
static LINENUM tiline[2] = {-1, -1};	/* 1st line in each buffer */
static LINENUM lines_per_buf;		/* how many lines per buffer */
static size_t tireclen;			/* length of records in tmp file */
static size_t last_line_size;		/* size of last input line */

static bool plan_a PARAMS ((char const *));/* yield FALSE if memory runs out */
static void plan_b PARAMS ((char const *));
static void report_revision PARAMS ((int));
static void too_many_lines PARAMS ((char const *)) __attribute__((noreturn));

/* New patch--prepare to edit another file. */

void
re_input()
{
    if (using_plan_a) {
	free (i_buffer);
	free (i_ptr);
    }
    else {
	close (tifd);
	tifd = -1;
	free(tibuf[0]);
	tibuf[0] = 0;
	tiline[0] = tiline[1] = -1;
	tireclen = 0;
    }
}

/* Construct the line index, somehow or other. */

void
scan_input(filename)
char *filename;
{
    using_plan_a = ! (debug & 16) && plan_a (filename);
    if (!using_plan_a)
	plan_b(filename);
    switch (verbosity)
      {
      case SILENT:
	break;

      case VERBOSE:
	say ("Patching file `%s' using Plan %s...\n",
	     filename, using_plan_a ? "A" : "B");
	break;

      case DEFAULT_VERBOSITY:
	say ("patching file `%s'\n", filename);
	break;
      }
}

/* Report whether a desired revision was found.  */

static void
report_revision (found_revision)
     int found_revision;
{
  if (found_revision)
    {
      if (verbosity == VERBOSE)
	say ("Good.  This file appears to be the %s version.\n", revision);
    }
  else if (force)
    {
      if (verbosity != SILENT)
	say ("Warning: this file doesn't appear to be the %s version -- patching anyway.\n",
	     revision);
    }
  else if (batch)
    {
      fatal ("This file doesn't appear to be the %s version -- aborting.",
	     revision);
    }
  else
    {
      ask ("This file doesn't appear to be the %s version -- patch anyway? [n] ",
	   revision);
      if (*buf != 'y')
	fatal ("aborted");
    }
}


static void
too_many_lines (filename)
     char const *filename;
{
  fatal ("File `%s' has too many lines.", filename);
}


void
get_input_file (filename, outname)
     char const *filename;
     char const *outname;
{
    int elsewhere = strcmp (filename, outname);
    char const *cs;
    char *diffbuf;
    char *getbuf;

    if (inerrno == -1)
      inerrno = stat (inname, &instat) == 0 ? 0 : errno;

    /* Perhaps look for RCS or SCCS versions.  */
    if (patch_get
	&& invc != 0
	&& (inerrno
	    || (! elsewhere
		&& (/* No one can write to it.  */
		    (instat.st_mode & (S_IWUSR|S_IWGRP|S_IWOTH)) == 0
		    /* Only the owner (who's not me) can write to it.  */
		    || ((instat.st_mode & (S_IWGRP|S_IWOTH)) == 0
			&& instat.st_uid != geteuid ()))))
	&& (invc = !! (cs = (version_controller
			     (filename, elsewhere,
			      inerrno ? (struct stat *) 0 : &instat,
			      &getbuf, &diffbuf))))) {

	    if (!inerrno) {
		if (!elsewhere
		    && (instat.st_mode & (S_IWUSR|S_IWGRP|S_IWOTH)) != 0)
		    /* Somebody can write to it.  */
		    fatal ("file `%s' seems to be locked by somebody else under %s",
			   filename, cs);
		/* It might be checked out unlocked.  See if it's safe to
		   check out the default version locked.  */
		if (verbosity == VERBOSE)
		    say ("Comparing file `%s' to default %s version...\n",
			 filename, cs);
		if (systemic (diffbuf) != 0)
		  {
		    say ("warning: patching file `%s', which does not match default %s version\n",
			 filename, cs);
		    cs = 0;
		  }
	    }

	    if (cs && version_get (filename, cs, ! inerrno, elsewhere, getbuf,
				   &instat))
	      inerrno = 0;

	    free (getbuf);
	    free (diffbuf);

    } else if (inerrno && !pch_says_nonexistent (reverse))
      {
	errno = inerrno;
	pfatal ("can't find file `%s'", filename);
      }

    if (inerrno)
      {
	instat.st_mode = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH;
	instat.st_size = 0;
      }
    else if (! S_ISREG (instat.st_mode))
      fatal ("`%s' is not a regular file -- can't patch", filename);
}


/* Try keeping everything in memory. */

static bool
plan_a(filename)
     char const *filename;
{
  register char const *s;
  register char const *lim;
  register char const **ptr;
  register char *buffer;
  register LINENUM iline;
  size_t size = instat.st_size;

  /* Fail if the file size doesn't fit in a size_t,
     or if storage isn't available.  */
  if (! (size == instat.st_size
	 && (buffer = malloc (size ? size : (size_t) 1))))
    return FALSE;

  /* Read the input file, but don't bother reading it if it's empty.
     When creating files, the files do not actually exist.  */
  if (size)
    {
      int ifd = open (filename, O_RDONLY|binary_transput);
      size_t buffered = 0, n;
      if (ifd < 0)
	pfatal ("can't open file `%s'", filename);

      while (size - buffered != 0)
	{
	  n = read (ifd, buffer + buffered, size - buffered);
	  if (n == 0)
	    {
	      /* Some non-POSIX hosts exaggerate st_size in text mode;
		 or the file may have shrunk!  */
	      size = buffered;
	      break;
	    }
	  if (n == (size_t) -1)
	    {
	      /* Perhaps size is too large for this host.  */
	      close (ifd);
	      free (buffer);
	      return FALSE;
	    }
	  buffered += n;
	}

      if (close (ifd) != 0)
	read_fatal ();
    }

  /* Scan the buffer and build array of pointers to lines.  */
  lim = buffer + size;
  iline = 3; /* 1 unused, 1 for SOF, 1 for EOF if last line is incomplete */
  for (s = buffer;  (s = (char *) memchr (s, '\n', lim - s));  s++)
    if (++iline < 0)
      too_many_lines (filename);
  if (! (iline == (size_t) iline
	 && (size_t) iline * sizeof *ptr / sizeof *ptr == (size_t) iline
	 && (ptr = (char const **) malloc ((size_t) iline * sizeof *ptr))))
    {
      free (buffer);
      return FALSE;
    }
  iline = 0;
  for (s = buffer;  ;  s++)
    {
      ptr[++iline] = s;
      if (! (s = (char *) memchr (s, '\n', lim - s)))
	break;
    }
  if (size && lim[-1] != '\n')
    ptr[++iline] = lim;
  input_lines = iline - 1;

  if (revision)
    {
      char const *rev = revision;
      int rev0 = rev[0];
      int found_revision = 0;
      size_t revlen = strlen (rev);

      if (revlen <= size)
	{
	  char const *limrev = lim - revlen;

	  for (s = buffer;  (s = (char *) memchr (s, rev0, limrev - s));  s++)
	    if (memcmp (s, rev, revlen) == 0
		&& (s == buffer || ISSPACE ((unsigned char) s[-1]))
		&& (s + 1 == limrev || ISSPACE ((unsigned char) s[revlen])))
	      {
		found_revision = 1;
		break;
	      }
	}

      report_revision (found_revision);
    }

  /* Plan A will work.  */
  i_buffer = buffer;
  i_ptr = ptr;
  return TRUE;
}

/* Keep (virtually) nothing in memory. */

static void
plan_b(filename)
     char const *filename;
{
  register FILE *ifp;
  register int c;
  register size_t len;
  register size_t maxlen;
  register int found_revision;
  register size_t i;
  register char const *rev;
  register size_t revlen;
  register LINENUM line = 1;

  if (instat.st_size == 0)
    filename = NULL_DEVICE;
  if (! (ifp = fopen (filename, binary_transput ? "rb" : "r")))
    pfatal ("can't open file `%s'", filename);
  tifd = create_file (TMPINNAME, O_RDWR | O_BINARY, (mode_t) 0);
  i = 0;
  len = 0;
  maxlen = 1;
  rev = revision;
  found_revision = !rev;
  revlen = rev ? strlen (rev) : 0;

  while ((c = getc (ifp)) != EOF)
    {
      len++;

      if (c == '\n')
	{
	  if (++line < 0)
	    too_many_lines (filename);
	  if (maxlen < len)
	      maxlen = len;
	  len = 0;
	}

      if (!found_revision)
	{
	  if (i == revlen)
	    {
	      found_revision = ISSPACE ((unsigned char) c);
	      i = (size_t) -1;
	    }
	  else if (i != (size_t) -1)
	    i = rev[i]==c ? i + 1 : (size_t) -1;

	  if (i == (size_t) -1  &&  ISSPACE ((unsigned char) c))
	    i = 0;
	}
    }

  if (revision)
    report_revision (found_revision);
  Fseek (ifp, (off_t) 0, SEEK_SET);		/* rewind file */
  for (tibufsize = TIBUFSIZE_MINIMUM;  tibufsize < maxlen;  tibufsize <<= 1)
    continue;
  lines_per_buf = tibufsize / maxlen;
  tireclen = maxlen;
  tibuf[0] = xmalloc (2 * tibufsize);
  tibuf[1] = tibuf[0] + tibufsize;

  for (line = 1; ; line++)
    {
      char *p = tibuf[0] + maxlen * (line % lines_per_buf);
      char const *p0 = p;
      if (! (line % lines_per_buf))	/* new block */
	if (write (tifd, tibuf[0], tibufsize) != tibufsize)
	  write_fatal ();
      if ((c = getc (ifp)) == EOF)
	break;

      for (;;)
	{
	  *p++ = c;
	  if (c == '\n')
	    {
	      last_line_size = p - p0;
	      break;
	    }

	  if ((c = getc (ifp)) == EOF)
	    {
	      last_line_size = p - p0;
	      line++;
	      goto EOF_reached;
	    }
	}
    }
 EOF_reached:
  if (ferror (ifp)  ||  fclose (ifp) != 0)
    read_fatal ();

  if (line % lines_per_buf  !=  0)
    if (write (tifd, tibuf[0], tibufsize) != tibufsize)
      write_fatal ();
  input_lines = line - 1;
}

/* Fetch a line from the input file. */

char const *
ifetch (line, whichbuf, psize)
register LINENUM line;
int whichbuf;				/* ignored when file in memory */
size_t *psize;
{
    register char const *q;
    register char const *p;

    if (line < 1 || line > input_lines) {
	*psize = 0;
	return "";
    }
    if (using_plan_a) {
	p = i_ptr[line];
	*psize = i_ptr[line + 1] - p;
	return p;
    } else {
	LINENUM offline = line % lines_per_buf;
	LINENUM baseline = line - offline;

	if (tiline[0] == baseline)
	    whichbuf = 0;
	else if (tiline[1] == baseline)
	    whichbuf = 1;
	else {
	    tiline[whichbuf] = baseline;
	    if (lseek (tifd, (off_t) (baseline/lines_per_buf * tibufsize),
		       SEEK_SET) == -1
		|| read (tifd, tibuf[whichbuf], tibufsize) < 0)
	      read_fatal ();
	}
	p = tibuf[whichbuf] + (tireclen*offline);
	if (line == input_lines)
	    *psize = last_line_size;
	else {
	    for (q = p;  *q++ != '\n';  )
		continue;
	    *psize = q - p;
	}
	return p;
    }
}
