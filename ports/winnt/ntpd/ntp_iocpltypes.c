/*
 * ntp_iocpltypes.c - data structures for overlapped IO
 *
 * Written by Juergen Perlinger (perlinger@ntp.org) for the NTP project.
 * The contents of 'html/copyright.html' apply.
 *
 * --------------------------------------------------------------------
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stddef.h>

#include "ntpd.h"
#include "ntp_iocplmem.h"
#include "ntp_iocpltypes.h"

/*
* ====================================================================
* Shared lock manipulation
* ====================================================================
*/

/* --------------------------------------------------------------------
 * Create new shared lock node. The shared lock is returned with a
 * refcount of 1, so the caller owns this immediately. The internal
 * lock is NOT aquired, and all IO handles or FDs are set to an
 * invalid value.
 */
SharedLock_t*  __fastcall
slCreate(
	void *	src
	)
{
	SharedLock_t* retv;

	retv = IOCPLPoolAlloc(sizeof(SharedLock_t), "Lock");
	if (retv != NULL) {
		InitializeCriticalSection(retv->mutex);
		retv->refc_count = 1;
		retv->rsrc.any   = src;
		retv->handles[0] = INVALID_HANDLE_VALUE;
		retv->handles[1] = INVALID_HANDLE_VALUE;
		retv->riofd	 = -1;
	}
	return retv;
}

/* --------------------------------------------------------------------
 * Attach to a lock. This just increments the use count, but does not
 * aquire the internal lock. Return a pointer to the lock.
 */
SharedLock_t*  __fastcall
slAttach(
	SharedLock_t *	lp
	)
{
	if (lp != NULL)
		InterlockedIncrement(&lp->refc_count);
	return lp;

}

/* --------------------------------------------------------------------
 * Detach from a shared lock. If the use count drops to zero, the lock
 * is destroyed and released.
 * Alwys return NULL.
 *
 * THE CALLER MUST NOT OWN THE INTERNAL LOCK WHEN DOING THIS!
 */
SharedLock_t*  __fastcall
slDetach(
	SharedLock_t *	lp
	)
{
	if (lp != NULL && !InterlockedDecrement(&lp->refc_count)) {
		DeleteCriticalSection(lp->mutex);
		memset(lp, 0xFF, sizeof(SharedLock_t));
		IOCPLPoolFree(lp, "Lock");
	}
	return NULL;
}

/* --------------------------------------------------------------------
 * Attach and aquire the lock for READ access. (This might block)
 */
SharedLock_t*  __fastcall
slAttachShared(
	SharedLock_t *	lp
	)
{
	if (NULL != (lp = slAttach(lp)))
		EnterCriticalSection(lp->mutex);
	return lp;
}

/* --------------------------------------------------------------------
 * Release the READ lock and detach from shared lock.
 * Alwys returns NULL.
 *
 * THE CALLER MUST OWN THE READ LOCK WHEN DOING THIS.
 */
SharedLock_t*  __fastcall
slDetachShared(
	SharedLock_t *	lp
	)
{
	if (lp != NULL)
		LeaveCriticalSection(lp->mutex);
	return slDetach(lp);
}

/* --------------------------------------------------------------------
 * Attach and aquire the lock for WRITE access. (This might block)
 */
SharedLock_t*  __fastcall
slAttachExclusive(
	SharedLock_t *	lp
)
{
	if (NULL != (lp = slAttach(lp)))
		EnterCriticalSection(lp->mutex);
	return lp;
}

/* --------------------------------------------------------------------
 * Release the WRITE lock and detach from shared lock.
 * Alwys returns NULL.
 *
 * THE CALLER MUST OWN THE WRITE LOCK WHEN DOING THIS.
 */
SharedLock_t*  __fastcall
slDetachExclusive(
	SharedLock_t *	lp
	)
{
	if (lp != NULL)
		LeaveCriticalSection(lp->mutex);
	return slDetach(lp);
}

/* --------------------------------------------------------------------
 * Predicate function: Is there an attached RIO, and is the RIO in
 * active state?
 */
BOOL __fastcall
slRefClockOK(
	const SharedLock_t *	lp
	)
{
	return	lp->rsrc.rio && lp->rsrc.rio->active;
}

/* --------------------------------------------------------------------
 * Predicate function: Is there an attached interface, and is the
 * interface accepting packets?
 */
BOOL __fastcall
slEndPointOK(
const SharedLock_t *	lp
)
{
	return	lp->rsrc.ept &&	!lp->rsrc.ept->ignore_packets;
}

/* --------------------------------------------------------------------
 * Enqueue a receive buffer under lock guard, but only if the shared
 * lock is still active and a given predicate function holds.
 *
 * Returns TRUE if buffer was queued, FALSE in all other cases.
 *
 * !!NOTE!! The buffer is consumed by this call IN ANY CASE,
 * independent of the function result!
 */
