/* tstuu.c
   Test the uucp package on a UNIX system.

   Copyright (C) 1991, 1992, 1993, 1994, 1995 Ian Lance Taylor

   This file is part of the Taylor UUCP package.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

   The author of the program may be contacted at ian@airs.com or
   c/o Cygnus Support, 48 Grove Street, Somerville, MA 02144.
   */

#include "uucp.h"

#if USE_RCS_ID
const char tstuu_rcsid[] = "$FreeBSD$";
#endif

#include "sysdep.h"
#include "system.h"
#include "getopt.h"

#include <stdio.h>
#include <ctype.h>
#include <errno.h>

#if HAVE_SYS_TIMES_H
#include <sys/times.h>
#endif

#if HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#if HAVE_SELECT
#if HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#endif

#if HAVE_POLL
#if HAVE_STROPTS_H
#include <stropts.h>
#endif
#if HAVE_POLL_H
#include <poll.h>
#endif
#endif

#if HAVE_FCNTL_H
#include <fcntl.h>
#else
#if HAVE_SYS_FILE_H
#include <sys/file.h>
#endif
#endif

#ifndef O_RDONLY
#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR 2
#endif

#if HAVE_TIME_H
#if ! HAVE_SYS_TIME_H || ! HAVE_SELECT || TIME_WITH_SYS_TIME
#include <time.h>
#endif
#endif

#if HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#if HAVE_UNION_WAIT
typedef union wait wait_status;
#else
typedef int wait_status;
#endif

#if HAVE_STREAMS_PTYS
#include <termio.h>
extern char *ptsname ();
#endif

/* Get definitions for both O_NONBLOCK and O_NDELAY.  */

#ifndef O_NDELAY
#ifdef FNDELAY
#define O_NDELAY FNDELAY
#else /* ! defined (FNDELAY) */
#define O_NDELAY 0
#endif /* ! defined (FNDELAY) */
#endif /* ! defined (O_NDELAY) */

#ifndef O_NONBLOCK
#ifdef FNBLOCK
#define O_NONBLOCK FNBLOCK
#else /* ! defined (FNBLOCK) */
#define O_NONBLOCK 0
#endif /* ! defined (FNBLOCK) */
#endif /* ! defined (O_NONBLOCK) */

#if O_NDELAY == 0 && O_NONBLOCK == 0
 #error No way to do nonblocking I/O
#endif

/* Get definitions for EAGAIN, EWOULDBLOCK and ENODATA.  */
#ifndef EAGAIN
#ifndef EWOULDBLOCK
#define EAGAIN (-1)
#define EWOULDBLOCK (-1)
#else /* defined (EWOULDBLOCK) */
#define EAGAIN EWOULDBLOCK
#endif /* defined (EWOULDBLOCK) */
#else /* defined (EAGAIN) */
#ifndef EWOULDBLOCK
#define EWOULDBLOCK EAGAIN
#endif /* ! defined (EWOULDBLOCK) */
#endif /* defined (EAGAIN) */

#ifndef ENODATA
#define ENODATA EAGAIN
#endif

/* Make sure we have a CLK_TCK definition, even if it makes no sense.
   This is in case TIMES_TICK is defined as CLK_TCK.  */
#ifndef CLK_TCK
#define CLK_TCK (60)
#endif

/* Don't try too hard to get a TIMES_TICK value; it doesn't matter
   that much.  */
#if TIMES_TICK == 0
#undef TIMES_TICK
#define TIMES_TICK CLK_TCK
#endif

#if TIMES_DECLARATION_OK
extern long times ();
#endif

#ifndef SIGCHLD
#define SIGCHLD SIGCLD
#endif

#if 1
#define ZUUCICO_CMD "login uucp"
#define UUCICO_EXECL "/bin/login", "login", "uucp"
#else
#define ZUUCICO_CMD "su - nuucp"
#define UUCICO_EXECL "/bin/su", "su", "-", "nuucp"
#endif

#if ! HAVE_SELECT && ! HAVE_POLL
 #error You need select or poll
#endif

#if ! HAVE_REMOVE
#undef remove
#define remove unlink
#endif

/* Buffer chain to hold data read from a uucico.  */

#define BUFCHARS (512)

struct sbuf
{
  struct sbuf *qnext;
  int cstart;
  int cend;
  char ab[BUFCHARS];
};
  
/* Local functions.  */

static void umake_file P((const char *zfile, int cextra));
static void uprepare_test P((boolean fmake, int itest,
			     boolean fcall_uucico,
			     const char *zsys));
static void ucheck_file P((const char *zfile, const char *zerr,
			   int cextra));
static void ucheck_test P((int itest, boolean fcall_uucico));
static RETSIGTYPE uchild P((int isig));
static int cpshow P((char *z, int bchar));
static void uchoose P((int *po1, int *po2));
static long cread P((int o, struct sbuf **));
static boolean fsend P((int o, int oslave, struct sbuf **));
static boolean fwritable P((int o));
static void xsystem P((const char *zcmd));
static FILE *xfopen P((const char *zname, const char *zmode));

static char *zDebug;
static int iTest;
static boolean fCall_uucico;
static int iPercent;
static pid_t iPid1, iPid2;
static int cFrom1, cFrom2;
static char abLogout1[sizeof "tstout /dev/ptyp0"];
static char abLogout2[sizeof "tstout /dev/ptyp0"];
static char *zProtocols;

