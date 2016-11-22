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
IoHndPad_T*  __fastcall
iohpCreate(
	void *	src
	)
{
	IoHndPad_T* retv;

	retv = IOCPLPoolAlloc(sizeof(IoHndPad_T), "Lock");
	if (retv != NULL) {
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
IoHndPad_T*  __fastcall
iohpAttach(
	IoHndPad_T *	lp
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
IoHndPad_T*  __fastcall
iohpDetach(
	IoHndPad_T *	lp
	)
{
	if (lp != NULL && !InterlockedDecrement(&lp->refc_count)) {
		memset(lp, 0xFF, sizeof(IoHndPad_T));
		IOCPLPoolFree(lp, "Lock");
	}
	return NULL;
}

/* --------------------------------------------------------------------
 * Predicate function: Is there an attached RIO, and is the RIO in
 * active state?
 */
BOOL __fastcall
iohpRefClockOK(
	const IoHndPad_T *	lp
	)
{
	return	lp->rsrc.rio && lp->rsrc.rio->active;
}

/* --------------------------------------------------------------------
 * Predicate function: Is there an attached interface, and is the
 * interface accepting packets?
 */
BOOL __fastcall
iohpEndPointOK(
const IoHndPad_T *	lp
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
iohpQueueLocked(
	CIoHndPad_T *	lp,
	IoPreCheck_T	pred,
	recvbuf_t *	buf
	)
{
	BOOL	done = FALSE;
	if (lp) {
		done = (*pred)(lp);
		if (done)
			add_full_recv_buffer(buf);
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
		devCtx->ref_count = 1;	/* already owned! */
		/* The initial COV values make sure there is no busy
		* loop on unused/empty slots.
		*/
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
	IoHndPad_T *	lock,
	DevCtx_t *	devCtx
	)
{
	IoCtx_t *	ctx;

	ctx = (IoCtx_t *)IOCPLPoolAlloc(sizeof(IoCtx_t), "IO ctx");
	if (ctx != NULL) {
		ctx->iopad = iohpAttach(lock);
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
		ctx->iopad  = iohpDetach(ctx->iopad);
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
		"Release overlapped IO data buffer";

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
		ctx->iopad		&&
		ctx->iopad->rsrc.any;
}

/* --------------------------------------------------------------------
 * Start an IO operation on a given context object with a specified
 * function and buffer.
 *
 * Returns TRUE if the starter was executed successfully, FALSE in
 * all other cases.
 *
 * !!NOTE!! The context object and the buffer are consumed by this
 * call IN ANY CASE, independent of the function result!
 */
BOOL
IoCtxStartChecked(
	IoCtx_t *	lpo,
	IoCtxStarterT	func,
	recvbuf_t *	buf
	)
{
	BOOL		done  = FALSE;
	IoHndPad_T *	iopad = lpo->iopad;
	if (iopad != NULL) {
		if ((lpo->io.hnd == iopad->handles[0]) ||
		    (lpo->io.hnd == iopad->handles[1])  )
		{
			done = (func)(lpo, buf);
			lpo  = NULL; /* consumed by 'func' */
		}
	}
	if (lpo != NULL) {
		freerecvbuf(buf);
		IoCtxFree(lpo);
	}
	return done;
}

/* -*- that's all folks -*- */
