/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1995-1999 by Internet Software Consortium
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* eventlib.c - implement glue for the eventlib
 * vix 09sep95 [initial]
 */

#if !defined(LINT) && !defined(CODECENTER)
static const char rcsid[] = "$Id: eventlib.c,v 1.5.18.5 2006-03-10 00:20:08 marka Exp $";
#endif

#include "port_before.h"
#include "fd_setsize.h"

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#ifdef	SOLARIS2
#include <limits.h>
#endif	/* SOLARIS2 */

#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>

#include <isc/eventlib.h>
#include <isc/assertions.h>
#include "eventlib_p.h"

#include "port_after.h"

int      __evOptMonoTime;

#ifdef USE_POLL
#define	pselect Pselect
#endif /* USE_POLL */

/* Forward. */

#if defined(NEED_PSELECT) || defined(USE_POLL)
static int		pselect(int, void *, void *, void *,
				struct timespec *,
				const sigset_t *);
#endif

int    __evOptMonoTime;

/* Public. */

int
evCreate(evContext *opaqueCtx) {
	evContext_p *ctx;

	/* Make sure the memory heap is initialized. */
	if (meminit(0, 0) < 0 && errno != EEXIST)
		return (-1);

	OKNEW(ctx);

	/* Global. */
	ctx->cur = NULL;

	/* Debugging. */
	ctx->debug = 0;
	ctx->output = NULL;

	/* Connections. */
	ctx->conns = NULL;
	INIT_LIST(ctx->accepts);

	/* Files. */
	ctx->files = NULL;
#ifdef USE_POLL
        ctx->pollfds = NULL;
	ctx->maxnfds = 0;
	ctx->firstfd = 0;
	emulMaskInit(ctx, rdLast, EV_READ, 1);
	emulMaskInit(ctx, rdNext, EV_READ, 0);
	emulMaskInit(ctx, wrLast, EV_WRITE, 1);
	emulMaskInit(ctx, wrNext, EV_WRITE, 0);
	emulMaskInit(ctx, exLast, EV_EXCEPT, 1);
	emulMaskInit(ctx, exNext, EV_EXCEPT, 0);
	emulMaskInit(ctx, nonblockBefore, EV_WASNONBLOCKING, 0);
#endif /* USE_POLL */
	FD_ZERO(&ctx->rdNext);
	FD_ZERO(&ctx->wrNext);
	FD_ZERO(&ctx->exNext);
	FD_ZERO(&ctx->nonblockBefore);
	ctx->fdMax = -1;
	ctx->fdNext = NULL;
	ctx->fdCount = 0;	/*%< Invalidate {rd,wr,ex}Last. */
#ifndef USE_POLL
	ctx->highestFD = FD_SETSIZE - 1;
	memset(ctx->fdTable, 0, sizeof ctx->fdTable);
#else   
	ctx->highestFD = INT_MAX / sizeof(struct pollfd);
	ctx->fdTable = NULL;
#endif /* USE_POLL */
#ifdef EVENTLIB_TIME_CHECKS
	ctx->lastFdCount = 0;
#endif

	/* Streams. */
	ctx->streams = NULL;
	ctx->strDone = NULL;
	ctx->strLast = NULL;

	/* Timers. */
	ctx->lastEventTime = evNowTime();
#ifdef EVENTLIB_TIME_CHECKS
	ctx->lastSelectTime = ctx->lastEventTime;
#endif
	ctx->timers = evCreateTimers(ctx);
	if (ctx->timers == NULL)
		return (-1);

	/* Waits. */
	ctx->waitLists = NULL;
	ctx->waitDone.first = ctx->waitDone.last = NULL;
	ctx->waitDone.prev = ctx->waitDone.next = NULL;

	opaqueCtx->opaque = ctx;
	return (0);
}

void
evSetDebug(evContext opaqueCtx, int level, FILE *output) {
	evContext_p *ctx = opaqueCtx.opaque;

	ctx->debug = level;
	ctx->output = output;
}

