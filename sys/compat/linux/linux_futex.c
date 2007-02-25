/*	$NetBSD: linux_futex.c,v 1.5 2005/11/23 16:14:57 manu Exp $ */

/*-
 * Copyright (c) 2005 Emmanuel Dreyfus, all rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Emmanuel Dreyfus
 * 4. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE THE AUTHOR AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#if 0
__KERNEL_RCSID(1, "$NetBSD: linux_futex.c,v 1.5 2005/11/23 16:14:57 manu Exp $");
#endif

#include "opt_compat.h"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sx.h>
#include <sys/malloc.h>

#ifdef COMPAT_LINUX32
#include <machine/../linux32/linux.h>
#include <machine/../linux32/linux32_proto.h>
#else
#include <machine/../linux/linux.h>
#include <machine/../linux/linux_proto.h>
#endif
#include <compat/linux/linux_futex.h>

struct futex;

struct waiting_proc {
	struct thread *wp_t;
	struct futex *wp_new_futex;
	TAILQ_ENTRY(waiting_proc) wp_list;
};
struct futex {
	void   *f_uaddr;
	int	f_refcount;
	LIST_ENTRY(futex) f_list;
	TAILQ_HEAD(lf_waiting_proc, waiting_proc) f_waiting_proc;
};

LIST_HEAD(futex_list, futex) futex_list;
struct sx futex_sx;		/* this protects the LIST of futexes */

#define FUTEX_LOCK sx_xlock(&futex_sx)
#define FUTEX_UNLOCK sx_xunlock(&futex_sx)

#define FUTEX_LOCKED	1
#define FUTEX_UNLOCKED	0

#define FUTEX_SYSTEM_LOCK mtx_lock(&Giant)
#define FUTEX_SYSTEM_UNLOCK mtx_unlock(&Giant)

static struct futex	*futex_get(void *, int);
static void futex_put(struct futex *);
static int futex_sleep(struct futex *, struct thread *, unsigned long);
static int futex_wake(struct futex *, int, struct futex *);
#ifdef __i386__
static int futex_atomic_op(struct thread *td, int encoded_op, caddr_t uaddr);
#endif 

/* support.s */
int futex_xchgl(int oparg, caddr_t uaddr, int *oldval);
int futex_addl(int oparg, caddr_t uaddr, int *oldval);
int futex_orl(int oparg, caddr_t uaddr, int *oldval);
int futex_andnl(int oparg, caddr_t uaddr, int *oldval);
int futex_xorl(int oparg, caddr_t uaddr, int *oldval);

