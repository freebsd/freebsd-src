/*
 * refclock_shm - clock driver for utc via shared memory 
 * - under construction -
 * To add new modes: Extend or union the shmTime-struct. Do not
 * extend/shrink size, because otherwise existing implementations
 * will specify wrong size of shared memory-segment
 * PB 18.3.97
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#if defined(REFCLOCK) && defined(CLOCK_SHM)

#undef fileno   
#include <ctype.h>
#undef fileno   
#include <sys/time.h>
#undef fileno   

#include "ntpd.h"
#undef fileno   
#include "ntp_io.h"
#undef fileno   
#include "ntp_refclock.h"
#undef fileno   
#include "ntp_unixtime.h"
#undef fileno   
#include "ntp_stdlib.h"

#ifndef SYS_WINNT
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#endif

/*
 * This driver supports a reference clock attached thru shared memory
 */ 

/*
 * SHM interface definitions
 */
#define PRECISION       (-1)    /* precision assumed (0.5 s) */
#define REFID           "SHM"   /* reference ID */
#define DESCRIPTION     "SHM/Shared memory interface"

#define NSAMPLES        3       /* stages of median filter */

/*
 * Function prototypes
 */
static  int     shm_start       (int, struct peer *);
static  void    shm_shutdown    (int, struct peer *);
static  void    shm_poll        (int unit, struct peer *);

/*
 * Transfer vector
 */
struct  refclock refclock_shm = {
	shm_start,              /* start up driver */
	shm_shutdown,           /* shut down driver */
	shm_poll,               /* transmit poll message */
	noentry,                /* not used */
	noentry,                /* initialize driver (not used) */
	noentry,                /* not used */
	NOFLAGS                 /* not used */
};
struct shmTime {
	int    mode; /* 0 - if valid set
		      *       use values, 
		      *       clear valid
		      * 1 - if valid set 
		      *       if count before and after read of values is equal,
		      *         use values 
		      *       clear valid
		      */
	int    count;
	time_t clockTimeStampSec;
	int    clockTimeStampUSec;
	time_t receiveTimeStampSec;
	int    receiveTimeStampUSec;
	int    leap;
	int    precision;
	int    nsamples;
	int    valid;
	int    dummy[10]; 
};
struct shmTime *getShmTime (int unit) {
#ifndef SYS_WINNT
	extern char *sys_errlist[ ];
	extern int sys_nerr;
	int shmid=0;

	assert (unit<10); /* MAXUNIT is 4, so should never happen */
	shmid=shmget (0x4e545030+unit, sizeof (struct shmTime), 
		      IPC_CREAT|(unit<2?0700:0777));
	if (shmid==-1) { /*error */
		char buf[20];
		char *pe=buf;
		if (errno<sys_nerr)
		    pe=sys_errlist[errno];
		else {
			sprintf (buf,"errno=%d",errno);
		}
		msyslog(LOG_ERR,"SHM shmget (unit %d): %s",unit,pe);
		return 0;
	}
	else { /* no error  */
		struct shmTime *p=(struct shmTime *)shmat (shmid, 0, 0);
		if ((int)(long)p==-1) { /* error */
			char buf[20];
			char *pe=buf;
			if (errno<sys_nerr)
			    pe=sys_errlist[errno];
			else {
				sprintf (buf,"errno=%d",errno);
			}
			msyslog(LOG_ERR,"SHM shmat (unit %d): %s",unit,pe);
			return 0;
		}
		return p;
	}
#else
	char buf[10];
	LPSECURITY_ATTRIBUTES psec=0;
	HANDLE shmid=0;
	SECURITY_DESCRIPTOR sd;
	SECURITY_ATTRIBUTES sa;
	sprintf (buf,"NTP%d",unit);
	if (unit>=2) { /* world access */
		if (!InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION)) {
			msyslog(LOG_ERR,"SHM InitializeSecurityDescriptor (unit %d): %m",unit);
			return 0;
		}
		if (!SetSecurityDescriptorDacl(&sd,1,0,0)) {
			msyslog(LOG_ERR,"SHM SetSecurityDescriptorDacl (unit %d): %m",unit);
			return 0;
		}
		sa.nLength=sizeof (SECURITY_ATTRIBUTES);
		sa.lpSecurityDescriptor=&sd;
		sa.bInheritHandle=0;
		psec=&sa;
	}
	shmid=CreateFileMapping ((HANDLE)0xffffffff, psec, PAGE_READWRITE,
				 0, sizeof (struct shmTime),buf);
	if (!shmid) { /*error*/
		char buf[1000];
		FormatMessage (FORMAT_MESSAGE_FROM_SYSTEM,
			       0, GetLastError (), 0, buf, sizeof (buf), 0);
		msyslog(LOG_ERR,"SHM CreateFileMapping (unit %d): %s",unit,buf);
		return 0;
	}
	else {
		struct shmTime *p=(struct shmTime *) MapViewOfFile (shmid, 
								    FILE_MAP_WRITE, 0, 0, sizeof (struct shmTime));
		if (p==0) { /*error*/
			char buf[1000];
			FormatMessage (FORMAT_MESSAGE_FROM_SYSTEM,
				       0, GetLastError (), 0, buf, sizeof (buf), 0);
			msyslog(LOG_ERR,"SHM MapViewOfFile (unit %d): %s",unit,buf);
			return 0;
		}
		return p;
	}
