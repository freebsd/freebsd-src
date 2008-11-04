/* $FreeBSD$ */
/*-
 * Copyright (c) 2008 Hans Petter Selasky. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#define	USB_DEBUG_VAR usb2_proc_debug

#include <dev/usb2/core/usb2_core.h>
#include <dev/usb2/core/usb2_process.h>
#include <dev/usb2/core/usb2_debug.h>
#include <dev/usb2/core/usb2_util.h>

#include <sys/proc.h>
#include <sys/kthread.h>
#include <sys/sched.h>

#if (__FreeBSD_version < 700000)
#define	thread_lock(td) mtx_lock_spin(&sched_lock)
#define	thread_unlock(td) mtx_unlock_spin(&sched_lock)
#endif

#if (__FreeBSD_version >= 800000)
#define	USB_THREAD_CREATE(f, s, p, ...) \
		kproc_create((f), (s), (p), RFHIGHPID, 0, __VA_ARGS__)
#define	USB_THREAD_SUSPEND(p)   kproc_suspend(p,0)
#define	USB_THREAD_EXIT(err)	kproc_exit(err)
#else
#define	USB_THREAD_CREATE(f, s, p, ...) \
		kthread_create((f), (s), (p), RFHIGHPID, 0, __VA_ARGS__)
#define	USB_THREAD_SUSPEND(p)   kthread_suspend(p,0)
#define	USB_THREAD_EXIT(err)	kthread_exit(err)
#endif

#if USB_DEBUG
static int usb2_proc_debug;

SYSCTL_NODE(_hw_usb2, OID_AUTO, proc, CTLFLAG_RW, 0, "USB process");
SYSCTL_INT(_hw_usb2_proc, OID_AUTO, debug, CTLFLAG_RW, &usb2_proc_debug, 0,
    "Debug level");
#endif

/*------------------------------------------------------------------------*
 *	usb2_process
 *
 * This function is the USB process dispatcher.
 *------------------------------------------------------------------------*/