int
linux_sys_futex(struct thread *td, struct linux_sys_futex_args *args)
{
	int val;
	int ret;
	struct l_timespec timeout = {0, 0};
	int error = 0;
	struct futex *f;
	struct futex *newf;
	int timeout_hz;
	struct timeval tv = {0, 0};
#ifdef __i386__
	struct futex *f2;
	int op_ret;
#endif

#ifdef	DEBUG
	if (ldebug(sys_futex))
		printf(ARGS(futex, "%p, %i, %i"), args->uaddr, args->op,
		    args->val);
#endif

	switch (args->op) {
	case LINUX_FUTEX_WAIT:
		FUTEX_SYSTEM_LOCK;

		if ((error = copyin(args->uaddr,
		    &val, sizeof(val))) != 0) {
			FUTEX_SYSTEM_UNLOCK;
			return error;
		}

		if (val != args->val) {
			FUTEX_SYSTEM_UNLOCK;
			return EWOULDBLOCK;
		}

		if (args->timeout != NULL) {
			if ((error = copyin(args->timeout,
			    &timeout, sizeof(timeout))) != 0) {
				FUTEX_SYSTEM_UNLOCK;
				return error;
			}
		}

#ifdef DEBUG
		if (ldebug(sys_futex))
			printf("FUTEX_WAIT %d: val = %d, uaddr = %p, "
			    "*uaddr = %d, timeout = %d.%09lu\n",
			    td->td_proc->p_pid, args->val,
			    args->uaddr, val, timeout.tv_sec,
			    (unsigned long)timeout.tv_nsec);
#endif
		tv.tv_usec = timeout.tv_sec * 1000000 + timeout.tv_nsec / 1000;
		timeout_hz = tvtohz(&tv);

		if (timeout.tv_sec == 0 && timeout.tv_nsec == 0)
			timeout_hz = 0;
		/*
		 * If the user process requests a non null timeout,
		 * make sure we do not turn it into an infinite
		 * timeout because timeout_hz gets null.
		 *
		 * We use a minimal timeout of 1/hz. Maybe it would
		 * make sense to just return ETIMEDOUT without sleeping.
		 */
		if (((timeout.tv_sec != 0) || (timeout.tv_nsec != 0)) &&
		    (timeout_hz == 0))
			timeout_hz = 1;


		f = futex_get(args->uaddr, FUTEX_UNLOCKED);
		ret = futex_sleep(f, td, timeout_hz);
		futex_put(f);

#ifdef DEBUG
		if (ldebug(sys_futex))
			printf("FUTEX_WAIT %d: uaddr = %p, "
			    "ret = %d\n", td->td_proc->p_pid, args->uaddr, ret);
#endif

		FUTEX_SYSTEM_UNLOCK;
		switch (ret) {
		case EWOULDBLOCK:	/* timeout */
			return ETIMEDOUT;
			break;
		case EINTR:		/* signal */
			return EINTR;
			break;
		case 0:		/* FUTEX_WAKE received */
#ifdef DEBUG
			if (ldebug(sys_futex))
				printf("FUTEX_WAIT %d: uaddr = %p, got FUTEX_WAKE\n",
				    td->td_proc->p_pid, args->uaddr);
#endif
			return 0;
			break;
		default:
#ifdef DEBUG
			if (ldebug(sys_futex))
				printf("FUTEX_WAIT: unexpected ret = %d\n", ret);
#endif
			break;
		}

		/* NOTREACHED */
		break;

	case LINUX_FUTEX_WAKE:
		FUTEX_SYSTEM_LOCK;

		/*
		 * XXX: Linux is able cope with different addresses
		 * corresponding to the same mapped memory in the sleeping
		 * and the waker process.
		 */
#ifdef DEBUG
		if (ldebug(sys_futex))
			printf("FUTEX_WAKE %d: uaddr = %p, val = %d\n",
			    td->td_proc->p_pid, args->uaddr, args->val);
#endif
		f = futex_get(args->uaddr, FUTEX_UNLOCKED);
		td->td_retval[0] = futex_wake(f, args->val, NULL);
		futex_put(f);

		FUTEX_SYSTEM_UNLOCK;
		break;

	case LINUX_FUTEX_CMP_REQUEUE:
		FUTEX_SYSTEM_LOCK;

		if ((error = copyin(args->uaddr,
		    &val, sizeof(val))) != 0) {
			FUTEX_SYSTEM_UNLOCK;
			return error;
		}

		if (val != args->val3) {
			FUTEX_SYSTEM_UNLOCK;
			return EAGAIN;
		}

		f = futex_get(args->uaddr, FUTEX_UNLOCKED);
		newf = futex_get(args->uaddr2, FUTEX_UNLOCKED);
		td->td_retval[0] = futex_wake(f, args->val, newf);
		futex_put(f);
		futex_put(newf);

		FUTEX_SYSTEM_UNLOCK;
		break;

	case LINUX_FUTEX_REQUEUE:
		FUTEX_SYSTEM_LOCK;
		
		f = futex_get(args->uaddr, FUTEX_UNLOCKED);
		newf = futex_get(args->uaddr2, FUTEX_UNLOCKED);
		td->td_retval[0] = futex_wake(f, args->val, newf);
		futex_put(f);
		futex_put(newf);
		
		FUTEX_SYSTEM_UNLOCK;
		break;

	case LINUX_FUTEX_FD:
		printf("linux_sys_futex: unimplemented op %d\n", 
		    args->op);
		break;

	case LINUX_FUTEX_WAKE_OP:
#ifdef	__i386__
		FUTEX_SYSTEM_LOCK;
#ifdef DEBUG
		if (ldebug(sys_futex))
			printf("FUTEX_WAKE_OP: %d: uaddr = %p, op = %d, val = %d, uaddr2 = %p, val3 = %d\n",
			    td->td_proc->p_pid, args->uaddr, args->op, args->val,
			    args->uaddr2, args->val3);
#endif
		f = futex_get(args->uaddr, FUTEX_UNLOCKED);
		f2 = futex_get(args->uaddr2, FUTEX_UNLOCKED);

		/*
		 * This function returns positive number as results and
		 * negative as errors
		 */
		op_ret = futex_atomic_op(td, args->val3, args->uaddr2);
		if (op_ret < 0) {

			/* XXX: we dont handle the EFAULT yet */
			if (op_ret != -EFAULT) {
				futex_put(f);
				futex_put(f2);
				FUTEX_SYSTEM_UNLOCK;
				return (-op_ret);
			}

			futex_put(f);
			futex_put(f2);

			FUTEX_SYSTEM_UNLOCK;
			return (EFAULT);

		}

		ret = futex_wake(f, args->val, NULL);
		futex_put(f);
		if (op_ret > 0) {
		   	op_ret = 0;
		   	/*
			 * Linux uses the address of the timespec parameter
			 * as the number of retries, so any large number will
			 * be ok.
			 */
   		   	op_ret += futex_wake(f2, 0x7fffffff, NULL);
			ret += op_ret;
		}
		futex_put(f2);
		td->td_retval[0] = ret;

		FUTEX_SYSTEM_UNLOCK;
#else
		printf("linux_sys_futex: wake_op not implemented");
#endif
	   	break;

	default:
		printf("linux_sys_futex: unknown op %d\n", 
		    args->op);
		break;
	}
	return 0;
}