int
evDestroy(evContext opaqueCtx) {
	evContext_p *ctx = opaqueCtx.opaque;
	int revs = 424242;	/*%< Doug Adams. */
	evWaitList *this_wl, *next_wl;
	evWait *this_wait, *next_wait;

	/* Connections. */
	while (revs-- > 0 && ctx->conns != NULL) {
		evConnID id;

		id.opaque = ctx->conns;
		(void) evCancelConn(opaqueCtx, id);
	}
	INSIST(revs >= 0);

	/* Streams. */
	while (revs-- > 0 && ctx->streams != NULL) {
		evStreamID id;

		id.opaque = ctx->streams;
		(void) evCancelRW(opaqueCtx, id);
	}

	/* Files. */
	while (revs-- > 0 && ctx->files != NULL) {
		evFileID id;

		id.opaque = ctx->files;
		(void) evDeselectFD(opaqueCtx, id);
	}
	INSIST(revs >= 0);

	/* Timers. */
	evDestroyTimers(ctx);

	/* Waits. */
	for (this_wl = ctx->waitLists;
	     revs-- > 0 && this_wl != NULL;
	     this_wl = next_wl) {
		next_wl = this_wl->next;
		for (this_wait = this_wl->first;
		     revs-- > 0 && this_wait != NULL;
		     this_wait = next_wait) {
			next_wait = this_wait->next;
			FREE(this_wait);
		}
		FREE(this_wl);
	}
	for (this_wait = ctx->waitDone.first;
	     revs-- > 0 && this_wait != NULL;
	     this_wait = next_wait) {
		next_wait = this_wait->next;
		FREE(this_wait);
	}

	FREE(ctx);
	return (0);
}

