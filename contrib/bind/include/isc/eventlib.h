/*
 * Copyright (c) 1995-1999 by Internet Software Consortium
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/* eventlib.h - exported interfaces for eventlib
 * vix 09sep95 [initial]
 *
 * $Id: eventlib.h,v 1.23 2001/05/29 05:47:09 marka Exp $
 */

#ifndef _EVENTLIB_H
#define _EVENTLIB_H

#include <sys/types.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <stdio.h>

#ifndef __P
# define __EVENTLIB_P_DEFINED
# ifdef __STDC__
#  define __P(x) x
# else
#  define __P(x) ()
# endif
#endif

/* In the absence of branded types... */
typedef struct { void *opaque; } evConnID;
typedef struct { void *opaque; } evFileID;
typedef struct { void *opaque; } evStreamID;
typedef struct { void *opaque; } evTimerID;
typedef struct { void *opaque; } evWaitID;
typedef struct { void *opaque; } evContext;
typedef struct { void *opaque; } evEvent;

#define	evInitID(id) ((id)->opaque = NULL)
#define	evTestID(id) ((id).opaque != NULL)

typedef void (*evConnFunc)__P((evContext ctx, void *uap, int fd,
			       const void *la, int lalen,
			       const void *ra, int ralen));
typedef void (*evFileFunc)__P((evContext ctx, void *uap, int fd, int evmask));
typedef	void (*evStreamFunc)__P((evContext ctx, void *uap, int fd, int bytes));
typedef void (*evTimerFunc)__P((evContext ctx, void *uap,
				struct timespec due, struct timespec inter));
typedef	void (*evWaitFunc)__P((evContext ctx, void *uap, const void *tag));

typedef	struct { unsigned char mask[256/8]; } evByteMask;
#define	EV_BYTEMASK_BYTE(b) ((b) / 8)
#define	EV_BYTEMASK_MASK(b) (1 << ((b) % 8))
#define	EV_BYTEMASK_SET(bm, b) \
	((bm).mask[EV_BYTEMASK_BYTE(b)] |= EV_BYTEMASK_MASK(b))
#define	EV_BYTEMASK_CLR(bm, b) \
	((bm).mask[EV_BYTEMASK_BYTE(b)] &= ~EV_BYTEMASK_MASK(b))
#define	EV_BYTEMASK_TST(bm, b) \
	((bm).mask[EV_BYTEMASK_BYTE(b)] & EV_BYTEMASK_MASK(b))

#define	EV_POLL		1
#define	EV_WAIT		2
#define	EV_NULL		4

#define	EV_READ		1
#define	EV_WRITE	2
#define	EV_EXCEPT	4

/* eventlib.c */
#define evCreate	__evCreate
#define evSetDebug	__evSetDebug
#define evDestroy	__evDestroy
#define evGetNext	__evGetNext
#define evDispatch	__evDispatch
#define evDrop		__evDrop
#define evMainLoop	__evMainLoop
#define evHighestFD	__evHighestFD

int  evCreate __P((evContext *ctx));
void evSetDebug __P((evContext ctx, int lev, FILE *out));
int  evDestroy __P((evContext ctx));
int  evGetNext __P((evContext ctx, evEvent *ev, int options));
int  evDispatch __P((evContext ctx, evEvent ev));
void evDrop __P((evContext ctx, evEvent ev));
int  evMainLoop __P((evContext ctx));
int  evHighestFD __P((evContext ctx));

/* ev_connects.c */
#define evListen	__evListen
#define evConnect	__evConnect
#define evCancelConn	__evCancelConn
#define evHold		__evHold
#define evUnhold	__evUnhold
#define evTryAccept	__evTryAccept

int evListen __P((evContext ctx, int fd, int maxconn,
		  evConnFunc func, void *uap, evConnID *id));
int evConnect __P((evContext ctx, int fd, const void *ra, int ralen,
		   evConnFunc func, void *uap, evConnID *id));
