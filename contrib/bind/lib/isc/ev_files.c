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

/* ev_files.c - implement asynch file IO for the eventlib
 * vix 11sep95 [initial]
 */

#if !defined(LINT) && !defined(CODECENTER)
static const char rcsid[] = "$Id: ev_files.c,v 1.22 2002/07/08 05:50:07 marka Exp $";
#endif

#include "port_before.h"
#include "fd_setsize.h"

#include <sys/types.h>
#include <sys/time.h>
#include <sys/ioctl.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <isc/eventlib.h>
#include "eventlib_p.h"

#include "port_after.h"

static evFile *FindFD(const evContext_p *ctx, int fd, int eventmask);

int
evSelectFD(evContext opaqueCtx,
	   int fd,
	   int eventmask,
	   evFileFunc func,
	   void *uap,
	   evFileID *opaqueID
) {
	evContext_p *ctx = opaqueCtx.opaque;
	evFile *id;
	int mode;

	evPrintf(ctx, 1,
		 "evSelectFD(ctx %p, fd %d, mask 0x%x, func %p, uap %p)\n",
		 ctx, fd, eventmask, func, uap);
	if (eventmask == 0 || (eventmask & ~EV_MASK_ALL) != 0)
		EV_ERR(EINVAL);
	if (fd > ctx->highestFD)
		EV_ERR(EINVAL);
	OK(mode = fcntl(fd, F_GETFL, NULL));	/* side effect: validate fd. */

	/*
	 * The first time we touch a file descriptor, we need to check to see
	 * if the application already had it in O_NONBLOCK mode and if so, all
	 * of our deselect()'s have to leave it in O_NONBLOCK.  If not, then
	 * all but our last deselect() has to leave it in O_NONBLOCK.
	 */
	id = FindFD(ctx, fd, EV_MASK_ALL);
	if (id == NULL) {
		if (mode & PORT_NONBLOCK)
			FD_SET(fd, &ctx->nonblockBefore);
		else {
#ifdef USE_FIONBIO_IOCTL
			int on = 1;
			OK(ioctl(fd, FIONBIO, (char *)&on));
#else
			OK(fcntl(fd, F_SETFL, mode | PORT_NONBLOCK));
#endif
			FD_CLR(fd, &ctx->nonblockBefore);
		}
	}

	/*
	 * If this descriptor is already in use, search for it again to see
	 * if any of the eventmask bits we want to set are already captured.
	 * We cannot usefully capture the same fd event more than once in the
	 * same context.
	 */
	if (id != NULL && FindFD(ctx, fd, eventmask) != NULL)
		EV_ERR(ETOOMANYREFS);

	/* Allocate and fill. */
	OKNEW(id);
	id->func = func;
	id->uap = uap;
	id->fd = fd;
	id->eventmask = eventmask;

	/*
	 * Insert at head.  Order could be important for performance if we
	 * believe that evGetNext()'s accesses to the fd_sets will be more
	 * serial and therefore more cache-lucky if the list is ordered by
	 * ``fd.''  We do not believe these things, so we don't do it.
	 *
	 * The interesting sequence is where GetNext() has cached a select()
	 * result and the caller decides to evSelectFD() on some descriptor.
	 * Since GetNext() starts at the head, it can miss new entries we add
	 * at the head.  This is not a serious problem since the event being
	 * evSelectFD()'d for has to occur before evSelectFD() is called for
	 * the file event to be considered "missed" -- a real corner case.
	 * Maintaining a "tail" pointer for ctx->files would fix this, but I'm
	 * not sure it would be ``more correct.''
	 */
	if (ctx->files != NULL)
		ctx->files->prev = id;
	id->prev = NULL;
	id->next = ctx->files;
	ctx->files = id;

	/* Insert into fd table. */
	if (ctx->fdTable[fd] != NULL)
		ctx->fdTable[fd]->fdprev = id;
	id->fdprev = NULL;
	id->fdnext = ctx->fdTable[fd];
	ctx->fdTable[fd] = id;

	/* Turn on the appropriate bits in the {rd,wr,ex}Next fd_set's. */
	if (eventmask & EV_READ)
		FD_SET(fd, &ctx->rdNext);
	if (eventmask & EV_WRITE)
		FD_SET(fd, &ctx->wrNext);
	if (eventmask & EV_EXCEPT)
		FD_SET(fd, &ctx->exNext);

	/* Update fdMax. */
	if (fd > ctx->fdMax)
		ctx->fdMax = fd;

	/* Remember the ID if the caller provided us a place for it. */
	if (opaqueID)
		opaqueID->opaque = id;

	evPrintf(ctx, 5,
		"evSelectFD(fd %d, mask 0x%x): new masks: 0x%lx 0x%lx 0x%lx\n",
		 fd, eventmask,
		 (u_long)ctx->rdNext.fds_bits[0],
		 (u_long)ctx->wrNext.fds_bits[0],
		 (u_long)ctx->exNext.fds_bits[0]);

	return (0);
}