int
evGetNext(evContext opaqueCtx, evEvent *opaqueEv, int options) {
	evContext_p *ctx = opaqueCtx.opaque;
	struct timespec nextTime;
	evTimer *nextTimer;
	evEvent_p *new;
	int x, pselect_errno, timerPast;
#ifdef EVENTLIB_TIME_CHECKS
	struct timespec interval;
#endif

	/* Ensure that exactly one of EV_POLL or EV_WAIT was specified. */
	x = ((options & EV_POLL) != 0) + ((options & EV_WAIT) != 0);
	if (x != 1)
		EV_ERR(EINVAL);

	/* Get the time of day.  We'll do this again after select() blocks. */
	ctx->lastEventTime = evNowTime();

 again:
	/* Finished accept()'s do not require a select(). */
	if (!EMPTY(ctx->accepts)) {
		OKNEW(new);
		new->type = Accept;
		new->u.accept.this = HEAD(ctx->accepts);
		UNLINK(ctx->accepts, HEAD(ctx->accepts), link);
		opaqueEv->opaque = new;
		return (0);
	}

	/* Stream IO does not require a select(). */
	if (ctx->strDone != NULL) {
		OKNEW(new);
		new->type = Stream;
		new->u.stream.this = ctx->strDone;
		ctx->strDone = ctx->strDone->nextDone;
		if (ctx->strDone == NULL)
			ctx->strLast = NULL;
		opaqueEv->opaque = new;
		return (0);
	}

	/* Waits do not require a select(). */
	if (ctx->waitDone.first != NULL) {
		OKNEW(new);
		new->type = Wait;
		new->u.wait.this = ctx->waitDone.first;
		ctx->waitDone.first = ctx->waitDone.first->next;
		if (ctx->waitDone.first == NULL)
			ctx->waitDone.last = NULL;
		opaqueEv->opaque = new;
		return (0);
	}

	/* Get the status and content of the next timer. */
	if ((nextTimer = heap_element(ctx->timers, 1)) != NULL) {
		nextTime = nextTimer->due;
		timerPast = (evCmpTime(nextTime, ctx->lastEventTime) <= 0);
	} else
		timerPast = 0;	/*%< Make gcc happy. */
	evPrintf(ctx, 9, "evGetNext: fdCount %d\n", ctx->fdCount);
	if (ctx->fdCount == 0) {
		static const struct timespec NoTime = {0, 0L};
		enum { JustPoll, Block, Timer } m;
		struct timespec t, *tp;

		/* Are there any events at all? */
		if ((options & EV_WAIT) != 0 && !nextTimer && ctx->fdMax == -1)
			EV_ERR(ENOENT);

		/* Figure out what select()'s timeout parameter should be. */
		if ((options & EV_POLL) != 0) {
			m = JustPoll;
			t = NoTime;
			tp = &t;
		} else if (nextTimer == NULL) {
			m = Block;
			/* ``t'' unused. */
			tp = NULL;
		} else if (timerPast) {
			m = JustPoll;
			t = NoTime;
			tp = &t;
		} else {
			m = Timer;
			/* ``t'' filled in later. */
			tp = &t;
		}
#ifdef EVENTLIB_TIME_CHECKS
		if (ctx->debug > 0) {
			interval = evSubTime(ctx->lastEventTime,
					     ctx->lastSelectTime);
			if (interval.tv_sec > 0 || interval.tv_nsec > 0)
				evPrintf(ctx, 1,
				   "time between pselect() %u.%09u count %d\n",
					 interval.tv_sec, interval.tv_nsec,
					 ctx->lastFdCount);
		}
#endif
		do {
#ifndef USE_POLL
			 /* XXX need to copy only the bits we are using. */
			 ctx->rdLast = ctx->rdNext;
			 ctx->wrLast = ctx->wrNext;
			 ctx->exLast = ctx->exNext;
#else
			/*
			 * The pollfd structure uses separate fields for
			 * the input and output events (corresponding to
			 * the ??Next and ??Last fd sets), so there's no
			 * need to copy one to the other.
			 */
#endif /* USE_POLL */
			if (m == Timer) {
				INSIST(tp == &t);
				t = evSubTime(nextTime, ctx->lastEventTime);
			}

			/* XXX should predict system's earliness and adjust. */
			x = pselect(ctx->fdMax+1,
				    &ctx->rdLast, &ctx->wrLast, &ctx->exLast,
				    tp, NULL);
			pselect_errno = errno;

#ifndef USE_POLL
			evPrintf(ctx, 4, "select() returns %d (err: %s)\n",
				 x, (x == -1) ? strerror(errno) : "none");
#else
			evPrintf(ctx, 4, "poll() returns %d (err: %s)\n",
				x, (x == -1) ? strerror(errno) : "none");
#endif /* USE_POLL */
			/* Anything but a poll can change the time. */
			if (m != JustPoll)
				ctx->lastEventTime = evNowTime();

			/* Select() likes to finish about 10ms early. */
		} while (x == 0 && m == Timer &&
			 evCmpTime(ctx->lastEventTime, nextTime) < 0);
#ifdef EVENTLIB_TIME_CHECKS
		ctx->lastSelectTime = ctx->lastEventTime;
#endif
		if (x < 0) {
			if (pselect_errno == EINTR) {
				if ((options & EV_NULL) != 0)
					goto again;
				OKNEW(new);
				new->type = Null;
				/* No data. */
				opaqueEv->opaque = new;
				return (0);
			}
			if (pselect_errno == EBADF) {
				for (x = 0; x <= ctx->fdMax; x++) {
					struct stat sb;

					if (FD_ISSET(x, &ctx->rdNext) == 0 &&
					    FD_ISSET(x, &ctx->wrNext) == 0 &&
					    FD_ISSET(x, &ctx->exNext) == 0)
						continue;
					if (fstat(x, &sb) == -1 &&
					    errno == EBADF)
						evPrintf(ctx, 1, "EBADF: %d\n",
							 x);
				}
				abort();
			}
			EV_ERR(pselect_errno);
		}
		if (x == 0 && (nextTimer == NULL || !timerPast) &&
		    (options & EV_POLL))
			EV_ERR(EWOULDBLOCK);
		ctx->fdCount = x;
#ifdef EVENTLIB_TIME_CHECKS
		ctx->lastFdCount = x;
#endif
	}
	INSIST(nextTimer || ctx->fdCount);

	/* Timers go first since we'd like them to be accurate. */
	if (nextTimer && !timerPast) {
		/* Has anything happened since we blocked? */
		timerPast = (evCmpTime(nextTime, ctx->lastEventTime) <= 0);
	}
	if (nextTimer && timerPast) {
		OKNEW(new);
		new->type = Timer;
		new->u.timer.this = nextTimer;
		opaqueEv->opaque = new;
		return (0);
	}

	/* No timers, so there should be a ready file descriptor. */
	x = 0;
	while (ctx->fdCount > 0) {
		evFile *fid;
		int fd, eventmask;

		if (ctx->fdNext == NULL) {
			if (++x == 2) {
				/*
				 * Hitting the end twice means that the last
				 * select() found some FD's which have since
				 * been deselected.
				 *
				 * On some systems, the count returned by
				 * selects is the total number of bits in
				 * all masks that are set, and on others it's
				 * the number of fd's that have some bit set,
				 * and on others, it's just broken.  We 
				 * always assume that it's the number of
				 * bits set in all masks, because that's what
				 * the man page says it should do, and
				 * the worst that can happen is we do an
				 * extra select().
				 */
				ctx->fdCount = 0;
				break;
			}
			ctx->fdNext = ctx->files;
		}
		fid = ctx->fdNext;
		ctx->fdNext = fid->next;

		fd = fid->fd;
		eventmask = 0;
		if (FD_ISSET(fd, &ctx->rdLast))
			eventmask |= EV_READ;
		if (FD_ISSET(fd, &ctx->wrLast))
			eventmask |= EV_WRITE;
		if (FD_ISSET(fd, &ctx->exLast))
			eventmask |= EV_EXCEPT;
		eventmask &= fid->eventmask;
		if (eventmask != 0) {
			if ((eventmask & EV_READ) != 0) {
				FD_CLR(fd, &ctx->rdLast);
				ctx->fdCount--;
			}
			if ((eventmask & EV_WRITE) != 0) {
				FD_CLR(fd, &ctx->wrLast);
				ctx->fdCount--;
			}
			if ((eventmask & EV_EXCEPT) != 0) {
				FD_CLR(fd, &ctx->exLast);
				ctx->fdCount--;
			}
			OKNEW(new);
			new->type = File;
			new->u.file.this = fid;
			new->u.file.eventmask = eventmask;
			opaqueEv->opaque = new;
			return (0);
		}
	}
	if (ctx->fdCount < 0) {
		/*
		 * select()'s count is off on a number of systems, and
		 * can result in fdCount < 0.
		 */
		evPrintf(ctx, 4, "fdCount < 0 (%d)\n", ctx->fdCount);
		ctx->fdCount = 0;
	}

	/* We get here if the caller deselect()'s an FD. Gag me with a goto. */
	goto again;
}