int
main (argc, argv)
     int argc;
     char **argv;
{
  int iopt;
  const char *zcmd1, *zcmd2;
  const char *zsys;
  boolean fmake = TRUE;
  int omaster1, oslave1, omaster2, oslave2;
  char abpty1[sizeof "/dev/ptyp0"];
  char abpty2[sizeof "/dev/ptyp0"];
  struct sbuf *qbuf1, *qbuf2;

#if ! HAVE_TAYLOR_CONFIG
  fprintf (stderr, "%s: only works when compiled with HAVE_TAYLOR_CONFIG\n",
	   argv[0]);
  exit (1);
#endif

  zcmd1 = NULL;
  zcmd2 = NULL;
  zsys = "test2";

  while ((iopt = getopt (argc, argv, "c:np:s:t:ux:1:2:")) != EOF)
    {
      switch (iopt)
	{
	case 'c':
	  zProtocols = optarg;
	  break;
	case 'n':
	  fmake = FALSE;
	  break;
	case 'p':
	  iPercent = (int) strtol (optarg, (char **) NULL, 10);
	  srand ((unsigned int) ixsysdep_time ((long *) NULL));
	  break;
	case 's':
	  zsys = optarg;
	  break;
	case 't':
	  iTest = (int) strtol (optarg, (char **) NULL, 10);
	  break;
	case 'u':
	  fCall_uucico = TRUE;
	  break;
	case 'x':
	  zDebug = optarg;
	  break;
	case '1':
	  zcmd1 = optarg;
	  break;
	case '2':
	  zcmd2 = optarg;
	  break;
	default:
	  fprintf (stderr,
		   "Taylor UUCP %s, copyright (C) 1991, 92, 93, 94, 1995 Ian Lance Taylor\n",
		   VERSION);
	  fprintf (stderr,
		   "Usage: tstuu [-xn] [-t #] [-u] [-1 cmd] [-2 cmd]\n");
	  exit (EXIT_FAILURE);
	}
    }

  if (fCall_uucico && zcmd2 == NULL)
    zcmd2 = ZUUCICO_CMD;

  uprepare_test (fmake, iTest, fCall_uucico, zsys);

  (void) remove ("/usr/tmp/tstuu/spool1/core");
  (void) remove ("/usr/tmp/tstuu/spool2/core");

  omaster1 = -1;
  oslave1 = -1;
  omaster2 = -1;
  oslave2 = -1;

#if ! HAVE_STREAMS_PTYS

  {
    char *zptyname;
    const char *zpty;

    zptyname = abpty1;

    for (zpty = "pqrs"; *zpty != '\0'; ++zpty)
      {
	int ipty;

	for (ipty = 0; ipty < 16; ipty++)
	  {
	    int om, os;
	    FILE *e;
  
	    sprintf (zptyname, "/dev/pty%c%c", *zpty,
		     "0123456789abcdef"[ipty]);
	    om = open (zptyname, O_RDWR);
	    if (om < 0)
	      continue;
	    zptyname[5] = 't';
	    os = open (zptyname, O_RDWR);
	    if (os < 0)
	      {
		(void) close (om);
		continue;
	      }

	    if (omaster1 == -1)
	      {
		omaster1 = om;
		oslave1 = os;

		e = fopen ("/usr/tmp/tstuu/pty1", "w");
		if (e == NULL)
		  {
		    perror ("fopen");
		    exit (EXIT_FAILURE);
		  }
		fprintf (e, "%s", zptyname + 5);
		if (fclose (e) != 0)
		  {
		    perror ("fclose");
		    exit (EXIT_FAILURE);
		  }

		zptyname = abpty2;
	      }
	    else
	      {
		omaster2 = om;
		oslave2 = os;

		e = fopen ("/usr/tmp/tstuu/pty2", "w");
		if (e == NULL)
		  {
		    perror ("fopen");
		    exit (EXIT_FAILURE);
		  }
		fprintf (e, "%s", zptyname + 5);
		if (fclose (e) != 0)
		  {
		    perror ("fclose");
		    exit (EXIT_FAILURE);
		  }
		break;
	      }
	  }

	if (omaster1 != -1 && omaster2 != -1)
	  break;
      }
  }

#else /* HAVE_STREAMS_PTYS */

  {
    int ipty;

    for (ipty = 0; ipty < 2; ipty++)
      {
	int om, os;
	FILE *e;
	char *znam;
	struct termio stio;

	om = open ((char *) "/dev/ptmx", O_RDWR);
	if (om < 0)
	  break;
	znam = ptsname (om);
	if (znam == NULL)
	  break;
	if (unlockpt (om) != 0
	    || grantpt (om) != 0)
	  break;

	os = open (znam, O_RDWR);
	if (os < 0)
	  {
	    (void) close (om);
	    om = -1;
	    break;
	  }

	if (ioctl (os, I_PUSH, "ptem") < 0
	    || ioctl(os, I_PUSH, "ldterm") < 0)
	  {
	    perror ("ioctl");
	    exit (EXIT_FAILURE);
	  }

	/* Can this really be right? */
	memset (&stio, 0, sizeof (stio));
	stio.c_cflag = B9600 | CS8 | CREAD | HUPCL;

	if (ioctl(os, TCSETA, &stio) < 0)
	  {
	    perror ("TCSETA");
	    exit (EXIT_FAILURE);
	  }

	if (omaster1 == -1)
	  {
	    strcpy (abpty1, znam);
	    omaster1 = om;
	    oslave1 = os;
	    e = fopen ("/usr/tmp/tstuu/pty1", "w");
	    if (e == NULL)
	      {
		perror ("fopen");
		exit (EXIT_FAILURE);
	      }
	    fprintf (e, "%s", znam + 5);
	    if (fclose (e) != 0)
	      {
		perror ("fclose");
		exit (EXIT_FAILURE);
	      }
	  }
	else
	  {
	    strcpy (abpty2, znam);
	    omaster2 = om;
	    oslave2 = os;
	    e = fopen ("/usr/tmp/tstuu/pty2", "w");
	    if (e == NULL)
	      {
		perror ("fopen");
		exit (EXIT_FAILURE);
	      }
	    fprintf (e, "%s", znam + 5);
	    if (fclose (e) != 0)
	      {
		perror ("fclose");
		exit (EXIT_FAILURE);
	      }
	  }
      }
  }

#endif /* HAVE_STREAMS_PTYS */

  if (omaster2 == -1)
    {
      fprintf (stderr, "No pseudo-terminals available\n");
      exit (EXIT_FAILURE);
    }

  /* Make sure we can or these into an int for the select call.  Most
     systems could use 31 instead of 15, but it should never be a
     problem.  */
  if (omaster1 > 15 || omaster2 > 15)
    {
      fprintf (stderr, "File descriptors are too large\n");
      exit (EXIT_FAILURE);
    }

  /* Prepare to log out the command if it is a login command.  On
     Ultrix 4.0 uucico can only be run from login for some reason.  */

  if (zcmd1 == NULL
      || strncmp (zcmd1, "login", sizeof "login" - 1) != 0)
    abLogout1[0] = '\0';
  else
    sprintf (abLogout1, "tstout %s", abpty1);

  if (zcmd2 == NULL
      || strncmp (zcmd2, "login", sizeof "login" - 1) != 0)
    abLogout2[0] = '\0';
  else
    sprintf (abLogout2, "tstout %s", abpty2);

  iPid1 = fork ();
  if (iPid1 < 0)
    {
      perror ("fork");
      exit (EXIT_FAILURE);
    }
  else if (iPid1 == 0)
    {
      if (close (0) < 0
	  || close (1) < 0
	  || close (omaster1) < 0
	  || close (omaster2) < 0
	  || close (oslave2) < 0)
	perror ("close");

      if (dup2 (oslave1, 0) < 0
	  || dup2 (oslave1, 1) < 0)
	perror ("dup2");

      if (close (oslave1) < 0)
	perror ("close");

      /* This is said to improve the tests on Linux.  */
      sleep (3);

      if (zDebug != NULL)
	fprintf (stderr, "About to exec first process\n");

      if (zcmd1 != NULL)
	exit (system ((char *) zcmd1));
      else
	{
	  (void) execl ("uucico", "uucico", "-I", "/usr/tmp/tstuu/Config1",
			"-q", "-S", zsys, "-pstdin", (const char *) NULL);
	  perror ("execl failed");
	  exit (EXIT_FAILURE);
	}
    }

  iPid2 = fork ();
  if (iPid2 < 0)
    {
      perror ("fork");
      kill (iPid1, SIGTERM);
      exit (EXIT_FAILURE);
    }
  else if (iPid2 == 0)
    {
      if (close (0) < 0
	  || close (1) < 0
	  || close (omaster1) < 0
	  || close (oslave1) < 0
	  || close (omaster2) < 0)
	perror ("close");

      if (dup2 (oslave2, 0) < 0
	  || dup2 (oslave2, 1) < 0)
	perror ("dup2");

      if (close (oslave2) < 0)
	perror ("close");

      /* This is said to improve the tests on Linux.  */
      sleep (5);

      if (zDebug != NULL)
	fprintf (stderr, "About to exec second process\n");

      if (fCall_uucico)
	{
	  (void) execl (UUCICO_EXECL, (const char *) NULL);
	  perror ("execl failed");
	  exit (EXIT_FAILURE);
	}
      else if (zcmd2 != NULL)
	exit (system ((char *) zcmd2));
      else
	{
	  (void) execl ("uucico", "uucico", "-I", "/usr/tmp/tstuu/Config2",
			"-lq", (const char *)NULL);
	  perror ("execl failed");
	  exit (EXIT_FAILURE);
	}
    }

  signal (SIGCHLD, uchild);

  if (fcntl (omaster1, F_SETFL, O_NDELAY | O_NONBLOCK) < 0
      && errno == EINVAL)
    (void) fcntl (omaster1, F_SETFL, O_NONBLOCK);
  if (fcntl (omaster2, F_SETFL, O_NDELAY | O_NONBLOCK) < 0
      && errno == EINVAL)
    (void) fcntl (omaster2, F_SETFL, O_NONBLOCK);

  qbuf1 = NULL;
  qbuf2 = NULL;

  while (TRUE)
    {
      int o1, o2;
      boolean fcont;

      o1 = omaster1;
      o2 = omaster2;
      uchoose (&o1, &o2);

      if (o1 == -1 && o2 == -1)
	{
	  if (zDebug != NULL)
	    fprintf (stderr, "Five second pause\n");
	  continue;
	}

      if (o1 != -1)
	cFrom1 += cread (omaster1, &qbuf1);

      if (o2 != -1)
	cFrom2 += cread (omaster2, &qbuf2);

      do
	{
	  fcont = FALSE;

	  if (qbuf1 != NULL
	      && fwritable (omaster2)
	      && fsend (omaster2, oslave2, &qbuf1))
	    fcont = TRUE;

	  if (qbuf2 != NULL
	      && fwritable (omaster1)
	      && fsend (omaster1, oslave1, &qbuf2))
	    fcont = TRUE;

	  if (! fcont
	      && (qbuf1 != NULL || qbuf2 != NULL))
	    {
	      long cgot1, cgot2;

	      cgot1 = cread (omaster1, &qbuf1);
	      cFrom1 += cgot1;
	      cgot2 = cread (omaster2, &qbuf2);
	      cFrom2 += cgot2;
	      fcont = TRUE;
	    }
	}
      while (fcont);
    }

  /*NOTREACHED*/
}

