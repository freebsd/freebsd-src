/*
 * Copyright (c) 1996 Joerg Wunsch
 *
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
 * THIS SOFTWARE IS PROVIDED BY THE DEVELOPERS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE DEVELOPERS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 *
 */

/*
 * Heuristics to convert old wtmp format to new one.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include <utmp.h>


#define	OUT_NAMESIZE	8
#define	OUT_LINESIZE	8
#define	OUT_HOSTSIZE	16

struct olastlog {
	time_t	ll_time;
	char	ll_line[OUT_LINESIZE];
	char	ll_host[OUT_HOSTSIZE];
};

struct outmp {
	char	ut_line[OUT_LINESIZE];
	char	ut_name[OUT_NAMESIZE];
	char	ut_host[OUT_HOSTSIZE];
	long	ut_time;
};

void	usage(void);
void	convert(const char *, int, int);

/*
 * NB: We cannot convert lastlog yet, but we don't need either.
 */

void
usage(void)
{
  errx(EX_USAGE, "usage: cvt-wtmp [-f] [-n] /var/log/wtmp*");
}


int
main(int argc, char **argv)
{
  int errs, i, nflag, forceflag, rv;

  errs = nflag = forceflag = 0;
  while ((i = getopt(argc, argv, "fn")) != -1)
    switch (i)
      {
      case 'f':
	forceflag++;
	break;

      case 'n':
	nflag++;
	break;

      default:
	errs++;
      }
  argc -= optind;
  argv += optind;

  if (argc <= 0 || errs)
    usage();

  for (;argc > 0; argc--, argv++)
    convert(*argv, nflag, forceflag);

  return 0;
}

void
convert(const char *name, int nflag, int forceflag)
{
  struct stat sb;
  struct timeval tv[2];
  char xname[1024], yname[1024];
  unsigned char buf[128];	/* large enough to hold one wtmp record */
  int fd1, fd2;
  size_t off, shouldbe;
  int old, new;
  time_t now, early, *t;
  struct tm tm;
  struct utmp u;
  struct outmp *ou;
  enum { OLD, NEW } which = OLD; /* what we're defaulting to */

  if (stat(name, &sb) == -1)
    {
      warn("Cannot stat file \"%s\", continuing.", name);
      return;
    }

  now = time(NULL);
  /* some point in time very early, before 386BSD 0.0 */
  tm.tm_sec = 0; tm.tm_min = 0; tm.tm_hour = 0;
  tm.tm_mday = 1; tm.tm_mon = 2; tm.tm_year = 92;
  tm.tm_isdst = 0;
  early = mktime(&tm);

  tv[0].tv_sec = sb.st_atimespec.tv_sec;
  tv[0].tv_usec = sb.st_atimespec.tv_nsec / 1000;
  tv[1].tv_sec = sb.st_mtimespec.tv_sec;
  tv[1].tv_usec = sb.st_mtimespec.tv_nsec / 1000;

  /* unzipping is handled best externally */
  if (strlen(name) > 3 && memcmp(&name[strlen(name) - 3], ".gz", 3) == 0)
    {
      warnx("Cannot handle gzipped files, ignoring \"%s\".", name);
      return;
    }

  (void) snprintf(xname, sizeof xname, "%s.new", name);
  if (!nflag && (fd1 = open(xname, O_WRONLY|O_CREAT|O_TRUNC, 0644)) == -1)
    err(EX_CANTCREAT, "Can't create new wtmp file");

  if ((fd2 = open(name, O_RDONLY, 0)) == -1)
    err(EX_UNAVAILABLE, "input file magically disappeared, i'm confused");

  old = new = 0; off = 0;
  memset(buf, 0, sizeof buf);

  while (read(fd2, &buf[off], sizeof(time_t)) == sizeof(time_t))
    {
      t = (time_t *)&buf[off];
      off += sizeof(time_t);
      if (off < sizeof(struct outmp))
	/* go on */
	continue;
      if (*t < early || *t > now)
	{
	  /* unreasonable, collect another entry */
	  if (off > sizeof buf)
	    {
	      if (!forceflag)
		{
	      	  (void) unlink(xname);
		  errx(EX_UNAVAILABLE, "I can't seem to make sense out of file \"%s\",\n"
		       "Could have forced using -f.",
		     name);
		}
	      else
		{
		  warnx("Record # %d in file \"%s\" seems bogus\n"
			"(time: %d, previous time: %d, now: %d),\n"
			"continuing anyway.",
			old + new + 1, name, *t, early, now);
		  if (which == NEW)
		    {
		      (void)lseek(fd2, sizeof(struct utmp) - sizeof buf, SEEK_CUR);
		      goto write_new;
		    }
		  else
		    {
		      (void)lseek(fd2, sizeof(struct outmp) - sizeof buf, SEEK_CUR);
		      goto write_old;
		    }
		}
	    }
	  continue;
	}
      /* time is reasonable, we seem to have collected a full entry */
      if (off == sizeof(struct utmp))
	{
	  /* new wtmp record */
	  which = NEW;
	write_new:
	  new++;
	  if (!nflag)
	    {
	      if (write(fd1, buf, sizeof(struct utmp)) != sizeof(struct utmp))
		err(EX_IOERR, "writing file \"%s\"", xname);
	    }
	}
      else if (off == sizeof(struct outmp))
	{
	  /* old fart */
	  which = OLD;
	write_old:
	  old++;
	  if (!nflag)
	    {
	      ou = (struct outmp *)buf;
	      memset(&u, 0, sizeof u);
	      memcpy(&u.ut_line, ou->ut_line, OUT_LINESIZE);
	      memcpy(&u.ut_name, ou->ut_name, OUT_NAMESIZE);
	      memcpy(&u.ut_host, ou->ut_host, OUT_HOSTSIZE);
	      memcpy(&u.ut_time, &ou->ut_time, sizeof u.ut_time);
	      if (write(fd1, &u, sizeof(struct utmp)) != sizeof(struct utmp))
		err(EX_IOERR, "writing file \"%s\"", xname);
	    }
	}
      else
	{
	  if (!forceflag)
	    {
	      warnx("Illegal record in file \"%s\", ignoring.", name);
	      off = 0;
	      continue;
	    }
	  else
	    {
	      warnx("Illegal record in file \"%s\", considering it %s one.",
		    name, (which == OLD? "an old": "a new"));
	      shouldbe = (which == OLD? sizeof(struct outmp): sizeof(struct utmp));
	      if (off < shouldbe)
		(void)read(fd2, &buf[off], shouldbe - off);
	      else
		(void)lseek(fd2, shouldbe - off, SEEK_CUR);
	      if (which == OLD)
		goto write_old;
	      else
		goto write_new;
	    }
	}
      off = 0;
      /*
       * Since the wtmp file is in chronologically acsending order, we
       * can move the `early' time as we go.  Allow for one hour
       * time-of-day adjustments.
       */
      early = *t - 3600;
      memset(buf, 0, sizeof buf);
    }
  close(fd2);

  printf("File \"%s\": %d old and %d new records found.\n",
	 name, old, new);

  if (nflag)
    return;

  (void) close(fd1);
  (void) snprintf(yname, sizeof yname, "%s.bak", name);

  if (rename(name, yname) == -1)
    err(EX_OSERR, "Cannot rename \"%s\" to \"%s\"", name, yname);
  
  if (rename(xname, name) == -1)
    err(EX_OSERR, "Cannot rename \"%s\" to \"%s\"", xname, name);

  if (utimes(name, tv) == -1)
    warn("Cannot adjust access and modification times for \"%s\"", name);
}