int
evDispatch(evContext opaqueCtx, evEvent opaqueEv) {
	evContext_p *ctx = opaqueCtx.opaque;
	evEvent_p *ev = opaqueEv.opaque;
#ifdef EVENTLIB_TIME_CHECKS
	void *func;
	struct timespec start_time;
	struct timespec interval;
#endif

#ifdef EVENTLIB_TIME_CHECKS
	if (ctx->debug > 0)
		start_time = evNowTime();
#endif
	ctx->cur = ev;
	switch (ev->type) {
	    case Accept: {
		evAccept *this = ev->u.accept.this;

		evPrintf(ctx, 5,
			"Dispatch.Accept: fd %d -> %d, func %p, uap %p\n",
			 this->conn->fd, this->fd,
			 this->conn->func, this->conn->uap);
		errno = this->ioErrno;
		(this->conn->func)(opaqueCtx, this->conn->uap, this->fd,
				   &this->la, this->lalen,
				   &this->ra, this->ralen);
#ifdef EVENTLIB_TIME_CHECKS
		func = this->conn->func;
#endif
		break;
	    }
	    case File: {
		evFile *this = ev->u.file.this;
		int eventmask = ev->u.file.eventmask;

		evPrintf(ctx, 5,
			"Dispatch.File: fd %d, mask 0x%x, func %p, uap %p\n",
			 this->fd, this->eventmask, this->func, this->uap);
		(this->func)(opaqueCtx, this->uap, this->fd, eventmask);
#ifdef EVENTLIB_TIME_CHECKS
		func = this->func;
#endif
		break;
	    }
	    case Stream: {
		evStream *this = ev->u.stream.this;

		evPrintf(ctx, 5,
			 "Dispatch.Stream: fd %d, func %p, uap %p\n",
			 this->fd, this->func, this->uap);
		errno = this->ioErrno;
		(this->func)(opaqueCtx, this->uap, this->fd, this->ioDone);
#ifdef EVENTLIB_TIME_CHECKS
		func = this->func;
#endif
		break;
	    }
	    case Timer: {
		evTimer *this = ev->u.timer.this;

		evPrintf(ctx, 5, "Dispatch.Timer: func %p, uap %p\n",
			 this->func, this->uap);
		(this->func)(opaqueCtx, this->uap, this->due, this->inter);
#ifdef EVENTLIB_TIME_CHECKS
		func = this->func;
#endif
		break;
	    }
	    case Wait: {
		evWait *this = ev->u.wait.this;

		evPrintf(ctx, 5,
			 "Dispatch.Wait: tag %p, func %p, uap %p\n",
			 this->tag, this->func, this->uap);
		(this->func)(opaqueCtx, this->uap, this->tag);
#ifdef EVENTLIB_TIME_CHECKS
		func = this->func;
#endif
		break;
	    }
	    case Null: {
		/* No work. */
#ifdef EVENTLIB_TIME_CHECKS
		func = NULL;
#endif
		break;
	    }
	    default: {
		abort();
	    }
	}
#ifdef EVENTLIB_TIME_CHECKS
	if (ctx->debug > 0) {
		interval = evSubTime(evNowTime(), start_time);
		/* 
		 * Complain if it took longer than 50 milliseconds.
		 *
		 * We call getuid() to make an easy to find mark in a kernel
		 * trace.
		 */
		if (interval.tv_sec > 0 || interval.tv_nsec > 50000000)
			evPrintf(ctx, 1,
			 "dispatch interval %u.%09u uid %d type %d func %p\n",
				 interval.tv_sec, interval.tv_nsec,
				 getuid(), ev->type, func);
	}
#endif
	ctx->cur = NULL;
	evDrop(opaqueCtx, opaqueEv);
	return (0);
}