/* When a child dies, kill them both.  */

static RETSIGTYPE
uchild (isig)
     int isig;
{
  struct tms sbase, s1, s2;

  signal (SIGCHLD, SIG_DFL);

  /* Give the processes a chance to die on their own.  */
  sleep (2);

  (void) kill (iPid1, SIGTERM);
  (void) kill (iPid2, SIGTERM);

  (void) times (&sbase);

#if HAVE_WAITPID
  (void) waitpid (iPid1, (pointer) NULL, 0);
#else /* ! HAVE_WAITPID */
#if HAVE_WAIT4
  (void) wait4 (iPid1, (pointer) NULL, 0, (struct rusage *) NULL);
#else /* ! HAVE_WAIT4 */
  (void) wait ((wait_status *) NULL);
#endif /* ! HAVE_WAIT4 */
#endif /* ! HAVE_WAITPID */

  (void) times (&s1);

#if HAVE_WAITPID
  (void) waitpid (iPid2, (pointer) NULL, 0);
#else /* ! HAVE_WAITPID */
#if HAVE_WAIT4
  (void) wait4 (iPid2, (wait_status *) NULL, 0, (struct rusage *) NULL);
#else /* ! HAVE_WAIT4 */
  (void) wait ((wait_status *) NULL);
#endif /* ! HAVE_WAIT4 */
#endif /* ! HAVE_WAITPID */

  (void) times (&s2);

  fprintf (stderr,
	   " First child: user: %g; system: %g\n",
	   (double) (s1.tms_cutime - sbase.tms_cutime) / (double) TIMES_TICK,
	   (double) (s1.tms_cstime - sbase.tms_cstime) / (double) TIMES_TICK);
  fprintf (stderr,
	   "Second child: user: %g; system: %g\n",
	   (double) (s2.tms_cutime - s1.tms_cutime) / (double) TIMES_TICK,
	   (double) (s2.tms_cstime - s1.tms_cstime) / (double) TIMES_TICK);

  ucheck_test (iTest, fCall_uucico);

  if (abLogout1[0] != '\0')
    {
      if (zDebug != NULL)
	fprintf (stderr, "Executing %s\n", abLogout1);
      (void) system (abLogout1);
    }
  if (abLogout2[0] != '\0')
    {
      if (zDebug != NULL)
	fprintf (stderr, "Executing %s\n", abLogout2);
      (void) system (abLogout2);
    }

  fprintf (stderr, "Wrote %d bytes from 1 to 2\n", cFrom1);
  fprintf (stderr, "Wrote %d bytes from 2 to 1\n", cFrom2);

  if (access ("/usr/tmp/tstuu/spool1/core", R_OK) == 0)
    fprintf (stderr, "core file 1 exists\n");
  if (access ("/usr/tmp/tstuu/spool2/core", R_OK) == 0)
    fprintf (stderr, "core file 2 exists\n");

  exit (EXIT_SUCCESS);
}