int evCancelConn __P((evContext ctx, evConnID id));
int evHold __P((evContext, evConnID));
int evUnhold __P((evContext, evConnID));
int evTryAccept __P((evContext, evConnID, int *));

/* ev_files.c */
#define evSelectFD	__evSelectFD
#define evDeselectFD	__evDeselectFD

int evSelectFD __P((evContext ctx, int fd, int eventmask,
		    evFileFunc func, void *uap, evFileID *id));
int evDeselectFD __P((evContext ctx, evFileID id));

/* ev_streams.c */
#define evConsIovec	__evConsIovec
#define evWrite		__evWrite
#define evRead		__evRead
#define evTimeRW	__evTimeRW
#define evUntimeRW	__evUntimeRW
#define	evCancelRW	__evCancelRW

struct iovec evConsIovec __P((void *buf, size_t cnt));
int evWrite __P((evContext ctx, int fd, const struct iovec *iov, int cnt,
		 evStreamFunc func, void *uap, evStreamID *id));
int evRead __P((evContext ctx, int fd, const struct iovec *iov, int cnt,
		evStreamFunc func, void *uap, evStreamID *id));
int evTimeRW __P((evContext ctx, evStreamID id, evTimerID timer));
int evUntimeRW __P((evContext ctx, evStreamID id));
int evCancelRW __P((evContext ctx, evStreamID id));

/* ev_timers.c */
#define evConsTime	__evConsTime
#define evAddTime	__evAddTime
#define evSubTime	__evSubTime
#define evCmpTime	__evCmpTime
#define	evTimeSpec	__evTimeSpec
#define	evTimeVal	__evTimeVal

#define evNowTime		__evNowTime
#define evLastEventTime		__evLastEventTime
#define evSetTimer		__evSetTimer
#define evClearTimer		__evClearTimer
#define evResetTimer		__evResetTimer
#define evSetIdleTimer		__evSetIdleTimer
#define evClearIdleTimer	__evClearIdleTimer
#define evResetIdleTimer	__evResetIdleTimer
#define evTouchIdleTimer	__evTouchIdleTimer

struct timespec evConsTime __P((time_t sec, long nsec));
struct timespec evAddTime __P((struct timespec add1, struct timespec add2));
struct timespec evSubTime __P((struct timespec minu, struct timespec subtra));
struct timespec evNowTime __P((void));
struct timespec evLastEventTime __P((evContext));
struct timespec evTimeSpec __P((struct timeval));
struct timeval evTimeVal __P((struct timespec));
int evCmpTime __P((struct timespec a, struct timespec b));
int evSetTimer __P((evContext ctx, evTimerFunc func, void *uap,
		    struct timespec due, struct timespec inter,
		    evTimerID *id));
int evClearTimer __P((evContext ctx, evTimerID id));
int evResetTimer __P((evContext, evTimerID, evTimerFunc, void *,
		      struct timespec, struct timespec));
int evSetIdleTimer __P((evContext, evTimerFunc, void *, struct timespec,
			evTimerID *));
int evClearIdleTimer __P((evContext, evTimerID));
int evResetIdleTimer __P((evContext, evTimerID, evTimerFunc, void *,
			  struct timespec));
int evTouchIdleTimer __P((evContext, evTimerID));

/* ev_waits.c */
#define evWaitFor	__evWaitFor
#define evDo		__evDo
#define evUnwait	__evUnwait
#define evDefer		__evDefer

int evWaitFor __P((evContext ctx, const void *tag, evWaitFunc func, void *uap,
		   evWaitID *id));
int evDo __P((evContext ctx, const void *tag));
int evUnwait __P((evContext ctx, evWaitID id));
int evDefer __P((evContext, evWaitFunc, void *));

#ifdef __EVENTLIB_P_DEFINED
# undef __P
#endif

#endif /*_EVENTLIB_H*/