void
evDrop(evContext opaqueCtx, evEvent opaqueEv) {
	evContext_p *ctx = opaqueCtx.opaque;
	evEvent_p *ev = opaqueEv.opaque;

	switch (ev->type) {
	    case Accept: {
		FREE(ev->u.accept.this);
		break;
	    }
	    case File: {
		/* No work. */
		break;
	    }
	    case Stream: {
		evStreamID id;

		id.opaque = ev->u.stream.this;
		(void) evCancelRW(opaqueCtx, id);
		break;
	    }
	    case Timer: {
		evTimer *this = ev->u.timer.this;
		evTimerID opaque;

		/* Check to see whether the user func cleared the timer. */
		if (heap_element(ctx->timers, this->index) != this) {
			evPrintf(ctx, 5, "Dispatch.Timer: timer rm'd?\n");
			break;
		}
		/*
		 * Timer is still there.  Delete it if it has expired,
		 * otherwise set it according to its next interval.
		 */
		if (this->inter.tv_sec == (time_t)0 &&
		    this->inter.tv_nsec == 0L) {
			opaque.opaque = this;			
			(void) evClearTimer(opaqueCtx, opaque);
		} else {
			opaque.opaque = this;
			(void) evResetTimer(opaqueCtx, opaque, this->func,
					    this->uap,
					    evAddTime((this->mode & EV_TMR_RATE) ?
						      this->due :
						      ctx->lastEventTime,
						      this->inter),
					    this->inter);
		}
		break;
	    }
	    case Wait: {
		FREE(ev->u.wait.this);
		break;
	    }
	    case Null: {
		/* No work. */
		break;
	    }
	    default: {
		abort();
	    }
	}
	FREE(ev);
}

int
evMainLoop(evContext opaqueCtx) {
	evEvent event;
	int x;

	while ((x = evGetNext(opaqueCtx, &event, EV_WAIT)) == 0)
		if ((x = evDispatch(opaqueCtx, event)) < 0)
			break;
	return (x);
}

int
evHighestFD(evContext opaqueCtx) {
	evContext_p *ctx = opaqueCtx.opaque;

	return (ctx->highestFD);
}

