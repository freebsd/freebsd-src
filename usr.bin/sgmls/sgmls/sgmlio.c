/* sgmlio.c -
   IO functions for core parser.

   Written by James Clark (jjc@jclark.com).
*/

/* SGML must see a file in which records start with RS and end with
   RE, and EOFCHAR (Ctl-Z) is present at the end.  This module must
   supply these characters if they are not naturally present in the
   file.  SGML will open two files at a time: when an entity is
   nested, the new file is opened before closing the old in order to
   make sure the open is successful. If it is, the original open file
   is closed temporarily (IOPEND); when the stack is popped, the new
   file is closed and the original file is re-opened (IOCONT). SGML
   will check error returns for the initial open of a file and all
   reads, and for re-openings when the stack is popped, but not for
   closes.  Returning <0 indicates an error; 0 or more is a successful
   operation, except for IOREAD where the return value is the number
   of characters read, and must exceed 0 to be successful.  The first
   READ must always be successful, and normally consists of just
   priming the buffer with EOBCHAR (or RS EOBCHAR).  SGMLIO must
   assure that there is an EOBCHAR at the end of each block read,
   except for the last block of the entity, which must have an
   EOFCHAR.

   SGML views an entity as a contiguous whole, without regard to its
   actual form of storage.  SGMLIO supports entities that are
   equivalent to a single file of one or more records, or to a
   concatenation of files.
*/

/* Uses only stream I/O.  This module should be portable to most ANSI
   systems. */
/* We try to ensure that if an IO operation fails, then errno will contain
   a meaningful value (although it may be zero.) */

#include "config.h"
#ifdef HAVE_O_NOINHERIT
#include <fcntl.h>
#include <io.h>
#endif /* HAVE_O_NOINHERIT */

#include "sgmlaux.h"          /* Include files for auxiliary functions.. */

#ifdef HAVE_O_NOINHERIT
#define FOPENR(file) nifopen(file)
FILE *nifopen P((char *));
#else /* not HAVE_O_NOINHERIT */
#define FOPENR(file) fopen((file), "r")
#endif /* not HAVE_O_NOINHERIT */

struct iofcb {                /* I/O file control block. */
     FILE *fp;		      /* File handle. */
     fpos_t off;              /* Offset in file of current read block. */
     char *next;              /* Next file (NULL if no more). */
     char *file;              /* Current file (no length byte). */
     int pendoff;	      /* Offset into line when file suspended. */
     char bol;	              /* Non-zero if currently at beginning of line. */
     char first;	      /* Non-zero if the first read.  */
     char wasbol;	      /* Non-zero if current block was at beginning of line. */
     char canseek;
     UNCH *pendbuf;	      /* Saved partial buffer for suspended file
				 that can't be closed and reopened. */
};

static char *lastfile;	      /* The name of the last file closed. */
static int bufsize;	      /* Size of buffer passed to ioread(). */
static char ismagic[256];     /* Table of magic chars that need to be prefixed
				 by DELNONCH. */
static int stdinused = 0;

static char *nextstr P((char *)); /* Iterate over list of strings. */
static FILE *openfile P((char *, char *));
static int closefile P((FILE *));
static int isreg P((FILE *));

VOID ioinit(swp)
struct switches *swp;
{
     ismagic[EOBCHAR] = 1;
     ismagic[EOFCHAR] = 1;
     ismagic[EOS] = 1;
     ismagic[(UNCH)DELNONCH] = 1;
     ismagic[(UNCH)GENRECHAR] = 1;
     bufsize = swp->swbufsz;
}

int ioopen(id, pp)
UNIV id;
UNIV *pp;
{
     struct iofcb *f;
     char *s;
     errno = 0;
     if (!id)
	  return -1;
     s = id;
     if (!*s)
	  return -1;
     f = (struct iofcb *)rmalloc((UNS)sizeof(struct iofcb));
     f->file = s;
     f->next = nextstr(s);
     errno = 0;
     f->fp = openfile(f->file, &f->canseek);
     f->bol = 1;
     f->first = 1;
     f->pendbuf = 0;
     *pp = (UNIV)f;
     return f->fp ? 1 : -1;
}

VOID ioclose(p)
UNIV p;
{
     struct iofcb *f = (struct iofcb *)p;
     if (f->fp)
	  closefile(f->fp);
     lastfile = f->file;
     frem((UNIV)f);
}

VOID iopend(p, off, buf)
UNIV p;
int off;
UNCH *buf;
{
     struct iofcb *f = (struct iofcb *)p;
     if (!f->canseek) {
	  UNCH *s;
	  for (s = buf + off; *s != EOFCHAR && *s != EOBCHAR; s++)
	       ;
	  s++;
	  f->pendbuf = (UNCH *)rmalloc((UNS)(s - buf - off));
	  memcpy((UNIV)f->pendbuf, (UNIV)(buf + off), (UNS)(s - buf - off));
	  return;
     }
     f->bol = 0;
     if (f->wasbol) {
	  if (off == 0)
	       f->bol = 1;
	  else
	       off--;
     }
     f->pendoff = off;
     if (f->fp) {
	  fclose(f->fp);
	  f->fp = 0;
     }
}