static struct futex *
futex_get(void *uaddr, int locked)
{
	struct futex *f;

	if (locked == FUTEX_UNLOCKED)
		FUTEX_LOCK;
	LIST_FOREACH(f, &futex_list, f_list) {
		if (f->f_uaddr == uaddr) {
			f->f_refcount++;
			if (locked == FUTEX_UNLOCKED)
				FUTEX_UNLOCK;
			return f;
		}
	}

	f = malloc(sizeof(*f), M_LINUX, M_WAITOK);
	f->f_uaddr = uaddr;
	f->f_refcount = 1;
	TAILQ_INIT(&f->f_waiting_proc);
	LIST_INSERT_HEAD(&futex_list, f, f_list);
	if (locked == FUTEX_UNLOCKED)
		FUTEX_UNLOCK;

	return f;
}

static void
futex_put(f)
	struct futex *f;
{
	FUTEX_LOCK;
	f->f_refcount--;
	if (f->f_refcount == 0) {
		LIST_REMOVE(f, f_list);
		free(f, M_LINUX);
	}
	FUTEX_UNLOCK;

	return;
}

static int
futex_sleep(struct futex *f, struct thread *td, unsigned long timeout)
{
	struct waiting_proc *wp;
	int ret;

	wp = malloc(sizeof(*wp), M_LINUX, M_WAITOK);
	wp->wp_t = td;
	wp->wp_new_futex = NULL;
	FUTEX_LOCK;
	TAILQ_INSERT_TAIL(&f->f_waiting_proc, wp, wp_list);
	FUTEX_UNLOCK;

#ifdef DEBUG
	if (ldebug(sys_futex))
		printf("FUTEX --> %d tlseep timeout = %ld\n", td->td_proc->p_pid,
		    timeout);
#endif
	ret = tsleep(wp, PCATCH | PZERO, "linuxfutex", timeout);

	FUTEX_LOCK;
	TAILQ_REMOVE(&f->f_waiting_proc, wp, wp_list);
	FUTEX_UNLOCK;

	if ((ret == 0) && (wp->wp_new_futex != NULL)) {
		ret = futex_sleep(wp->wp_new_futex, td, timeout);
		futex_put(wp->wp_new_futex);	/* futex_get called in wakeup */
	}

	free(wp, M_LINUX);

	return ret;
}

static int
futex_wake(struct futex *f, int n, struct futex *newf)
{
	struct waiting_proc *wp;
	int count = 0;

	FUTEX_LOCK;
	TAILQ_FOREACH(wp, &f->f_waiting_proc, wp_list) {
		if (count <= n) {
			wakeup_one(wp);
			count++;
		} else {
			if (newf != NULL) {
				/* futex_put called after tsleep */
				wp->wp_new_futex = futex_get(newf->f_uaddr, FUTEX_LOCKED);
				wakeup_one(wp);
			}
		}
	}
	FUTEX_UNLOCK;

	return count;
}

#ifdef __i386__
static int
futex_atomic_op(struct thread *td, int encoded_op, caddr_t uaddr)
{
   	int op = (encoded_op >> 28) & 7;
        int cmp = (encoded_op >> 24) & 15;
        int oparg = (encoded_op << 8) >> 20;
        int cmparg = (encoded_op << 20) >> 20;
        int oldval = 0, ret;

	if (encoded_op & (FUTEX_OP_OPARG_SHIFT << 28))
	   	oparg = 1 << oparg;

#ifdef DEBUG	
	printf("futex_atomic_op: op = %d, cmp = %d, oparg = %d, cmparg = %d, uaddr = %p\n",
	      op, cmp, oparg, cmparg, uaddr);
#endif
	/* XXX: linux verifies access here and returns EFAULT */

	critical_enter();

	switch (op) {
	   	case FUTEX_OP_SET:
		   	ret = futex_xchgl(oparg, uaddr, &oldval);
			break;
		case FUTEX_OP_ADD:
			ret = futex_addl(oparg, uaddr, &oldval);
			break;
		case FUTEX_OP_OR:
			ret = futex_orl(oparg, uaddr, &oldval);
			break;
		case FUTEX_OP_ANDN:
			ret = futex_andnl(oparg, uaddr, &oldval);
			break;
		case FUTEX_OP_XOR:
			ret = futex_xorl(oparg, uaddr, &oldval);
			break;
		default:
			ret = -ENOSYS;
	}

	critical_exit();

	if (!ret)
	   	switch (cmp) {
		      case FUTEX_OP_CMP_EQ: 
			 	ret = (oldval == cmparg); 
				break;
		      case FUTEX_OP_CMP_NE: 
				ret = (oldval != cmparg); 
				break;
		      case FUTEX_OP_CMP_LT: 
				ret = (oldval < cmparg); 
				break;
		      case FUTEX_OP_CMP_GE: 
				ret = (oldval >= cmparg); 
				break;
		      case FUTEX_OP_CMP_LE: 
				ret = (oldval <= cmparg); 
				break;
		      case FUTEX_OP_CMP_GT: 
				ret = (oldval > cmparg); 
				break;
		      default: ret = -ENOSYS;
		}

	return (ret);
}
#endif