int
evDeselectFD(evContext opaqueCtx, evFileID opaqueID) {
	evContext_p *ctx = opaqueCtx.opaque;
	evFile *del = opaqueID.opaque;
	evFile *cur;
	int mode, eventmask;

	if (!del) {
		evPrintf(ctx, 11, "evDeselectFD(NULL) ignored\n");
		errno = EINVAL;
		return (-1);
	}

	evPrintf(ctx, 1, "evDeselectFD(fd %d, mask 0x%x)\n",
		 del->fd, del->eventmask);

	/* Get the mode.  Unless the file has been closed, errors are bad. */
	mode = fcntl(del->fd, F_GETFL, NULL);
	if (mode == -1 && errno != EBADF)
		EV_ERR(errno);

	/* Remove from the list of files. */
	if (del->prev != NULL)
		del->prev->next = del->next;
	else
		ctx->files = del->next;
	if (del->next != NULL)
		del->next->prev = del->prev;

	/* Remove from the fd table. */
	if (del->fdprev != NULL)
		del->fdprev->fdnext = del->fdnext;
	else
		ctx->fdTable[del->fd] = del->fdnext;
	if (del->fdnext != NULL)
		del->fdnext->fdprev = del->fdprev;

	/*
	 * If the file descriptor does not appear in any other select() entry,
	 * and if !EV_WASNONBLOCK, and if we got no EBADF when we got the mode
	 * earlier, then: restore the fd to blocking status.
	 */
	if (!(cur = FindFD(ctx, del->fd, EV_MASK_ALL)) &&
	    !FD_ISSET(del->fd, &ctx->nonblockBefore) &&
	    mode != -1) {
		/*
		 * Note that we won't return an error status to the caller if
		 * this fcntl() fails since (a) we've already done the work
		 * and (b) the caller didn't ask us anything about O_NONBLOCK.
		 */
#ifdef USE_FIONBIO_IOCTL
		int off = 1;
		(void) ioctl(del->fd, FIONBIO, (char *)&off);
#else
		(void) fcntl(del->fd, F_SETFL, mode & ~PORT_NONBLOCK);
#endif
	}

	/*
	 * Now find all other uses of this descriptor and OR together an event
	 * mask so that we don't turn off {rd,wr,ex}Next bits that some other
	 * file event is using.  As an optimization, stop if the event mask
	 * fills.
	 */
	eventmask = 0;
	for ((void)NULL;
	     cur != NULL && eventmask != EV_MASK_ALL;
	     cur = cur->next)
		if (cur->fd == del->fd)
			eventmask |= cur->eventmask;

	/* OK, now we know which bits we can clear out. */
	if (!(eventmask & EV_READ)) {
		FD_CLR(del->fd, &ctx->rdNext);
		if (FD_ISSET(del->fd, &ctx->rdLast)) {
			FD_CLR(del->fd, &ctx->rdLast);
			ctx->fdCount--;
		}
	}
	if (!(eventmask & EV_WRITE)) {
		FD_CLR(del->fd, &ctx->wrNext);
		if (FD_ISSET(del->fd, &ctx->wrLast)) {
			FD_CLR(del->fd, &ctx->wrLast);
			ctx->fdCount--;
		}
	}
	if (!(eventmask & EV_EXCEPT)) {
		FD_CLR(del->fd, &ctx->exNext);
		if (FD_ISSET(del->fd, &ctx->exLast)) {
			FD_CLR(del->fd, &ctx->exLast);
			ctx->fdCount--;
		}
	}

	/* If this was the maxFD, find the new one. */
	if (del->fd == ctx->fdMax) {
		ctx->fdMax = -1;
		for (cur = ctx->files; cur; cur = cur->next)
			if (cur->fd > ctx->fdMax)
				ctx->fdMax = cur->fd;
	}

	/* If this was the fdNext, cycle that to the next entry. */
	if (del == ctx->fdNext)
		ctx->fdNext = del->next;

	evPrintf(ctx, 5,
	      "evDeselectFD(fd %d, mask 0x%x): new masks: 0x%lx 0x%lx 0x%lx\n",
		 del->fd, eventmask,
		 (u_long)ctx->rdNext.fds_bits[0],
		 (u_long)ctx->wrNext.fds_bits[0],
		 (u_long)ctx->exNext.fds_bits[0]);

	/* Couldn't free it before now since we were using fields out of it. */
	FREE(del);

	return (0);
}

static evFile *
FindFD(const evContext_p *ctx, int fd, int eventmask) {
	evFile *id;

	for (id = ctx->fdTable[fd]; id != NULL; id = id->fdnext)
		if (id->fd == fd && (id->eventmask & eventmask) != 0)
			break;
	return (id);
}