void
evPrintf(const evContext_p *ctx, int level, const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	if (ctx->output != NULL && ctx->debug >= level) {
		vfprintf(ctx->output, fmt, ap);
		fflush(ctx->output);
	}
	va_end(ap);
}

int
evSetOption(evContext *opaqueCtx, const char *option, int value) {
	/* evContext_p *ctx = opaqueCtx->opaque; */

	UNUSED(opaqueCtx);
	UNUSED(value);
#ifndef CLOCK_MONOTONIC
	UNUSED(option);
#endif 

#ifdef CLOCK_MONOTONIC
	if (strcmp(option, "monotime") == 0) {
		if (opaqueCtx  != NULL)
			errno = EINVAL;
		if (value == 0 || value == 1) {
			__evOptMonoTime = value;
			return (0);
		} else {
			errno = EINVAL;
			return (-1);
		}
	} 
#endif
	errno = ENOENT;
	return (-1);
}

int
evGetOption(evContext *opaqueCtx, const char *option, int *value) {
	/* evContext_p *ctx = opaqueCtx->opaque; */

	UNUSED(opaqueCtx);
#ifndef CLOCK_MONOTONIC
	UNUSED(value);
	UNUSED(option);
#endif 

#ifdef CLOCK_MONOTONIC
	if (strcmp(option, "monotime") == 0) {
		if (opaqueCtx  != NULL)
			errno = EINVAL;
		*value = __evOptMonoTime;
		return (0);
	}
#endif
	errno = ENOENT;
	return (-1);
}

#if defined(NEED_PSELECT) || defined(USE_POLL)
/* XXX needs to move to the porting library. */
static int
pselect(int nfds, void *rfds, void *wfds, void *efds,
	struct timespec *tsp,
	const sigset_t *sigmask)
{
	struct timeval tv, *tvp;
	sigset_t sigs;
	int n;
#ifdef USE_POLL
	int	polltimeout = INFTIM;
	evContext_p	*ctx;
	struct pollfd	*fds;
	nfds_t		pnfds;

	UNUSED(nfds);
#endif /* USE_POLL */

	if (tsp) {
		tvp = &tv;
		tv = evTimeVal(*tsp);
#ifdef USE_POLL
		polltimeout = 1000 * tv.tv_sec + tv.tv_usec / 1000;
#endif /* USE_POLL */
	} else
		tvp = NULL;
	if (sigmask)
		sigprocmask(SIG_SETMASK, sigmask, &sigs);
#ifndef USE_POLL
	 n = select(nfds, rfds, wfds, efds, tvp);
#else
        /*
	 * rfds, wfds, and efds should all be from the same evContext_p,
	 * so any of them will do. If they're all NULL, the caller is
	 * presumably calling us to block.
	 */
	if (rfds != NULL)
		ctx = ((__evEmulMask *)rfds)->ctx;
	else if (wfds != NULL)
		ctx = ((__evEmulMask *)wfds)->ctx;
	else if (efds != NULL)
		ctx = ((__evEmulMask *)efds)->ctx;
	else
		ctx = NULL;
	if (ctx != NULL && ctx->fdMax != -1) {
		fds = &(ctx->pollfds[ctx->firstfd]);
		pnfds = ctx->fdMax - ctx->firstfd + 1;
	} else {
		fds = NULL;
		pnfds = 0;
	}
	n = poll(fds, pnfds, polltimeout);
	if (n > 0) {
		int     i, e;

		INSIST(ctx != NULL);
		for (e = 0, i = ctx->firstfd; i <= ctx->fdMax; i++) {
			if (ctx->pollfds[i].fd < 0)
				continue;
			if (FD_ISSET(i, &ctx->rdLast))
				e++;
			if (FD_ISSET(i, &ctx->wrLast))
				e++;
			if (FD_ISSET(i, &ctx->exLast))
				e++;
		}
		n = e;
	}
#endif /* USE_POLL */
	if (sigmask)
		sigprocmask(SIG_SETMASK, &sigs, NULL);
	if (tsp)
		*tsp = evTimeSpec(tv);
	return (n);
}
#endif

