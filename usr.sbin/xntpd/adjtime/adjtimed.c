/*************************************************************************/
/* (c) Copyright Tai Jin, 1988.  All Rights Reserved.                    */
/*     Hewlett-Packard Laboratories.                                     */
/*                                                                       */
/* Permission is hereby granted for unlimited modification, use, and     */
/* distribution.  This software is made available with no warranty of    */
/* any kind, express or implied.  This copyright notice must remain      */
/* intact in all versions of this software.                              */
/*                                                                       */
/* The author would appreciate it if any bug fixes and enhancements were */
/* to be sent back to him for incorporation into future versions of this */
/* software.  Please send changes to tai@iag.hp.com or ken@sdd.hp.com.   */
/*************************************************************************/

#ifndef lint
static char RCSid[] = "adjtimed.c,v 3.1 1993/07/06 01:04:45 jbj Exp";
#endif

/*
 * Adjust time daemon.
 * This deamon adjusts the rate of the system clock a la BSD's adjtime().
 * The adjtime() routine uses SYSV messages to communicate with this daemon.
 *
 * Caveat: This emulation uses an undocumented kernel variable.  As such, it
 * cannot be guaranteed to work in future HP-UX releases.  Perhaps a real
 * adjtime(2) will be supported in the future.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <time.h>
#include <signal.h>
#include <nlist.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include "ntp_syslog.h"
#include "adjtime.h"

double atof();
extern int optind;
extern char *optarg;

int InitClockRate();
int AdjustClockRate();
#ifdef notdef
LONG GetClockRate();
#endif
int SetClockRate();
void ResetClockRate();
void Cleanup();
void Exit();

#define MILLION		1000000L

#define tvtod(tv)	((double)(LONG)tv.tv_sec + \
			((double)tv.tv_usec / (double)MILLION))

char *progname = NULL;
int verbose = 0;
int sysdebug = 0;
static int mqid;
static double oldrate = 0.0;
static double RATE = 0.25;
static double PERIOD = 6.666667;


main(argc, argv)
     int argc;
     char **argv;
{
  struct timeval remains;
  struct sigvec vec;
  MsgBuf msg;
  char ch;
  int nofork = 0;
  int fd;

  progname = argv[0];

  openlog("adjtimed", LOG_PID, LOG_LOCAL6);

  while ((ch = getopt(argc, argv, "hkrvdfp:")) != EOF) {
    switch (ch) {
    case 'k':
    case 'r':
      if ((mqid = msgget(KEY, 0)) != -1) {
	if (msgctl(mqid, IPC_RMID, (struct msqid_ds *)0) == -1) {
	  syslog(LOG_ERR, "remove old message queue: %m");
	  perror("adjtimed: remove old message queue");
	  exit(1);
        }
      }

      if (ch == 'k')
	exit(0);

      break;

    case 'v':
      ++verbose, nofork = 1;
      break;

    case 'd':
      ++sysdebug;
      break;

    case 'f':
      nofork = 1;
      break;

    case 'p':
      if ((RATE = atof(optarg)) <= 0.0 || RATE >= 100.0) {
	fputs("adjtimed: percentage must be between 0.0 and 100.0\n", stderr);
	exit(1);
      }

      RATE /= 100.0;
      PERIOD = 1.0 / RATE;
      break;

    default:
      puts("usage: adjtimed -hkrvdf -p rate");
      puts("-h\thelp");
      puts("-k\tkill existing adjtimed, if any");
      puts("-r\trestart (kills existing adjtimed, if any)");
      puts("-v\tdebug output (repeat for more output)");
      puts("-d\tsyslog output (repeat for more output)");
      puts("-f\tno fork");
      puts("-p rate\tpercent rate of change");
      syslog(LOG_ERR, "usage error");
      exit(1);
    } /* switch */
  } /* while */

  if (!nofork) {
    switch (fork()) {
    case 0:
      close(fileno(stdin));
      close(fileno(stdout));
      close(fileno(stderr));

#ifdef TIOCNOTTY
      if ((fd = open("/dev/tty")) != -1) {
	ioctl(fd, TIOCNOTTY, 0);
	close(fd);
      }
#else
      setpgrp();
#endif
      break;

    case -1:
      syslog(LOG_ERR, "fork: %m");
      perror("adjtimed: fork");
      exit(1);

    default:
      exit(0);
    } /* switch */
  } /* if */

  if (nofork) {
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    setvbuf(stderr, NULL, _IONBF, BUFSIZ);
  }

  syslog(LOG_INFO, "started (rate %.2f%%)", RATE * 100.0);
  if (verbose) printf("adjtimed: started (rate %.2f%%)\n", RATE * 100.0);

  if (InitClockRate() == -1)
    Exit(2);

  (void)signal(SIGHUP, SIG_IGN);
  (void)signal(SIGINT, SIG_IGN);
  (void)signal(SIGQUIT, SIG_IGN);
  (void)signal(SIGTERM, Cleanup);

  vec.sv_handler = ResetClockRate;
  vec.sv_flags = 0;
  vec.sv_mask = ~0;
  sigvector(SIGALRM, &vec, (struct sigvec *)0);

  if (msgget(KEY, IPC_CREAT|IPC_EXCL) == -1) {
    if (errno == EEXIST) {
      syslog(LOG_ERR, "message queue already exists, use -r to remove it");
      fputs("adjtimed: message queue already exists, use -r to remove it\n",
		stderr);
      Exit(1);
    }

    syslog(LOG_ERR, "create message queue: %m");
    perror("adjtimed: create message queue");
    Exit(1);
  }

  if ((mqid = msgget(KEY, 0)) == -1) {
    syslog(LOG_ERR, "get message queue id: %m");
    perror("adjtimed: get message queue id");
    Exit(1);
  }

  for (;;) {
    if (msgrcv(mqid, &msg.msgp, MSGSIZE, CLIENT, 0) == -1) {
      if (errno == EINTR) continue;
      syslog(LOG_ERR, "read message: %m");
      perror("adjtimed: read message");
      Cleanup();
    }

    switch (msg.msgb.code) {
    case DELTA1:
    case DELTA2:
      AdjustClockRate(&msg.msgb.tv, &remains);

      if (msg.msgb.code == DELTA2) {
	msg.msgb.tv = remains;
	msg.msgb.mtype = SERVER;

	while (msgsnd(mqid, &msg.msgp, MSGSIZE, 0) == -1) {
	  if (errno == EINTR) continue;
	  syslog(LOG_ERR, "send message: %m");
	  perror("adjtimed: send message");
	  Cleanup();
	}
      }

      if (remains.tv_sec + remains.tv_usec != 0L) {
	if (verbose) {
	  printf("adjtimed: previous correction remaining %.6fs\n",
			tvtod(remains));
	}
	if (sysdebug) {
	  syslog(LOG_INFO, "previous correction remaining %.6fs",
			tvtod(remains));
	}
      }
      break;

    default:
      fprintf(stderr, "adjtimed: unknown message code %d\n", msg.msgb.code);
      syslog(LOG_ERR, "unknown message code %d", msg.msgb.code);
    } /* switch */
  } /* loop */
} /* main */