/* Open a file without error.  */

static FILE *
xfopen (zname, zmode)
     const char *zname;
     const char *zmode;
{
  FILE *eret;

  eret = fopen (zname, zmode);
  if (eret == NULL)
    {
      perror (zname);
      exit (EXIT_FAILURE);
    }
  return eret;
}

/* Close a file without error.  */

static void xfclose P((FILE *e));

static void
xfclose (e)
     FILE *e;
{
  if (fclose (e) != 0)
    {
      perror ("fclose");
      exit (EXIT_FAILURE);
    }
}

/* Create a test file.  */

static void
umake_file (z, c)
     const char *z;
     int c;
{
  int i;
  FILE *e;

  e = xfopen (z, "w");
	
  for (i = 0; i < 256; i++)
    {
      int i2;

      for (i2 = 0; i2 < 256; i2++)
	putc (i, e);
    }

  for (i = 0; i < c; i++)
    putc (i, e);

  xfclose (e);
}

/* Check a test file.  */

static void
ucheck_file (z, zerr, c)
     const char *z;
     const char *zerr;
     int c;
{
  int i;
  FILE *e;

  e = xfopen (z, "r");

  for (i = 0; i < 256; i++)
    {
      int i2;

      for (i2 = 0; i2 < 256; i2++)
	{
	  int bread;

	  bread = getc (e);
	  if (bread == EOF)
	    {
	      fprintf (stderr,
		       "%s: Unexpected EOF at position %d,%d\n",
		       zerr, i, i2);
	      xfclose (e);
	      return;
	    }
	  if (bread != i)
	    fprintf (stderr,
		     "%s: At position %d,%d got %d expected %d\n",
		     zerr, i, i2, bread, i);
	}
    }

  for (i = 0; i < c; i++)
    {
      int bread;

      bread = getc (e);
      if (bread == EOF)
	{
	  fprintf (stderr, "%s: Unexpected EOF at extra %d\n", zerr, i);
	  xfclose (e);
	  return;
	}
      if (bread != i)
	fprintf (stderr, "%s: At extra %d got %d expected %d\n",
		 zerr, i, bread, i);
    }

  if (getc (e) != EOF)
    fprintf (stderr, "%s: File is too long", zerr);

  xfclose (e);
}

/* Prepare all the configuration files for testing.  */

