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

/* eventlib.c - implement glue for the eventlib
 * vix 09sep95 [initial]
 */

#if !defined(LINT) && !defined(CODECENTER)
static const char rcsid[] = "$Id: eventlib.c,v 1.46 2001/11/01 05:35:48 marka Exp $";
#endif

#include "port_before.h"
#include "fd_setsize.h"

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>

#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>

#include <isc/eventlib.h>
#include <isc/assertions.h>
#include "eventlib_p.h"

#include "port_after.h"

/* Forward. */

#ifdef NEED_PSELECT
static int		pselect(int, void *, void *, void *,
				struct timespec *,
				const sigset_t *);
#endif

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
	FD_ZERO(&ctx->rdNext);
	FD_ZERO(&ctx->wrNext);
	FD_ZERO(&ctx->exNext);
	FD_ZERO(&ctx->nonblockBefore);
	ctx->fdMax = -1;
	ctx->fdNext = NULL;
	ctx->fdCount = 0;	/* Invalidate {rd,wr,ex}Last. */
	ctx->highestFD = FD_SETSIZE - 1;
#ifdef EVENTLIB_TIME_CHECKS
	ctx->lastFdCount = 0;
#endif
	memset(ctx->fdTable, 0, sizeof ctx->fdTable);

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
	int revs = 424242;	/* Doug Adams. */
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
		timerPast = 0;	/* Make gcc happy. */

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
			if (interval.tv_sec > 0)
				evPrintf(ctx, 1,
				   "time between pselect() %u.%09u count %d\n",
					 interval.tv_sec, interval.tv_nsec,
					 ctx->lastFdCount);
		}
#endif
		do {
			/* XXX need to copy only the bits we are using. */
			ctx->rdLast = ctx->rdNext;
			ctx->wrLast = ctx->wrNext;
			ctx->exLast = ctx->exNext;

			if (m == Timer) {
				INSIST(tp == &t);
				t = evSubTime(nextTime, ctx->lastEventTime);
			}

			evPrintf(ctx, 4,
				"pselect(%d, 0x%lx, 0x%lx, 0x%lx, %d.%09ld)\n",
				 ctx->fdMax+1,
				 (u_long)ctx->rdLast.fds_bits[0],
				 (u_long)ctx->wrLast.fds_bits[0],
				 (u_long)ctx->exLast.fds_bits[0],
				 tp ? tp->tv_sec : -1,
				 tp ? tp->tv_nsec : -1);

			/* XXX should predict system's earliness and adjust. */
			x = pselect(ctx->fdMax+1,
				    &ctx->rdLast, &ctx->wrLast, &ctx->exLast,
				    tp, NULL);
			pselect_errno = errno;

			evPrintf(ctx, 4, "select() returns %d (err: %s)\n",
				 x, (x == -1) ? strerror(errno) : "none");

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
			"Dispatch.Accept: fd %d -> %d, func %#x, uap %#x\n",
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
			"Dispatch.File: fd %d, mask 0x%x, func %#x, uap %#x\n",
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
			 "Dispatch.Stream: fd %d, func %#x, uap %#x\n",
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

		evPrintf(ctx, 5, "Dispatch.Timer: func %#x, uap %#x\n",
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
			 "Dispatch.Wait: tag %#x, func %#x, uap %#x\n",
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
		if (this->inter.tv_sec == 0 && this->inter.tv_nsec == 0L) {
			opaque.opaque = this;			
			(void) evClearTimer(opaqueCtx, opaque);
		} else {
			opaque.opaque = this;
			(void) evResetTimer(opaqueCtx, opaque, this->func,
					    this->uap,
					    evAddTime(ctx->lastEventTime,
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

#ifdef NEED_PSELECT
/* XXX needs to move to the porting library. */
static int
pselect(int nfds, void *rfds, void *wfds, void *efds,
	struct timespec *tsp,
	const sigset_t *sigmask)
{
	struct timeval tv, *tvp;
	sigset_t sigs;
	int n;

	if (tsp) {
		tvp = &tv;
		tv = evTimeVal(*tsp);
	} else
		tvp = NULL;
	if (sigmask)
		sigprocmask(SIG_SETMASK, sigmask, &sigs);
	n = select(nfds, rfds, wfds, efds, tvp);
	if (sigmask)
		sigprocmask(SIG_SETMASK, &sigs, NULL);
	if (tsp)
		*tsp = evTimeSpec(tv);
	return (n);
}
#endif