#endif
	return 0;
}
/*
 * shm_start - attach to shared memory
 */
static int
shm_start(
	int unit,
	struct peer *peer
	)
{
	struct refclockproc *pp;
	pp = peer->procptr;
	pp->io.clock_recv = noentry;
	pp->io.srcclock = (caddr_t)peer;
	pp->io.datalen = 0;
	pp->io.fd = -1;
	pp->unitptr = (caddr_t)getShmTime(unit);

	/*
	 * Initialize miscellaneous peer variables
	 */
	memcpy((char *)&pp->refid, REFID, 4);
	if (pp->unitptr!=0) {
		((struct shmTime*)pp->unitptr)->precision=PRECISION;
		peer->precision = ((struct shmTime*)pp->unitptr)->precision;
		((struct shmTime*)pp->unitptr)->valid=0;
		((struct shmTime*)pp->unitptr)->nsamples=NSAMPLES;
		pp->clockdesc = DESCRIPTION;
		return (1);
	}
	else {
		return 0;
	}
}


/*
 * shm_shutdown - shut down the clock
 */
static void
shm_shutdown(
	int unit,
	struct peer *peer
	)
{
	register struct shmTime *up;
	struct refclockproc *pp;

	pp = peer->procptr;
	up = (struct shmTime *)pp->unitptr;
#ifndef SYS_WINNT
	shmdt (up);
#else
	UnmapViewOfFile (up);
#endif
}


/*
 * shm_poll - called by the transmit procedure
 */
static void
shm_poll(
	int unit,
	struct peer *peer
	)
{
	register struct shmTime *up;
	struct refclockproc *pp;

	/*
	 * This is the main routine. It snatches the time from the shm
	 * board and tacks on a local timestamp.
	 */
	pp = peer->procptr;
	up = (struct shmTime*)pp->unitptr;
	if (up==0) { /* try to map again - this may succeed if meanwhile some-
			body has ipcrm'ed the old (unaccessible) shared mem
			segment  */
		pp->unitptr = (caddr_t)getShmTime(unit);
		up = (struct shmTime*)pp->unitptr;
	}
	if (up==0) {
		refclock_report(peer, CEVNT_FAULT);
		return;
	}
	if (up->valid) {
		struct timeval tvr;
		struct timeval tvt;
		struct tm *t;
		int ok=1;
		switch (up->mode) {
		    case 0: {
			    tvr.tv_sec=up->receiveTimeStampSec;
			    tvr.tv_usec=up->receiveTimeStampUSec;
			    tvt.tv_sec=up->clockTimeStampSec;
			    tvt.tv_usec=up->clockTimeStampUSec;
		    }
		    break;
		    case 1: {
			    int cnt=up->count;
			    tvr.tv_sec=up->receiveTimeStampSec;
			    tvr.tv_usec=up->receiveTimeStampUSec;
			    tvt.tv_sec=up->clockTimeStampSec;
			    tvt.tv_usec=up->clockTimeStampUSec;
			    ok=(cnt==up->count);
		    }
		    break;
		    default:
			msyslog (LOG_ERR, "SHM: bad mode found in shared memory: %d",up->mode);
		}
		up->valid=0;
		if (ok) {
			TVTOTS(&tvr,&pp->lastrec);
			/* pp->lasttime = current_time; */
			pp->polls++;
			t=gmtime (&tvt.tv_sec);
			pp->day=t->tm_yday+1;
			pp->hour=t->tm_hour;
			pp->minute=t->tm_min;
			pp->second=t->tm_sec;
			pp->msec=0;
			pp->usec=tvt.tv_usec;
			peer->precision=up->precision;
			pp->leap=up->leap;
		} 
		else {
			refclock_report(peer, CEVNT_FAULT);
			msyslog (LOG_NOTICE, "SHM: access clash in shared memory");
			return;
		}
	}
	else {
		refclock_report(peer, CEVNT_TIMEOUT);
		msyslog (LOG_NOTICE, "SHM: no new value found in shared memory");
		return;
	}
	if (!refclock_process(pp)) {
		refclock_report(peer, CEVNT_BADTIME);
		return;
	}
	refclock_receive(peer);
}

#else
int refclock_shm_bs;
#endif /* REFCLOCK */