static void
uprepare_test (fmake, itest, fcall_uucico, zsys)
     boolean fmake;
     int itest;
     boolean fcall_uucico;
     const char *zsys;
{
  FILE *e;
  const char *zuucp1, *zuucp2;
  const char *zuux1, *zuux2;
  char ab[1000];
  const char *zfrom;
  const char *zto;

/* We must make /usr/tmp/tstuu world writeable or we won't be able to
   receive files into it.  */
  (void) umask (0);

#ifndef S_IWOTH
#define S_IWOTH 02
#endif

  if (mkdir ((char *) "/usr/tmp/tstuu",
	     IPUBLIC_DIRECTORY_MODE | S_IWOTH) != 0
      && errno != EEXIST)
    {
      perror ("mkdir");
      exit (EXIT_FAILURE);
    }

  if (mkdir ((char *) "/usr/tmp/tstuu/spool1", IPUBLIC_DIRECTORY_MODE) != 0
      && errno != EEXIST)
    {
      perror ("mkdir");
      exit (EXIT_FAILURE);
    }

  if (mkdir ((char *) "/usr/tmp/tstuu/spool2", IPUBLIC_DIRECTORY_MODE) != 0
      && errno != EEXIST)
    {
      perror ("mkdir");
      exit (EXIT_FAILURE);
    }

  if (fmake)
    {
      e = xfopen ("/usr/tmp/tstuu/Config1", "w");

      fprintf (e, "# First test configuration file\n");
      fprintf (e, "nodename test1\n");
      fprintf (e, "spool /usr/tmp/tstuu/spool1\n");
      fprintf (e, "lockdir /usr/tmp/tstuu/spool1\n");
      fprintf (e, "sysfile /usr/tmp/tstuu/System1\n");
      fprintf (e, "sysfile /usr/tmp/tstuu/System1.2\n");
      fprintf (e, "portfile /usr/tmp/tstuu/Port1\n");
      (void) remove ("/usr/tmp/tstuu/Log1");
#if ! HAVE_HDB_LOGGING
      fprintf (e, "logfile /usr/tmp/tstuu/Log1\n");
#else
      fprintf (e, "%s\n", "logfile /usr/tmp/tstuu/Log1/%s/%s");
#endif
      fprintf (e, "statfile /usr/tmp/tstuu/Stats1\n");
      fprintf (e, "debugfile /usr/tmp/tstuu/Debug1\n");
      fprintf (e, "callfile /usr/tmp/tstuu/Call1\n");
      fprintf (e, "pubdir /usr/tmp/tstuu\n");
#if HAVE_V2_CONFIG
      fprintf (e, "v2-files no\n");
#endif
#if HAVE_HDB_CONFIG
      fprintf (e, "hdb-files no\n");
#endif
      if (zDebug != NULL)
	fprintf (e, "debug %s\n", zDebug);

      xfclose (e);

      e = xfopen ("/usr/tmp/tstuu/System1", "w");

      fprintf (e, "# This file is ignored, to test multiple system files\n");
      fprintf (e, "time never\n");

      xfclose (e);

      e = xfopen ("/usr/tmp/tstuu/System1.2", "w");

      fprintf (e, "# First test system file\n");
      fprintf (e, "time any\n");
      fprintf (e, "port stdin\n");
      fprintf (e, "# That was the defaults\n");
      fprintf (e, "system %s\n", zsys);
      if (! fcall_uucico)
	{
	  FILE *eprog;

	  eprog = xfopen ("/usr/tmp/tstuu/Chat1", "w");

	  /* Wait for the other side to open the port and flush input.  */
	  fprintf (eprog, "sleep 2\n");
	  fprintf (eprog,
		   "echo password $1 speed $2 1>&2\n");
	  fprintf (eprog, "echo test1\n");
	  fprintf (eprog, "exit 0\n");

	  xfclose (eprog);

	  if (chmod ("/usr/tmp/tstuu/Chat1",
		     S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) < 0)
	    {
	      perror ("chmod (/usr/tmp/tstuu/Chat1)");
	      exit (EXIT_FAILURE);
	    }

	  fprintf (e, "chat-program /usr/tmp/tstuu/Chat1 \\P \\S\n");

	  fprintf (e, "chat word: \\P\n");
	  fprintf (e, "chat-fail login;\n");
	  fprintf (e, "call-login *\n");
	  fprintf (e, "call-password *\n");
	}
      else
	fprintf (e, "chat \"\"\n");
      fprintf (e, "call-transfer yes\n");
      fprintf (e, "commands cat\n");
      if (! fcall_uucico && iPercent == 0)
	{
	  fprintf (e, "protocol-parameter g window 7\n");
	  fprintf (e, "protocol-parameter g packet-size 4096\n");
	  fprintf (e, "protocol-parameter j avoid \\377\n");
	}
      if (zProtocols != NULL)
	fprintf (e, "protocol %s\n", zProtocols);

      xfclose (e);

      e = xfopen ("/usr/tmp/tstuu/Port1", "w");

      fprintf (e, "port stdin\n");
      fprintf (e, "type stdin\n");

      xfclose (e);

      e = xfopen ("/usr/tmp/tstuu/Call1", "w");

      fprintf (e, "Call out password file\n");
      fprintf (e, "%s test1 pass\\s1\n", zsys);

      xfclose (e);

      if (! fcall_uucico)
	{
	  FILE *eprog;

	  e = xfopen ("/usr/tmp/tstuu/Config2", "w");

	  fprintf (e, "# Second test configuration file\n");
	  fprintf (e, "nodename test2\n");
	  fprintf (e, "spool /usr/tmp/tstuu/spool2\n");
	  fprintf (e, "lockdir /usr/tmp/tstuu/spool2\n");
	  fprintf (e, "sysfile /usr/tmp/tstuu/System2\n");
	  (void) remove ("/usr/tmp/tstuu/Log2");
#if ! HAVE_HDB_LOGGING
	  fprintf (e, "logfile /usr/tmp/tstuu/Log2\n");
#else
	  fprintf (e, "%s\n", "logfile /usr/tmp/tstuu/Log2/%s/%s");
#endif
	  fprintf (e, "statfile /usr/tmp/tstuu/Stats2\n");
	  fprintf (e, "debugfile /usr/tmp/tstuu/Debug2\n");
	  fprintf (e, "passwdfile /usr/tmp/tstuu/Pass2\n");
	  fprintf (e, "pubdir /usr/tmp/tstuu\n");
#if HAVE_V2_CONFIG
	  fprintf (e, "v2-files no\n");
#endif
#if HAVE_HDB_CONFIG
	  fprintf (e, "hdb-files no\n");
#endif
	  if (zDebug != NULL)
	    fprintf (e, "debug %s\n", zDebug);

	  xfclose (e);

	  e = xfopen ("/usr/tmp/tstuu/System2", "w");

	  fprintf (e, "# Second test system file\n");
	  fprintf (e, "system test1\n");
	  fprintf (e, "called-login test1\n");
	  fprintf (e, "request true\n");
	  fprintf (e, "commands cat\n");
	  if (zProtocols != NULL)
	    fprintf (e, "protocol %s\n", zProtocols);

	  eprog = xfopen ("/usr/tmp/tstuu/Chat2", "w");

	  fprintf (eprog,
		   "echo port $1 1>&2\n");
	  fprintf (eprog, "exit 0\n");

	  xfclose (eprog);

	  if (chmod ("/usr/tmp/tstuu/Chat2",
		     S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) < 0)
	    {
	      perror ("chmod (/usr/tmp/tstuu/Chat2");
	      exit (EXIT_FAILURE);
	    }

	  fprintf (e, "called-chat-program /bin/sh /usr/tmp/tstuu/Chat2 \\Y\n");
	  fprintf (e, "time any\n");

	  xfclose (e);

	  e = xfopen ("/usr/tmp/tstuu/Pass2", "w");

	  fprintf (e, "# Call in password file\n");
	  fprintf (e, "test1 pass\\s1\n");

	  xfclose (e);
	}
    }

  zuucp1 = "./uucp -I /usr/tmp/tstuu/Config1 -r";
  zuux1 = "./uux -I /usr/tmp/tstuu/Config1 -r";

  if (fcall_uucico)
    {
      zuucp2 = "/usr/bin/uucp -r";
      zuux2 = "/usr/bin/uux -r";
    }
  else
    {
      zuucp2 = "./uucp -I /usr/tmp/tstuu/Config2 -r";
      zuux2 = "./uux -I /usr/tmp/tstuu/Config2 -r";
    }

  /* Test transferring a file from the first system to the second.  */
  if (itest == 0 || itest == 1)
    {
      zfrom = "/usr/tmp/tstuu/from1";
      if (fcall_uucico)
	zto = "/usr/spool/uucppublic/to1";
      else
	zto = "/usr/tmp/tstuu/to1";

      (void) remove (zto);
      umake_file (zfrom, 0);

      sprintf (ab, "%s %s %s!%s", zuucp1, zfrom, zsys, zto);
      xsystem (ab);
    }

  /* Test having the first system request a file from the second.  */
  if (itest == 0 || itest == 2)
    {
      if (fcall_uucico)
	zfrom = "/usr/spool/uucppublic/from2";
      else
	zfrom = "/usr/tmp/tstuu/from2";
      zto = "/usr/tmp/tstuu/to2";

      (void) remove (zto);
      umake_file (zfrom, 3);

      sprintf (ab, "%s %s!%s %s", zuucp1, zsys, zfrom, zto);
      xsystem (ab);
    }

  /* Test having the second system send a file to the first.  */
  if (itest == 0 || itest == 3)
    {
      if (fcall_uucico)
	zfrom = "/usr/spool/uucppublic/from3";
      else
	zfrom = "/usr/tmp/tstuu/from3";
      zto = "/usr/tmp/tstuu/to3";

      (void) remove (zto);
      umake_file (zfrom, 5);

      sprintf (ab, "%s -c \\~/from3 test1!~/to3", zuucp2);
      xsystem (ab);
    }

  /* Test having the second system request a file from the first.  */
  if (itest == 0 || itest == 4)
    {
      zfrom = "/usr/tmp/tstuu/from4";
      if (fcall_uucico)
	zto = "/usr/spool/uucppublic/to4";
      else
	zto = "/usr/tmp/tstuu/to4";

      (void) remove (zto);
      umake_file (zfrom, 7);

      sprintf (ab, "%s test1!%s %s", zuucp2, zfrom, zto);
      xsystem (ab);
    }

  /* Test having the second system make an execution request.  */
  if (itest == 0 || itest == 5)
    {
      zfrom = "/usr/tmp/tstuu/from5";
      if (fcall_uucico)
	zto = "/usr/spool/uucppublic/to5";
      else
	zto = "/usr/tmp/tstuu/to5";

      (void) remove (zto);
      umake_file (zfrom, 11);

      sprintf (ab, "%s test1!cat '<%s' '>%s'", zuux2, zfrom, zto);
      xsystem (ab);
    }

  /* Test having the first system request a wildcard.  */
  if (itest == 0 || itest == 6)
    {
      const char *zfrom1, *zfrom2;

      if (fcall_uucico)
	{
	  zfrom = "/usr/spool/uucppublic/to6\\*";
	  zfrom1 = "/usr/spool/uucppublic/to6.1";
	  zfrom2 = "/usr/spool/uucppublic/to6.2";
	}
      else
	{
	  zfrom = "/usr/tmp/tstuu/spool2/to6\\*";
	  zfrom1 = "/usr/tmp/tstuu/spool2/to6.1";
	  zfrom2 = "/usr/tmp/tstuu/spool2/to6.2";
	}

      umake_file (zfrom1, 100);
      umake_file (zfrom2, 101);
      (void) remove ("/usr/tmp/tstuu/to6.1");
      (void) remove ("/usr/tmp/tstuu/to6.2");

      sprintf (ab, "%s %s!%s /usr/tmp/tstuu", zuucp1, zsys, zfrom);
      xsystem (ab);
    }

  /* Test having the second system request a wildcard.  */
  if (itest == 0 || itest == 7)
    {
      const char *zto1, *zto2;

      if (fcall_uucico)
	{
	  zto = "/usr/spool/uucppublic";
	  zto1 = "/usr/spool/uucppublic/to7.1";
	  zto2 = "/usr/spool/uucppublic/to7.2";
	}
      else
	{
	  zto = "/usr/tmp/tstuu";
	  zto1 = "/usr/tmp/tstuu/to7.1";
	  zto2 = "/usr/tmp/tstuu/to7.2";
	}

      umake_file ("/usr/tmp/tstuu/spool1/to7.1", 150);
      umake_file ("/usr/tmp/tstuu/spool1/to7.2", 155);
      (void) remove (zto1);
      (void) remove (zto2);

      sprintf (ab, "%s test1!/usr/tmp/tstuu/spool1/to7.\\* %s", zuucp2,
	       zto);
      xsystem (ab);
    }

  /* Test an E command.  This runs cat, discarding the output.  */
  if ((itest == 0 || itest == 8) && ! fcall_uucico)
    {
      umake_file ("/usr/tmp/tstuu/from8", 30);
      sprintf (ab, "%s - test2!cat < /usr/tmp/tstuu/from8", zuux1);
      xsystem (ab);
    }
}