#ifdef USE_POLL
int
evPollfdRealloc(evContext_p *ctx, int pollfd_chunk_size, int fd) {
 
	int     i, maxnfds;
	void	*pollfds, *fdTable;
 
	if (fd < ctx->maxnfds)
		return (0);
 
	/* Don't allow ridiculously small values for pollfd_chunk_size */
	if (pollfd_chunk_size < 20)
		pollfd_chunk_size = 20;
 
	maxnfds = (1 + (fd/pollfd_chunk_size)) * pollfd_chunk_size;
 
	pollfds = realloc(ctx->pollfds, maxnfds * sizeof(*ctx->pollfds));
	if (pollfds != NULL)
		ctx->pollfds = pollfds;
	fdTable = realloc(ctx->fdTable, maxnfds * sizeof(*ctx->fdTable));
	if (fdTable != NULL)
		ctx->fdTable = fdTable;
 
	if (pollfds == NULL || fdTable == NULL) {
		evPrintf(ctx, 2, "pollfd() realloc (%ld) failed\n",
			 (long)maxnfds*sizeof(struct pollfd));
		return (-1);
	}
 
	for (i = ctx->maxnfds; i < maxnfds; i++) {
		ctx->pollfds[i].fd = -1;
		ctx->pollfds[i].events = 0;
		ctx->fdTable[i] = 0;
	}

	ctx->maxnfds = maxnfds;

	return (0);
}
 
/* Find the appropriate 'events' or 'revents' field in the pollfds array */
short *
__fd_eventfield(int fd, __evEmulMask *maskp) {
 
	evContext_p     *ctx = (evContext_p *)maskp->ctx;
 
	if (!maskp->result || maskp->type == EV_WASNONBLOCKING)
		return (&(ctx->pollfds[fd].events));
	else
		return (&(ctx->pollfds[fd].revents));
}
 
/* Translate to poll(2) event */
short
__poll_event(__evEmulMask *maskp) {
 
	switch ((maskp)->type) {
	case EV_READ:
		return (POLLRDNORM);
	case EV_WRITE:
		return (POLLWRNORM);
	case EV_EXCEPT:
		return (POLLRDBAND | POLLPRI | POLLWRBAND);
	case EV_WASNONBLOCKING:
		return (POLLHUP);
	default:
		return (0);
	}
}
 
/*
 * Clear the events corresponding to the specified mask. If this leaves
 * the events mask empty (apart from the POLLHUP bit), set the fd field
 * to -1 so that poll(2) will ignore this fd.
 */
void
__fd_clr(int fd, __evEmulMask *maskp) {
 
	evContext_p     *ctx = maskp->ctx;
 
	*__fd_eventfield(fd, maskp) &= ~__poll_event(maskp);
	if ((ctx->pollfds[fd].events & ~POLLHUP) == 0) {
		ctx->pollfds[fd].fd = -1;
		if (fd == ctx->fdMax)
			while (ctx->fdMax > ctx->firstfd &&
			       ctx->pollfds[ctx->fdMax].fd < 0)
				ctx->fdMax--;
		if (fd == ctx->firstfd)
			while (ctx->firstfd <= ctx->fdMax &&
			       ctx->pollfds[ctx->firstfd].fd < 0)
				ctx->firstfd++;
		/*
		 * Do we have a empty set of descriptors?
		 */
		if (ctx->firstfd > ctx->fdMax) {
			ctx->fdMax = -1;
			ctx->firstfd = 0;
		}
	}
}
 
/*
 * Set the events bit(s) corresponding to the specified mask. If the events
 * field has any other bits than POLLHUP set, also set the fd field so that
 * poll(2) will watch this fd.
 */
void
__fd_set(int fd, __evEmulMask *maskp) {
 
	evContext_p     *ctx = maskp->ctx;

	*__fd_eventfield(fd, maskp) |= __poll_event(maskp);
	if ((ctx->pollfds[fd].events & ~POLLHUP) != 0) {
		ctx->pollfds[fd].fd = fd;
		if (fd < ctx->firstfd || ctx->fdMax == -1)
			ctx->firstfd = fd;
		if (fd > ctx->fdMax)
			ctx->fdMax = fd;
	}
}
#endif /* USE_POLL */

/*! \file */
