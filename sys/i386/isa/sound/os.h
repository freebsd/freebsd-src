/*
 * os.h -- only included by sound_config.h right after local.h
 *
 * $FreeBSD: src/sys/i386/isa/sound/os.h,v 1.40.2.1 2000/08/03 01:01:27 peter Exp $
 */

#ifndef _OS_H_
#define _OS_H_

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/filio.h>
#include <sys/uio.h>
#include <sys/kernel.h>
#include <sys/poll.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/signalvar.h>

#include <sys/soundcard.h>

#include <i386/isa/isa_device.h>

#undef DELAY
#define DELAY(x)  tenmicrosec(x)
typedef struct uio snd_rw_buf;

struct snd_wait {
	int             mode;
	int             aborting;
};


unsigned long   get_time(void);

#endif	/* _OS_H_ */

typedef caddr_t ioctl_arg;

typedef struct sound_os_info {
	int             unit;
}               sound_os_info;


/*
 * The following macro calls tsleep. It should be implemented such that
 * the process is resumed if it receives a signal.
 * The q parameter is a wait_queue defined with DEFINE_WAIT_QUEUE(),
 * and the second is a workarea parameter. The third is a timeout 
 * in ticks. Zero means no timeout.
 */
#define DO_SLEEP(q, f, time_limit)	\
	{ \
	  int flag; \
	  f.mode = WK_SLEEP; \
	  flag=tsleep(&q, (PRIBIO-5)|PCATCH, "sndint", time_limit); \
	  f.mode &= ~WK_SLEEP; \
	  if (flag == EWOULDBLOCK) { \
		f.mode |= WK_TIMEOUT; \
		f.aborting = 0; \
	  } else \
		f.aborting = flag; \
	}

#define DO_SLEEP1(q, f, time_limit)	\
	{ \
	  int flag; \
	  f.mode = WK_SLEEP; \
	  flag=tsleep(&q, (PRIBIO-5)|PCATCH, "snd1", time_limit); \
	  f.mode &= ~WK_SLEEP; \
	  if (flag == EWOULDBLOCK) { \
		f.mode |= WK_TIMEOUT; \
		f.aborting = 0; \
	  } else \
		f.aborting = flag; \
	}

#define DO_SLEEP2(q, f, time_limit)	\
	{ \
	  int flag; \
	  f.mode = WK_SLEEP; \
	  flag=tsleep(&q, (PRIBIO-5)|PCATCH, "snd2", time_limit); \
	  f.mode &= ~WK_SLEEP; \
	  if (flag == EWOULDBLOCK) { \
		f.mode |= WK_TIMEOUT; \
		f.aborting = 0; \
	  } else \
		f.aborting = flag; \
	}

#define PROCESS_ABORTING( f) (f.aborting || CURSIG(curproc))
#define TIMED_OUT( f) (f.mode & WK_TIMEOUT)

#ifdef ALLOW_POLL
typedef struct proc select_table;
extern struct selinfo selinfo[];
#endif