/* Try to make sure the file transfers were successful.  */

static void
ucheck_test (itest, fcall_uucico)
     int itest;
     boolean fcall_uucico;
{
  if (itest == 0 || itest == 1)
    {
      if (fcall_uucico)
	ucheck_file ("/usr/spool/uucppublic/to1", "test 1", 0);
      else
	ucheck_file ("/usr/tmp/tstuu/to1", "test 1", 0);
    }

  if (itest == 0 || itest == 2)
    ucheck_file ("/usr/tmp/tstuu/to2", "test 2", 3);

  if (itest == 0 || itest == 3)
    ucheck_file ("/usr/tmp/tstuu/to3", "test 3", 5);

  if (itest == 0 || itest == 4)
    {
      if (fcall_uucico)
	ucheck_file ("/usr/spool/uucppublic/to4", "test 4", 7);
      else
	ucheck_file ("/usr/tmp/tstuu/to4", "test 4", 7);
    }

  if (itest == 0 || itest == 6)
    {
      ucheck_file ("/usr/tmp/tstuu/to6.1", "test 6.1", 100);
      ucheck_file ("/usr/tmp/tstuu/to6.2", "test 6.2", 101);
    }

  if (itest == 0 || itest == 7)
    {
      const char *zto1, *zto2;

      if (fcall_uucico)
	{
	  zto1 = "/usr/spool/uucppublic/to7.1";
	  zto2 = "/usr/spool/uucppublic/to7.2";
	}
      else
	{
	  zto1 = "/usr/tmp/tstuu/to7.1";
	  zto2 = "/usr/tmp/tstuu/to7.2";
	}

      ucheck_file (zto1, "test 7.1", 150);
      ucheck_file (zto2, "test 7.2", 155);
    }
}