/*
 * Default clock rate (old_tick).
 */
#define DEFAULT_RATE	(MILLION / HZ)
#define UNKNOWN_RATE	0L
#define SLEW_RATE	(MILLION / DEFAULT_RATE)
#define MIN_DELTA	SLEW_RATE
/*
#define RATE		0.005
#define PERIOD		(1.0 / RATE)
*/
static LONG default_rate = DEFAULT_RATE;
static LONG slew_rate = SLEW_RATE;

AdjustClockRate(delta, olddelta)
     register struct timeval *delta, *olddelta;
{
  register LONG rate, dt;
  struct itimerval period, remains;
  static LONG leftover = 0;
/*
 * rate of change
 */
  dt = (delta->tv_sec * MILLION) + delta->tv_usec + leftover;

  if (dt < MIN_DELTA && dt > -MIN_DELTA) {
    leftover += delta->tv_usec;

    if (olddelta) {
      getitimer(ITIMER_REAL, &remains);
      dt = ((remains.it_value.tv_sec * MILLION) + remains.it_value.tv_usec) *
		oldrate;
      olddelta->tv_sec = dt / MILLION;
      olddelta->tv_usec = dt - (olddelta->tv_sec * MILLION); 
    }

    if (verbose > 2) printf("adjtimed: delta is too small: %dus\n", dt);
    if (sysdebug > 2) syslog(LOG_INFO, "delta is too small: %dus", dt);
    return (1);
  }

  leftover = dt % MIN_DELTA;
  dt -= leftover;

  if (verbose)
    printf("adjtimed: new correction %.6fs\n", (double)dt / (double)MILLION);
  if (sysdebug)
    syslog(LOG_INFO, "new correction %.6fs", (double)dt / (double)MILLION);
  if (verbose > 2) printf("adjtimed: leftover %dus\n", leftover);
  if (sysdebug > 2) syslog(LOG_INFO, "leftover %dus", leftover);
  rate = dt * RATE;

  if (rate < slew_rate && rate > -slew_rate) {
    rate = (rate < 0L ? -slew_rate : slew_rate);
    dt = abs(dt * (MILLION / slew_rate));
    period.it_value.tv_sec = dt / MILLION;
  } else {
    period.it_value.tv_sec = (LONG)PERIOD;
  }
/*
 * The adjustment will always be a multiple of the minimum adjustment.
 * So the period will always be a whole second value.
 */
  period.it_value.tv_usec = 0;

  if (verbose > 1)
    printf("adjtimed: will be complete in %ds\n", period.it_value.tv_sec);
  if (sysdebug > 1)
    syslog(LOG_INFO, "will be complete in %ds", period.it_value.tv_sec);
/*
 * adjust the clock rate
 */
  if (SetClockRate((rate / slew_rate) + default_rate) == -1) {
    syslog(LOG_ERR, "set clock rate: %m");
    perror("adjtimed: set clock rate");
  }
/*
 * start the timer
 * (do this after changing the rate because the period has been rounded down)
 */
  period.it_interval.tv_sec = period.it_interval.tv_usec = 0L;
  setitimer(ITIMER_REAL, &period, &remains);
/*
 * return old delta
 */
  if (olddelta) {
    dt = ((remains.it_value.tv_sec * MILLION) + remains.it_value.tv_usec) *
		oldrate;
    olddelta->tv_sec = dt / MILLION;
    olddelta->tv_usec = dt - (olddelta->tv_sec * MILLION); 
  }

  oldrate = (double)rate / (double)MILLION;
} /* AdjustClockRate */