static void
usb2_process(void *arg)
{
	struct usb2_process *up = arg;
	struct usb2_proc_msg *pm;
	struct thread *td;

	/* adjust priority */
	td = curthread;
	thread_lock(td);
	sched_prio(td, up->up_prio);
	thread_unlock(td);

	mtx_lock(up->up_mtx);

	up->up_curtd = td;

	while (1) {

		if (up->up_gone) {
			break;
		}
		/*
		 * NOTE to reimplementors: dequeueing a command from the
		 * "used" queue and executing it must be atomic, with regard
		 * to the "up_mtx" mutex. That means any attempt to queue a
		 * command by another thread must be blocked until either:
		 *
		 * 1) the command sleeps
		 *
		 * 2) the command returns
		 *
		 * Here is a practical example that shows how this helps
		 * solving a problem:
		 *
		 * Assume that you want to set the baud rate on a USB serial
		 * device. During the programming of the device you don't
		 * want to receive nor transmit any data, because it will be
		 * garbage most likely anyway. The programming of our USB
		 * device takes 20 milliseconds and it needs to call
		 * functions that sleep.
		 *
		 * Non-working solution: Before we queue the programming
		 * command, we stop transmission and reception of data. Then
		 * we queue a programming command. At the end of the
		 * programming command we enable transmission and reception
		 * of data.
		 *
		 * Problem: If a second programming command is queued while the
		 * first one is sleeping, we end up enabling transmission
		 * and reception of data too early.
		 *
		 * Working solution: Before we queue the programming command,
		 * we stop transmission and reception of data. Then we queue
		 * a programming command. Then we queue a second command
		 * that only enables transmission and reception of data.
		 *
		 * Why it works: If a second programming command is queued
		 * while the first one is sleeping, then the queueing of a
		 * second command to enable the data transfers, will cause
		 * the previous one, which is still on the queue, to be
		 * removed from the queue, and re-inserted after the last
		 * baud rate programming command, which then gives the
		 * desired result.
		 */
		pm = TAILQ_FIRST(&up->up_qhead);

		if (pm) {
			DPRINTF("Message pm=%p, cb=%p (enter)\n",
			    pm, pm->pm_callback);

			(pm->pm_callback) (pm);

			if (pm == TAILQ_FIRST(&up->up_qhead)) {
				/* nothing changed */
				TAILQ_REMOVE(&up->up_qhead, pm, pm_qentry);
				pm->pm_qentry.tqe_prev = NULL;
			}
			DPRINTF("Message pm=%p (leave)\n", pm);

			continue;
		}
		/* end if messages - check if anyone is waiting for sync */
		if (up->up_dsleep) {
			up->up_dsleep = 0;
			usb2_cv_broadcast(&up->up_drain);
		}
		up->up_msleep = 1;
		usb2_cv_wait(&up->up_cv, up->up_mtx);
	}

	up->up_ptr = NULL;
	usb2_cv_signal(&up->up_cv);
	mtx_unlock(up->up_mtx);

	USB_THREAD_EXIT(0);
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_proc_setup
 *
 * This function will create a process using the given "prio" that can
 * execute callbacks. The mutex pointed to by "p_mtx" will be applied
 * before calling the callbacks and released after that the callback
 * has returned. The structure pointed to by "up" is assumed to be
 * zeroed before this function is called.
 *
 * Return values:
 *    0: success
 * Else: failure
 *------------------------------------------------------------------------*/
uint8_t
usb2_proc_setup(struct usb2_process *up, struct mtx *p_mtx, uint8_t prio)
{
	up->up_mtx = p_mtx;
	up->up_prio = prio;

	TAILQ_INIT(&up->up_qhead);

	usb2_cv_init(&up->up_cv, "WMSG");
	usb2_cv_init(&up->up_drain, "DMSG");

	if (USB_THREAD_CREATE(&usb2_process, up,
	    &up->up_ptr, "USBPROC")) {
		DPRINTFN(0, "Unable to create USB process.");
		up->up_ptr = NULL;
		goto error;
	}
	return (0);

error:
	usb2_proc_unsetup(up);
	return (1);
}

/*------------------------------------------------------------------------*
 *	usb2_proc_unsetup
 *
 * NOTE: If the structure pointed to by "up" is all zero, this
 * function does nothing.
 *
 * NOTE: Messages that are pending on the process queue will not be
 * removed nor called.
 *------------------------------------------------------------------------*/
void
usb2_proc_unsetup(struct usb2_process *up)
{
	if (!(up->up_mtx)) {
		/* not initialised */
		return;
	}
	usb2_proc_drain(up);

	usb2_cv_destroy(&up->up_cv);
	usb2_cv_destroy(&up->up_drain);

	/* make sure that we do not enter here again */
	up->up_mtx = NULL;
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_proc_msignal
 *
 * This function will queue one of the passed USB process messages on
 * the USB process queue. The first message that is not already queued
 * will get queued. If both messages are already queued the one queued
 * last will be removed from the queue and queued in the end. The USB
 * process mutex must be locked when calling this function. This
 * function exploits the fact that a process can only do one callback
 * at a time. The message that was queued is returned.
 *------------------------------------------------------------------------*/
void   *
usb2_proc_msignal(struct usb2_process *up, void *_pm0, void *_pm1)
{
	struct usb2_proc_msg *pm0 = _pm0;
	struct usb2_proc_msg *pm1 = _pm1;
	struct usb2_proc_msg *pm2;
	uint32_t d;
	uint8_t t;

	mtx_assert(up->up_mtx, MA_OWNED);

	t = 0;

	if (pm0->pm_qentry.tqe_prev) {
		t |= 1;
	}
	if (pm1->pm_qentry.tqe_prev) {
		t |= 2;
	}
	if (t == 0) {
		/*
		 * No entries are queued. Queue "pm0" and use the existing
		 * message number.
		 */
		pm2 = pm0;
	} else if (t == 1) {
		/* Check if we need to increment the message number. */
		if (pm0->pm_num == up->up_msg_num) {
			up->up_msg_num++;
		}
		pm2 = pm1;
	} else if (t == 2) {
		/* Check if we need to increment the message number. */
		if (pm1->pm_num == up->up_msg_num) {
			up->up_msg_num++;
		}
		pm2 = pm0;
	} else if (t == 3) {
		/*
		 * Both entries are queued. Re-queue the entry closest to
		 * the end.
		 */
		d = (pm1->pm_num - pm0->pm_num);

		/* Check sign after subtraction */
		if (d & 0x80000000) {
			pm2 = pm0;
		} else {
			pm2 = pm1;
		}

		TAILQ_REMOVE(&up->up_qhead, pm2, pm_qentry);
	} else {
		pm2 = NULL;		/* panic - should not happen */
	}

	DPRINTF(" t=%u, num=%u\n", t, up->up_msg_num);

	/* Put message last on queue */

	pm2->pm_num = up->up_msg_num;
	TAILQ_INSERT_TAIL(&up->up_qhead, pm2, pm_qentry);

	/* Check if we need to wakeup the USB process. */

	if (up->up_msleep) {
		up->up_msleep = 0;	/* save "cv_signal()" calls */
		usb2_cv_signal(&up->up_cv);
	}
	return (pm2);
}

/*------------------------------------------------------------------------*
 *	usb2_proc_is_gone
 *
 * Return values:
 *    0: USB process is running
 * Else: USB process is tearing down
 *------------------------------------------------------------------------*/
uint8_t
usb2_proc_is_gone(struct usb2_process *up)
{
	mtx_assert(up->up_mtx, MA_OWNED);

	return (up->up_gone ? 1 : 0);
}

/*------------------------------------------------------------------------*
 *	usb2_proc_mwait
 *
 * This function will return when the USB process message pointed to
 * by "pm" is no longer on a queue. This function must be called
 * having "up->up_mtx" locked.
 *------------------------------------------------------------------------*/
void
usb2_proc_mwait(struct usb2_process *up, void *_pm0, void *_pm1)
{
	struct usb2_proc_msg *pm0 = _pm0;
	struct usb2_proc_msg *pm1 = _pm1;

	mtx_assert(up->up_mtx, MA_OWNED);

	if (up->up_curtd == curthread) {
		/* Just remove the messages from the queue. */
		if (pm0->pm_qentry.tqe_prev) {
			TAILQ_REMOVE(&up->up_qhead, pm0, pm_qentry);
			pm0->pm_qentry.tqe_prev = NULL;
		}
		if (pm1->pm_qentry.tqe_prev) {
			TAILQ_REMOVE(&up->up_qhead, pm1, pm_qentry);
			pm1->pm_qentry.tqe_prev = NULL;
		}
	} else
		while (pm0->pm_qentry.tqe_prev ||
		    pm1->pm_qentry.tqe_prev) {
			/* check if config thread is gone */
			if (up->up_gone)
				break;
			up->up_dsleep = 1;
			usb2_cv_wait(&up->up_drain, up->up_mtx);
		}
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_proc_drain
 *
 * This function will tear down an USB process, waiting for the
 * currently executing command to return.
 *
 * NOTE: If the structure pointed to by "up" is all zero,
 * this function does nothing.
 *------------------------------------------------------------------------*/
void
usb2_proc_drain(struct usb2_process *up)
{
	if (!(up->up_mtx)) {
		/* not initialised */
		return;
	}
	if (up->up_mtx != &Giant) {
		mtx_assert(up->up_mtx, MA_NOTOWNED);
	}
	mtx_lock(up->up_mtx);

	/* Set the gone flag */

	up->up_gone = 1;

	while (up->up_ptr) {

		/* Check if we need to wakeup the USB process */

		if (up->up_msleep || up->up_csleep) {
			up->up_msleep = 0;
			up->up_csleep = 0;
			usb2_cv_signal(&up->up_cv);
		}
		/* Check if we are still cold booted */

		if (cold) {
			USB_THREAD_SUSPEND(up->up_ptr);
			printf("WARNING: A USB process has been left suspended!\n");
			break;
		}
		usb2_cv_wait(&up->up_cv, up->up_mtx);
	}
	/* Check if someone is waiting - should not happen */

	if (up->up_dsleep) {
		up->up_dsleep = 0;
		usb2_cv_broadcast(&up->up_drain);
		DPRINTF("WARNING: Someone is waiting "
		    "for USB process drain!\n");
	}
	mtx_unlock(up->up_mtx);
	return;
}

/*------------------------------------------------------------------------*
 *	usb2_proc_cwait
 *
 * This function will suspend the current process until
 * "usb2_proc_signal()" or "usb2_proc_drain()" is called. The
 * "timeout" parameter defines the maximum wait time in system
 * ticks. If "timeout" is zero that means no timeout.
 *
 * NOTE: This function can only be called from within an USB process.
 *
 * Return values:
 *   USB_PROC_WAIT_TIMEOUT: Timeout
 *   USB_PROC_WAIT_NORMAL: Success
 *   Else: USB process is tearing down
 *------------------------------------------------------------------------*/
uint8_t
usb2_proc_cwait(struct usb2_process *up, int timeout)
{
	int error;

	mtx_assert(up->up_mtx, MA_OWNED);

	if (up->up_gone) {
		return (USB_PROC_WAIT_DRAIN);
	}
	up->up_csleep = 1;

	if (timeout == 0) {
		usb2_cv_wait(&up->up_cv, up->up_mtx);
		error = 0;
	} else {
		error = usb2_cv_timedwait(&up->up_cv, up->up_mtx, timeout);
	}

	up->up_csleep = 0;

	if (up->up_gone) {
		return (USB_PROC_WAIT_DRAIN);
	}
	if (error == EWOULDBLOCK) {
		return (USB_PROC_WAIT_TIMEOUT);
	}
	return (0);
}

/*------------------------------------------------------------------------*
 *	usb2_proc_csignal
 *
 * This function will wakeup the given USB process.
 *------------------------------------------------------------------------*/
void
usb2_proc_csignal(struct usb2_process *up)
{
	mtx_assert(up->up_mtx, MA_OWNED);

	if (up->up_csleep) {
		up->up_csleep = 0;
		usb2_cv_signal(&up->up_cv);
	}
	return;
}