int iocont(p)
UNIV p;
{
     struct iofcb *f = (struct iofcb *)p;
     int c = EOF;
     int off = f->pendoff;

     if (!f->canseek)
	  return 0;

     errno = 0;
     f->fp = FOPENR(f->file);
     if (!f->fp)
	  return -1;
     if (fsetpos(f->fp, &f->off))
	  return -1;
     while (--off >= 0) {
	  c = getc(f->fp);
	  if (c != EOF && ismagic[c])
	       off--;
     }
     if (c == '\n')
	  f->bol = 1;
     if (ferror(f->fp))
	  return -1;
     return 0;
}

/* Return -1 on error, otherwise the number of bytes read.  The
strategy is to concatenate the files, insert a RS at the beginning of
each line, and change each '\n' into a RE.  The returned data
shouldn't cross a file boundary, otherwise error messages might be
inaccurate.  The first read must always succeed. */

int ioread(p, buf, newfilep)
UNIV p;
UNCH *buf;
int *newfilep;
{
     int i = 0;
     struct iofcb *f = (struct iofcb *)p;
     FILE *fp;
     int c;
     
     *newfilep = 0;
     if (f->first) {
	  buf[i] = EOBCHAR;
	  f->first = 0;
	  return 1;
     }
     if (f->pendbuf) {
	  for (i = 0;
	       (buf[i] = f->pendbuf[i]) != EOBCHAR && buf[i] != EOFCHAR;
	       i++)
	       ;
	  frem((UNIV)f->pendbuf);
	  f->pendbuf = 0;
	  return i + 1;
     }
     fp = f->fp;
     for (;;) {
	  errno = 0;
	  if (f->canseek && fgetpos(fp, &f->off))
	       f->canseek = 0;
	  errno = 0;
	  c = getc(fp);
	  if (c != EOF)
	       break;
	  if (ferror(fp))
	       return -1;
	  if (closefile(fp) == EOF)
	       return -1;
	  if (!f->next){
	       f->fp = 0;
	       buf[0] = EOFCHAR;
	       return 1;
	  }
	  f->file = f->next;
	  f->next = nextstr(f->next);
	  *newfilep = 1;
	  errno = 0;
	  fp = f->fp = openfile(f->file, &f->canseek);
	  if (!fp)
	       return -1;
	  f->bol = 1;
     }
     if (f->bol) {
	  f->bol = 0;
	  buf[i++] = RSCHAR;
	  f->wasbol = 1;
     }
     else
	  f->wasbol = 0;
     errno = 0;
     for (;;) {
	  if (c == '\n') {
	       f->bol = 1;
	       buf[i++] = RECHAR;
	       break;
	  }
	  if (ismagic[c]) {
	       buf[i++] = DELNONCH;
	       buf[i++] = SHIFTNON(c);
	  }
	  else
	       buf[i++] = c;
	  if (i >= bufsize - 2)
	       break;
	  c = getc(fp);
	  if (c == EOF) {
	       if (ferror(fp))
		    return -1;
	       /* This is in the middle of a line. */
	       break;
	  }
     }
     buf[i++] = EOBCHAR;
     return i;
}

static char *nextstr(p)
char *p;
{
     p = strchr(p, '\0');
     return *++p ? p : 0;
}

/* Return the filename associated with p.  If p is NULL, return the filename
of the last file closed. */

char *ioflid(p)
UNIV p;
{
     if (!p)
	  return lastfile;
     return ((struct iofcb *)p)->file;
}

static
FILE *openfile(name, seekp)
char *name;
char *seekp;
{
     FILE *fp;
     if (strcmp(name, STDINNAME) == 0) {
	  if (stdinused)
	       return 0;
	  stdinused = 1;
	  *seekp = 0;
	  return stdin;
     }
     fp = FOPENR(name);
     if (fp)
	  *seekp = isreg(fp);
     return fp;
}

/* Return -1 on error, 0 otherwise. */

static
int closefile(fp)
FILE *fp;
{
     if (fp == stdin) {
	  stdinused = 0;
	  clearerr(fp);
	  return 0;
     }
     else
	  return fclose(fp);
}

#ifdef HAVE_O_NOINHERIT

/* This is the same as fopen(name, "r") except that it tells DOS that
the file descriptor should not be inherited by child processes.  */

FILE *nifopen(name)
char *name;
{
     int fd = open(name, O_RDONLY|O_NOINHERIT|O_TEXT);
     if (fd < 0)
	  return 0;
     return fdopen(fd, "r");
}

#endif /* HAVE_O_NOINHERIT */

#ifdef HAVE_SYS_STAT_H

#include <sys/types.h>
#include <sys/stat.h>

#ifndef S_ISREG
#ifdef S_IFMT
#ifdef S_IFREG
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif /* S_IFREG */
#endif /* S_IFMT */
#endif /* not S_ISREG */

#endif /* HAVE_SYS_STAT_H */

/* Return 1 if fp might be associated with a regular file.  0
otherwise.  We check this because on many Unix systems lseek() will
succeed on a (pseudo-)terminal although terminals aren't seekable in
the way we need. */

static
int isreg(fp)
FILE *fp;
{
#ifdef S_ISREG
     struct stat sb;

     /* This assumes that a system that has S_ISREG will also have
        fstat() and fileno(). */
     if (fstat(fileno(fp), &sb) == 0)
	  return S_ISREG(sb.st_mode);
#endif /* S_ISREG */
     return 1;
}


/*
Local Variables:
c-indent-level: 5
c-continued-statement-offset: 5
c-brace-offset: -5
c-argdecl-indent: 0
c-label-offset: -5
comment-column: 30
End:
*/