/* A debugging routine used when displaying buffers.  */

static int
cpshow (z, ichar)
     char *z;
     int ichar;
{
  if (isprint (BUCHAR (ichar)) && ichar != '\"')
    {
      *z = (char) ichar;
      return 1;
    }

  *z++ = '\\';

  switch (ichar)
    {
    case '\n':
      *z = 'n';
      return 2;
    case '\r':
      *z = 'r';
      return 2;
    case '\"':
      *z = '\"';
      return 2;
    default:
      sprintf (z, "%03o", (unsigned int)(ichar & 0xff));
      return strlen (z) + 1;
    }
}      

/* Pick one of two file descriptors which is ready for reading, or
   return in five seconds.  If the argument is ready for reading,
   leave it alone; otherwise set it to -1.  */

static void
uchoose (po1, po2)
     int *po1;
     int *po2;
{
#if HAVE_SELECT

  int iread;
  struct timeval stime;

  iread = (1 << *po1) | (1 << *po2);
  stime.tv_sec = 5;
  stime.tv_usec = 0;

  if (select ((*po1 > *po2 ? *po1 : *po2) + 1, (pointer) &iread,
	      (pointer) NULL, (pointer) NULL, &stime) < 0)
    {
      perror ("select");
      uchild (SIGCHLD);
    }

  if ((iread & (1 << *po1)) == 0)
    *po1 = -1;

  if ((iread & (1 << *po2)) == 0)
    *po2 = -1;

#else /* ! HAVE_SELECT */

#if HAVE_POLL

  struct pollfd as[2];

  as[0].fd = *po1;
  as[0].events = POLLIN;
  as[1].fd = *po2;
  as[1].events = POLLIN;

  if (poll (as, 2, 5 * 1000) < 0)
    {
      perror ("poll");
      uchild (SIGCHLD);
    }

  if ((as[0].revents & POLLIN) == 0)
    *po1 = -1;
  
  if ((as[1].revents & POLLIN) == 0)
    *po2 = -1;

#endif /* HAVE_POLL */
#endif /* ! HAVE_SELECT */
}

/* Read some data from a file descriptor.  This keeps reading until
   one of the reads gets no data.  */

static long
cread (o, pqbuf)
     int o;
     struct sbuf **pqbuf;
{
  long ctotal;

  while (*pqbuf != NULL && (*pqbuf)->qnext != NULL)
    pqbuf = &(*pqbuf)->qnext;