static struct nlist nl[] = {
#ifdef hp9000s800
#ifdef PRE7_0
  { "tick" },
#else
  { "old_tick" },
#endif
#else
  { "_old_tick" },
#endif
  { "" }
};

static int kmem;

/*
 * The return value is the clock rate in old_tick units or -1 if error.
 */
LONG
GetClockRate()
{
  LONG rate, mask;

  if (lseek(kmem, (LONG)nl[0].n_value, 0) == -1L)
    return (-1L);

  mask = sigblock(sigmask(SIGALRM));

  if (read(kmem, (caddr_t)&rate, sizeof(rate)) != sizeof(rate))
    rate = UNKNOWN_RATE;

  sigsetmask(mask);
  return (rate);
} /* GetClockRate */

/*
 * The argument is the new rate in old_tick units.
 */
SetClockRate(rate)
     LONG rate;
{
  LONG mask;

  if (lseek(kmem, (LONG)nl[0].n_value, 0) == -1L)
    return (-1);

  mask = sigblock(sigmask(SIGALRM));

  if (write(kmem, (caddr_t)&rate, sizeof(rate)) != sizeof(rate)) {
    sigsetmask(mask);
    return (-1);
  }

  sigsetmask(mask);

  if (rate != default_rate) {
    if (verbose > 3) {
      printf("adjtimed: clock rate (%lu) %ldus/s\n", rate,
		(rate - default_rate) * slew_rate);
    }
    if (sysdebug > 3) {
      syslog(LOG_INFO, "clock rate (%lu) %ldus/s", rate,
		(rate - default_rate) * slew_rate);
    }
  }

  return (0);
} /* SetClockRate */

InitClockRate()
{
  if ((kmem = open("/dev/kmem", O_RDWR)) == -1) {
    syslog(LOG_ERR, "open(/dev/kmem): %m");
    perror("adjtimed: open(/dev/kmem)");
    return (-1);
  }

  nlist("/hp-ux", nl);

  if (nl[0].n_type == 0) {
    fputs("adjtimed: /hp-ux has no symbol table\n", stderr);
    syslog(LOG_ERR, "/hp-ux has no symbol table");
    return (-1);
  }
/*
 * Set the default to the system's original value
 */
  default_rate = GetClockRate();
  if (default_rate == UNKNOWN_RATE) default_rate = DEFAULT_RATE;
  slew_rate = (MILLION / default_rate);

  return (0);
} /* InitClockRate */

/*
 * Reset the clock rate to the default value.
 */
void
ResetClockRate()
{
  struct itimerval it;

  it.it_value.tv_sec = it.it_value.tv_usec = 0L;
  setitimer(ITIMER_REAL, &it, (struct itimerval *)0);

  if (verbose > 2) puts("adjtimed: resetting the clock");
  if (sysdebug > 2) syslog(LOG_INFO, "resetting the clock");

  if (GetClockRate() != default_rate) {
    if (SetClockRate(default_rate) == -1) {
      syslog(LOG_ERR, "set clock rate: %m");
      perror("adjtimed: set clock rate");
    }
  }

  oldrate = 0.0;
} /* ResetClockRate */

void
Cleanup()
{
  ResetClockRate();

  if (msgctl(mqid, IPC_RMID, (struct msqid_ds *)0) == -1) {
    if (errno != EINVAL) {
      syslog(LOG_ERR, "remove message queue: %m");
      perror("adjtimed: remove message queue");
    }
  }

  Exit(2);
} /* Cleanup */

void
Exit(status)
     int status;
{
  syslog(LOG_ERR, "terminated");
  closelog();
  if (kmem != -1) close(kmem);
  exit(status);
} /* Exit */
