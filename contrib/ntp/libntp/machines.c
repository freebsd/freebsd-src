/* machines.c - provide special support for peculiar architectures
 *
 * Real bummers unite !
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ntp_machine.h"
#include "ntp_syslog.h"
#include "ntp_stdlib.h"
#include "ntp_unixtime.h"

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef SYS_WINNT
# include <conio.h>
#else

#ifdef SYS_VXWORKS
#include "taskLib.h"
#include "sysLib.h"
#include "time.h"
#include "ntp_syslog.h"

/*	some translations to the world of vxWorkings -casey */
/* first some netdb type things */
#include "ioLib.h"
#include <socket.h>
int h_errno;

struct hostent *gethostbyname(char *name)
	{
	struct hostent *host1;
	h_errno = 0;					/* we are always successful!!! */
	host1 = (struct hostent *) malloc (sizeof(struct hostent));
	host1->h_name = name;
	host1->h_addrtype = AF_INET;
	host1->h_aliases = name;
	host1->h_length = 4;
	host1->h_addr_list[0] = (char *)hostGetByName (name);
	host1->h_addr_list[1] = NULL;
	return host1;
	}

struct hostent *gethostbyaddr(char *name, int size, int addr_type)
	{
	struct hostent *host1;
	h_errno = 0;  /* we are always successful!!! */
	host1 = (struct hostent *) malloc (sizeof(struct hostent));
	host1->h_name = name;
	host1->h_addrtype = AF_INET;
	host1->h_aliases = name;
	host1->h_length = 4;
	host1->h_addr_list = NULL;
	return host1;
	}

struct servent *getservbyname (char *name, char *type)
	{
	struct servent *serv1;
	serv1 = (struct servent *) malloc (sizeof(struct servent));
	serv1->s_name = "ntp";      /* official service name */
	serv1->s_aliases = NULL;	/* alias list */
	serv1->s_port = 123;		/* port # */
	serv1->s_proto = "udp";     /* protocol to use */
	return serv1;
	}

/* second
 * vxworks thinks it has insomnia
 * we have to sleep for number of seconds
 */

#define CLKRATE 	sysClkRateGet()

/* I am not sure how valid the granularity is - it is from G. Eger's port */
#define CLK_GRANULARITY  1		/* Granularity of system clock in usec	*/
								/* Used to round down # usecs/tick		*/
								/* On a VCOM-100, PIT gets 8 MHz clk,	*/
								/*	& it prescales by 32, thus 4 usec	*/
								/* on mv167, granularity is 1usec anyway*/
								/* To defeat rounding, set to 1 		*/
#define USECS_PER_SEC		MILLION		/* Microseconds per second	*/
#define TICK (((USECS_PER_SEC / CLKRATE) / CLK_GRANULARITY) * CLK_GRANULARITY)

/* emulate unix sleep
 * casey
 */
void sleep(int seconds)
	{
	taskDelay(seconds*TICK);
	}
/* emulate unix alarm
 * that pauses and calls SIGALRM after the seconds are up...
 * so ... taskDelay() fudged for seconds should amount to the same thing.
 * casey
 */
void alarm (int seconds)
	{
	sleep(seconds);
	}

#endif /* SYS_VXWORKS */

#ifdef SYS_PTX			/* Does PTX still need this? */
/*#include <sys/types.h>	*/
#include <sys/procstats.h>

int
gettimeofday(
	struct timeval *tvp
	)
{
	/*
	 * hi, this is Sequents sneak path to get to a clock
	 * this is also the most logical syscall for such a function
	 */
	return (get_process_stats(tvp, PS_SELF, (struct procstats *) 0,
				  (struct procstats *) 0));
}
#endif /* SYS_PTX */

const char *set_tod_using = "UNKNOWN";

int
ntp_set_tod(
	struct timeval *tvp,
	void *tzp
	)
{
	int rc;

#ifdef HAVE_CLOCK_SETTIME
	{
		struct timespec ts;

		/* Convert timeval to timespec */
		ts.tv_sec = tvp->tv_sec;
		ts.tv_nsec = 1000 *  tvp->tv_usec;

		rc = clock_settime(CLOCK_REALTIME, &ts);
		if (!rc)
		{
			set_tod_using = "clock_settime";
			return rc;
		}
	}
#endif /* HAVE_CLOCK_SETTIME */
#ifdef HAVE_SETTIMEOFDAY
	{
		rc = SETTIMEOFDAY(tvp, tzp);
		if (!rc)
		{
			set_tod_using = "settimeofday";
			return rc;
		}
	}
#endif /* HAVE_SETTIMEOFDAY */
#ifdef HAVE_STIME
	{
		long tp = tvp->tv_sec;

		rc = stime(&tp); /* lie as bad as SysVR4 */
		if (!rc)
		{
			set_tod_using = "stime";
			return rc;
		}
	}
#endif /* HAVE_STIME */
	set_tod_using = "Failed!";
	return -1;
}

#endif /* not SYS_WINNT */

#if defined (SYS_WINNT) || defined (SYS_VXWORKS)
/* getpass is used in ntpq.c and ntpdc.c */

char *
getpass(const char * prompt)
{
	int c, i;
	static char password[32];
#ifdef DEBUG
	fprintf(stderr, "%s", prompt);
	fflush(stderr);
#endif
	for (i=0; i<sizeof(password)-1 && ((c=_getch())!='\n'); i++) {
		password[i] = (char) c;
	}
	password[i] = '\0';

	return password;
}
#endif /* SYS_WINNT */

#if !defined(HAVE_MEMSET)
void
ntp_memset(
	char *a,
	int x,
	int c
	)
{
	while (c-- > 0)
		*a++ = (char) x;
}
#endif /*POSIX*/