  ctotal = 0;

  while (TRUE)
    {
      int cgot;

      if (*pqbuf != NULL
	  && (*pqbuf)->cend >= sizeof (*pqbuf)->ab)
	pqbuf = &(*pqbuf)->qnext;

      if (*pqbuf == NULL)
	{
	  *pqbuf = (struct sbuf *) malloc (sizeof (struct sbuf));
	  if (*pqbuf == NULL)
	    {
	      fprintf (stderr, "Out of memory\n");
	      uchild (SIGCHLD);
	    }
	  (*pqbuf)->qnext = NULL;
	  (*pqbuf)->cstart = 0;
	  (*pqbuf)->cend = 0;
	}
      
      cgot = read (o, (*pqbuf)->ab + (*pqbuf)->cend,
		   (sizeof (*pqbuf)->ab) - (*pqbuf)->cend);
      if (cgot < 0)
	{
	  if (errno == EAGAIN || errno == EWOULDBLOCK || errno == ENODATA)
	    cgot = 0;
	  else
	    {
	      perror ("read");
	      uchild (SIGCHLD);
	    }
	}

      if (cgot == 0)
	return ctotal;

      ctotal += cgot;

      if (zDebug != NULL)
	{
	  char abshow[325];
	  char *zfrom;
	  char *zshow;
	  int i;

	  zfrom = (*pqbuf)->ab + (*pqbuf)->cend;
	  zshow = abshow;
	  for (i = 0; i < cgot && i < 80; i++, zfrom++)
	    zshow += cpshow (zshow, *zfrom);
	  if (i < cgot)
	    {
	      *zshow++ = '.';
	      *zshow++ = '.';
	      *zshow++ = '.';
	    }
	  *zshow = '\0';
	  fprintf (stderr, "Read from %d: %d \"%s\"\n", o, cgot, abshow);
	  fflush (stderr);
	}

      if (iPercent > 0)
	{
	  int i;
	  int c;

	  c = 0;
	  for (i = 0; i < cgot; i++)
	    {
	      if (rand () % 1000 < iPercent)
		{
		  ++(*pqbuf)->ab[(*pqbuf)->cend + i];
		  ++c;
		}
	    }
	  if (zDebug != NULL && c > 0)
	    fprintf (stderr, "Clobbered %d bytes\n", c);
	}

      (*pqbuf)->cend += cgot;

      if (ctotal > 256)
	return ctotal;
    }
}

/* Write data to a file descriptor until one of the writes gets no
   data.  */

static boolean
fsend (o, oslave, pqbuf)
     int o;
     int oslave;
     struct sbuf **pqbuf;
{
  long ctotal;

  ctotal = 0;
  while (*pqbuf != NULL)
    {
      int cwrite, cwrote;

      if ((*pqbuf)->cstart >= (*pqbuf)->cend)
	{
	  struct sbuf *qfree;

	  qfree = *pqbuf;
	  *pqbuf = (*pqbuf)->qnext;
	  free ((pointer) qfree);
	  continue;
	}

#ifdef FIONREAD
      {
	long cunread;

	if (ioctl (oslave, FIONREAD, &cunread) < 0)
	  {
	    perror ("FIONREAD");
	    uchild (SIGCHLD);
	  }
	if (zDebug != NULL)
	  fprintf (stderr, "%ld unread\n", cunread);
	cwrite = 256 - cunread;
	if (cwrite <= 0)
	  break;
      }
#else /* ! FIONREAD */
      if (! fwritable (o))
	break;
      cwrite = 1;
#endif /* ! FIONREAD */

      if (cwrite > (*pqbuf)->cend - (*pqbuf)->cstart)
	cwrite = (*pqbuf)->cend - (*pqbuf)->cstart;

      cwrote = write (o, (*pqbuf)->ab + (*pqbuf)->cstart, cwrite);
      if (cwrote < 0)
	{
	  if (errno == EAGAIN || errno == EWOULDBLOCK || errno == ENODATA)
	    cwrote = 0;
	  else
	    {
	      perror ("write");
	      uchild (SIGCHLD);
	    }
	}
      
      if (cwrote == 0)
	break;

      ctotal += cwrote;
      (*pqbuf)->cstart += cwrote;
    }

  if (zDebug != NULL && ctotal > 0)
    fprintf (stderr, "Wrote %ld to %d\n", ctotal, o);

  return ctotal > 0;
}

/* Check whether a file descriptor can be written to.  */

static boolean
fwritable (o)
     int o;
{
#if HAVE_SELECT

  int iwrite;
  struct timeval stime;
  int cfds;

  iwrite = 1 << o;

  stime.tv_sec = 0;
  stime.tv_usec = 0;

  cfds = select (o + 1, (pointer) NULL, (pointer) &iwrite,
		 (pointer) NULL, &stime);
  if (cfds < 0)
    {
      perror ("select");
      uchild (SIGCHLD);
    }

  return cfds > 0;

#else /* ! HAVE_SELECT */

#if HAVE_POLL

  struct pollfd s;
  int cfds;

  s.fd = o;
  s.events = POLLOUT;

  cfds = poll (&s, 1, 0);
  if (cfds < 0)
    {
      perror ("poll");
      uchild (SIGCHLD);
    }

  return cfds > 0;

#endif /* HAVE_POLL */
#endif /* ! HAVE_SELECT */
}

/* A version of the system command that checks for errors.  */

static void
xsystem (zcmd)
     const char *zcmd;
{
  int istat;

  istat = system ((char *) zcmd);
  if (istat != 0)
    {
      fprintf (stderr, "Command failed with status %d\n", istat);
      fprintf (stderr, "%s\n", zcmd);
      exit (EXIT_FAILURE);
    }
}