BOOL
slQueueLocked(
	SharedLock_t *	lp,
	LockPredicateT	pred,
	recvbuf_t *	buf
	)
{
	BOOL	done = FALSE;
	if (slAttachShared(lp)) {
		done = (*pred)(lp);
		if (done)
			add_full_recv_buffer(buf);
		slDetachShared(lp);
	}
	if (done)
		SetEvent(WaitableIoEventHandle);
	else
		freerecvbuf(buf);
	return done;
}


/* ====================================================================
 * Alloc & Free of Device context
 * ====================================================================
 */

/* !NOTE! The returned context is already owned by the caller! */
DevCtx_t * __fastcall
DevCtxAlloc(void)
{
	DevCtx_t *	devCtx;
	u_long		slot;

	/* allocate struct and tag all slots as invalid */
	devCtx = (DevCtx_t *)IOCPLPoolAlloc(sizeof(DevCtx_t), "DEV ctx");
	if (devCtx != NULL) {
		/* The initial COV values make sure there is no busy
		* loop on unused/empty slots.
		*/
		devCtx->cov_count = 1;	/* already owned! */
		for (slot = 0; slot < PPS_QUEUE_LEN; slot++)
			devCtx->pps_buff[slot].cov_count = ~slot;
	}
	return devCtx;
}

DevCtx_t * __fastcall
DevCtxAttach(
	DevCtx_t *	devCtx
	)
{
	if (devCtx != NULL)
		InterlockedIncrement(&devCtx->ref_count);
	return devCtx;
}

DevCtx_t * __fastcall
DevCtxDetach(
	DevCtx_t *	devCtx
	)
{
	if (devCtx && !InterlockedDecrement(&devCtx->ref_count))
		IOCPLPoolFree(devCtx, "DEV ctx");
	return NULL;
}

/* ====================================================================
 * Alloc & Free of I/O context
 * ====================================================================
 */

/* --------------------------------------------------------------------
 * Allocate a new IO transfer context node and attach it to the lock
 * and device context given. (Either or both may be NULL.)
 * Returns new node, or NULL on error.
 */
IoCtx_t * __fastcall
IoCtxAlloc(
	SharedLock_t *	lock,
	DevCtx_t *	devCtx
	)
{
	IoCtx_t *	ctx;

	ctx = (IoCtx_t *)IOCPLPoolAlloc(sizeof(IoCtx_t), "IO ctx");
	if (ctx != NULL) {
		ctx->slock = slAttach(lock);
		ctx->devCtx = DevCtxAttach(devCtx);
	}
	return ctx;
}

/* --------------------------------------------------------------------
 * Free an IO transfer context node after detaching it from lock and
 * device context.
 *
 * This does *NOT* free any attache data buffers! Use 'IoCtxRelease()'
 * for dropping the node and attached buffers.
 */
void __fastcall
IoCtxFree(
	IoCtx_t *	ctx
	)
{
	if (ctx) {
		ctx->slock  = slDetach(ctx->slock);
		ctx->devCtx = DevCtxDetach(ctx->devCtx);
		IOCPLPoolFree(ctx, "IO ctx");
	}
}

/* --------------------------------------------------------------------
 * Free an IO transfer context node after detaching it from lock and
 * device context.
 *
 * Also disposes of any attached data buffers -- the buffer pointers
 * should either be a valid reference or NULL.
 */
void __fastcall
IoCtxRelease(
	IoCtx_t * 	ctx
	)
{
	static const char *const dmsg =
		"overlapped IO data buffer";

	if (ctx) {
		if (ctx->flRawMem)
			IOCPLPoolFree(ctx->trans_buf, dmsg);
		else
			freerecvbuf(ctx->recv_buf);
		IoCtxFree(ctx);
	}
}

/* --------------------------------------------------------------------
 * Check if any source is attached to shared lock associated with
 * this context node.
 *
 * UNGUARDED -- ONLY CALL UNDER LOCK.
 */
BOOL __fastcall
IoCtxAlive(
	IoCtx_t *	ctx
	)
{
	return ctx			&&
		ctx->slock		&&
		ctx->slock->rsrc.any;
}

/* --------------------------------------------------------------------
 * Start an IO operation on a given context object with a specified
 * function and buffer.
 * This locks the shared lock on the context, checks for the lock
 * being active, and only then runs the starter function.
 *
 * Returns TRUE if the starter was executed successfully, FALSE in
 * all other cases.
 *
 * !!NOTE!! The context object and the buffer are consumed by this
 * call IN ANY CASE, independent of the function result!
 */
BOOL
IoCtxStartLocked(
	IoCtx_t *	lpo,
	IoCtxStarterT	func,
	recvbuf_t *	buf
	)
{
	BOOL		done = FALSE;
	SharedLock_t *	slock = slAttachShared(lpo->slock);
	if (slock != NULL) {
		if ((lpo->io.hnd == slock->handles[0]) ||
		    (lpo->io.hnd == slock->handles[1])  )
		{
			done = (func)(lpo, buf);
			lpo = NULL; /* consumed by 'func' */
		}
		slDetachShared(slock);
	}
	if (lpo != NULL) {
		freerecvbuf(buf);
		IoCtxFree(lpo);
	}
	return done;
}

/* -*- that's all folks -*- */
