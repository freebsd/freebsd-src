/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2009-2021 Dmitry Chagin <dchagin@FreeBSD.org>
 * Copyright (c) 2008 Roman Divacky
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/imgact.h>
#include <sys/imgact_elf.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/sysent.h>
#include <sys/vnode.h>
#include <sys/umtxvar.h>

#ifdef COMPAT_LINUX32
#include <machine/../linux32/linux.h>
#include <machine/../linux32/linux32_proto.h>
#else
#include <machine/../linux/linux.h>
#include <machine/../linux/linux_proto.h>
#endif
#include <compat/linux/linux_emul.h>
#include <compat/linux/linux_futex.h>
#include <compat/linux/linux_misc.h>
#include <compat/linux/linux_time.h>
#include <compat/linux/linux_util.h>

#define	FUTEX_SHARED	0x8     /* shared futex */
#define	FUTEX_UNOWNED	0

#define	GET_SHARED(a)	(a->flags & FUTEX_SHARED) ? AUTO_SHARE : THREAD_SHARE

static int futex_atomic_op(struct thread *, int, uint32_t *, int *);
static int handle_futex_death(struct thread *td, struct linux_emuldata *,
    uint32_t *, unsigned int, bool);
static int fetch_robust_entry(struct linux_robust_list **,
    struct linux_robust_list **, unsigned int *);

struct linux_futex_args {
	uint32_t	*uaddr;
	int32_t		op;
	uint32_t	flags;
	bool		clockrt;
	uint32_t	val;
	struct timespec	*ts;
	uint32_t	*uaddr2;
	uint32_t	val3;
	bool		val3_compare;
	struct timespec	kts;
};

static inline int futex_key_get(const void *, int, int, struct umtx_key *);
static void linux_umtx_abs_timeout_init(struct umtx_abs_timeout *,
	    struct linux_futex_args *);
static int linux_futex(struct thread *, struct linux_futex_args *);
static int linux_futex_wait(struct thread *, struct linux_futex_args *);
static int linux_futex_wake(struct thread *, struct linux_futex_args *);
static int linux_futex_requeue(struct thread *, struct linux_futex_args *);
static int linux_futex_wakeop(struct thread *, struct linux_futex_args *);
static int linux_futex_lock_pi(struct thread *, bool, struct linux_futex_args *);
static int linux_futex_unlock_pi(struct thread *, bool,
	    struct linux_futex_args *);
static int futex_wake_pi(struct thread *, uint32_t *, bool);

static int
futex_key_get(const void *uaddr, int type, int share, struct umtx_key *key)
{

	/* Check that futex address is a 32bit aligned. */
	if (!__is_aligned(uaddr, sizeof(uint32_t)))
		return (EINVAL);
	return (umtx_key_get(uaddr, type, share, key));
}

int
futex_wake(struct thread *td, uint32_t *uaddr, int val, bool shared)
{
	struct linux_futex_args args;

	bzero(&args, sizeof(args));
	args.op = LINUX_FUTEX_WAKE;
	args.uaddr = uaddr;
	args.flags = shared == true ? FUTEX_SHARED : 0;
	args.val = val;
	args.val3 = FUTEX_BITSET_MATCH_ANY;

	return (linux_futex_wake(td, &args));
}

static int
futex_wake_pi(struct thread *td, uint32_t *uaddr, bool shared)
{
	struct linux_futex_args args;

	bzero(&args, sizeof(args));
	args.op = LINUX_FUTEX_UNLOCK_PI;
	args.uaddr = uaddr;
	args.flags = shared == true ? FUTEX_SHARED : 0;

	return (linux_futex_unlock_pi(td, true, &args));
}

static int
futex_atomic_op(struct thread *td, int encoded_op, uint32_t *uaddr,
    int *res)
{
	int op = (encoded_op >> 28) & 7;
	int cmp = (encoded_op >> 24) & 15;
	int oparg = (encoded_op << 8) >> 20;
	int cmparg = (encoded_op << 20) >> 20;
	int oldval = 0, ret;

	if (encoded_op & (FUTEX_OP_OPARG_SHIFT << 28))
		oparg = 1 << oparg;

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
		ret = futex_andl(~oparg, uaddr, &oldval);
		break;
	case FUTEX_OP_XOR:
		ret = futex_xorl(oparg, uaddr, &oldval);
		break;
	default:
		ret = ENOSYS;
		break;
	}

	if (ret != 0)
		return (ret);

	switch (cmp) {
	case FUTEX_OP_CMP_EQ:
		*res = (oldval == cmparg);
		break;
	case FUTEX_OP_CMP_NE:
		*res = (oldval != cmparg);
		break;
	case FUTEX_OP_CMP_LT:
		*res = (oldval < cmparg);
		break;
	case FUTEX_OP_CMP_GE:
		*res = (oldval >= cmparg);
		break;
	case FUTEX_OP_CMP_LE:
		*res = (oldval <= cmparg);
		break;
	case FUTEX_OP_CMP_GT:
		*res = (oldval > cmparg);
		break;
	default:
		ret = ENOSYS;
	}

	return (ret);
}

static int
linux_futex(struct thread *td, struct linux_futex_args *args)
{
	struct linux_pemuldata *pem;
	struct proc *p;

	if (args->op & LINUX_FUTEX_PRIVATE_FLAG) {
		args->flags = 0;
		args->op &= ~LINUX_FUTEX_PRIVATE_FLAG;
	} else
		args->flags = FUTEX_SHARED;

	args->clockrt = args->op & LINUX_FUTEX_CLOCK_REALTIME;
	args->op = args->op & ~LINUX_FUTEX_CLOCK_REALTIME;

	if (args->clockrt &&
	    args->op != LINUX_FUTEX_WAIT_BITSET &&
	    args->op != LINUX_FUTEX_WAIT_REQUEUE_PI &&
	    args->op != LINUX_FUTEX_LOCK_PI2)
		return (ENOSYS);

	switch (args->op) {
	case LINUX_FUTEX_WAIT:
		args->val3 = FUTEX_BITSET_MATCH_ANY;
		/* FALLTHROUGH */

	case LINUX_FUTEX_WAIT_BITSET:
		LINUX_CTR3(sys_futex, "WAIT uaddr %p val 0x%x bitset 0x%x",
		    args->uaddr, args->val, args->val3);

		return (linux_futex_wait(td, args));

	case LINUX_FUTEX_WAKE:
		args->val3 = FUTEX_BITSET_MATCH_ANY;
		/* FALLTHROUGH */

	case LINUX_FUTEX_WAKE_BITSET:
		LINUX_CTR3(sys_futex, "WAKE uaddr %p nrwake 0x%x bitset 0x%x",
		    args->uaddr, args->val, args->val3);

		return (linux_futex_wake(td, args));

	case LINUX_FUTEX_REQUEUE:
		/*
		 * Glibc does not use this operation since version 2.3.3,
		 * as it is racy and replaced by FUTEX_CMP_REQUEUE operation.
		 * Glibc versions prior to 2.3.3 fall back to FUTEX_WAKE when
		 * FUTEX_REQUEUE returned EINVAL.
		 */
		pem = pem_find(td->td_proc);
		if ((pem->flags & LINUX_XDEPR_REQUEUEOP) == 0) {
			linux_msg(td, "unsupported FUTEX_REQUEUE");
			pem->flags |= LINUX_XDEPR_REQUEUEOP;
		}

		/*
		 * The above is true, however musl libc does make use of the
		 * futex requeue operation, allow operation for brands which
		 * set LINUX_BI_FUTEX_REQUEUE bit of Brandinfo flags.
		 */
		p = td->td_proc;
		Elf_Brandinfo *bi = p->p_elf_brandinfo;
		if (bi == NULL || ((bi->flags & LINUX_BI_FUTEX_REQUEUE)) == 0)
			return (EINVAL);
		args->val3_compare = false;
		/* FALLTHROUGH */

	case LINUX_FUTEX_CMP_REQUEUE:
		LINUX_CTR5(sys_futex, "CMP_REQUEUE uaddr %p "
		    "nrwake 0x%x uval 0x%x uaddr2 %p nrequeue 0x%x",
		    args->uaddr, args->val, args->val3, args->uaddr2,
		    args->ts);

		return (linux_futex_requeue(td, args));

	case LINUX_FUTEX_WAKE_OP:
		LINUX_CTR5(sys_futex, "WAKE_OP "
		    "uaddr %p nrwake 0x%x uaddr2 %p op 0x%x nrwake2 0x%x",
		    args->uaddr, args->val, args->uaddr2, args->val3,
		    args->ts);

		return (linux_futex_wakeop(td, args));

	case LINUX_FUTEX_LOCK_PI:
		args->clockrt = true;
		/* FALLTHROUGH */

	case LINUX_FUTEX_LOCK_PI2:
		LINUX_CTR2(sys_futex, "LOCKPI uaddr %p val 0x%x",
		    args->uaddr, args->val);

		return (linux_futex_lock_pi(td, false, args));

	case LINUX_FUTEX_UNLOCK_PI:
		LINUX_CTR1(sys_futex, "UNLOCKPI uaddr %p",
		    args->uaddr);

		return (linux_futex_unlock_pi(td, false, args));

	case LINUX_FUTEX_TRYLOCK_PI:
		LINUX_CTR1(sys_futex, "TRYLOCKPI uaddr %p",
		    args->uaddr);

		return (linux_futex_lock_pi(td, true, args));

	/*
	 * Current implementation of FUTEX_WAIT_REQUEUE_PI and FUTEX_CMP_REQUEUE_PI
	 * can't be used anymore to implement conditional variables.
	 * A detailed explanation can be found here:
	 *
	 * https://sourceware.org/bugzilla/show_bug.cgi?id=13165
	 * and here http://austingroupbugs.net/view.php?id=609
	 *
	 * And since commit
	 * https://sourceware.org/git/gitweb.cgi?p=glibc.git;h=ed19993b5b0d05d62cc883571519a67dae481a14
	 * glibc does not use them.
	 */
	case LINUX_FUTEX_WAIT_REQUEUE_PI:
		/* not yet implemented */
		pem = pem_find(td->td_proc);
		if ((pem->flags & LINUX_XUNSUP_FUTEXPIOP) == 0) {
			linux_msg(td, "unsupported FUTEX_WAIT_REQUEUE_PI");
			pem->flags |= LINUX_XUNSUP_FUTEXPIOP;
		}
		return (ENOSYS);

	case LINUX_FUTEX_CMP_REQUEUE_PI:
		/* not yet implemented */
		pem = pem_find(td->td_proc);
		if ((pem->flags & LINUX_XUNSUP_FUTEXPIOP) == 0) {
			linux_msg(td, "unsupported FUTEX_CMP_REQUEUE_PI");
			pem->flags |= LINUX_XUNSUP_FUTEXPIOP;
		}
		return (ENOSYS);

	default:
		linux_msg(td, "unsupported futex op %d", args->op);
		return (ENOSYS);
	}
}

/*
 * pi protocol:
 * - 0 futex word value means unlocked.
 * - TID futex word value means locked.
 * Userspace uses atomic ops to lock/unlock these futexes without entering the
 * kernel. If the lock-acquire fastpath fails, (transition from 0 to TID fails),
 * then FUTEX_LOCK_PI is called.
 * The kernel atomically set FUTEX_WAITERS bit in the futex word value, if no
 * other waiters exists looks up the thread that owns the futex (it has put its
 * own TID into the futex value) and made this thread the owner of the internal
 * pi-aware lock object (mutex). Then the kernel tries to lock the internal lock
 * object, on which it blocks. Once it returns, it has the mutex acquired, and it
 * sets the futex value to its own TID and returns (futex value contains
 * FUTEX_WAITERS|TID).
 * The unlock fastpath would fail (because the FUTEX_WAITERS bit is set) and
 * FUTEX_UNLOCK_PI will be called.
 * If a futex is found to be held at exit time, the kernel sets the OWNER_DIED
 * bit of the futex word and wakes up the next futex waiter (if any), WAITERS
 * bit is preserved (if any).
 * If OWNER_DIED bit is set the kernel sanity checks the futex word value against
 * the internal futex state and if correct, acquire futex.
 */
static int
linux_futex_lock_pi(struct thread *td, bool try, struct linux_futex_args *args)
{
	struct umtx_abs_timeout timo;
	struct linux_emuldata *em;
	struct umtx_pi *pi, *new_pi;
	struct thread *td1;
	struct umtx_q *uq;
	int error, rv;
	uint32_t owner, old_owner;

	em = em_find(td);
	uq = td->td_umtxq;
	error = futex_key_get(args->uaddr, TYPE_PI_FUTEX, GET_SHARED(args),
	    &uq->uq_key);
	if (error != 0)
		return (error);
	if (args->ts != NULL)
		linux_umtx_abs_timeout_init(&timo, args);

	umtxq_lock(&uq->uq_key);
	pi = umtx_pi_lookup(&uq->uq_key);
	if (pi == NULL) {
		new_pi = umtx_pi_alloc(M_NOWAIT);
		if (new_pi == NULL) {
			umtxq_unlock(&uq->uq_key);
			new_pi = umtx_pi_alloc(M_WAITOK);
			umtxq_lock(&uq->uq_key);
			pi = umtx_pi_lookup(&uq->uq_key);
			if (pi != NULL) {
				umtx_pi_free(new_pi);
				new_pi = NULL;
			}
		}
		if (new_pi != NULL) {
			new_pi->pi_key = uq->uq_key;
			umtx_pi_insert(new_pi);
			pi = new_pi;
		}
	}
	umtx_pi_ref(pi);
	umtxq_unlock(&uq->uq_key);
	for (;;) {
		/* Try uncontested case first. */
		rv = casueword32(args->uaddr, FUTEX_UNOWNED, &owner, em->em_tid);
		/* The acquire succeeded. */
		if (rv == 0) {
			error = 0;
			break;
		}
		if (rv == -1) {
			error = EFAULT;
			break;
		}

		/*
		 * Nobody owns it, but the acquire failed. This can happen
		 * with ll/sc atomic.
		 */
		if (owner == FUTEX_UNOWNED) {
			error = thread_check_susp(td, true);
			if (error != 0)
				break;
			continue;
		}

		/*
		 * Avoid overwriting a possible error from sleep due
		 * to the pending signal with suspension check result.
		 */
		if (error == 0) {
			error = thread_check_susp(td, true);
			if (error != 0)
				break;
		}

		/* The futex word at *uaddr is already locked by the caller. */
		if ((owner & FUTEX_TID_MASK) == em->em_tid) {
			error = EDEADLK;
			break;
		}

		/*
		 * Futex owner died, handle_futex_death() set the OWNER_DIED bit
		 * and clear tid. Try to acquire it.
		 */
		if ((owner & FUTEX_TID_MASK) == FUTEX_UNOWNED) {
			old_owner = owner;
			owner = owner & (FUTEX_WAITERS | FUTEX_OWNER_DIED);
			owner |= em->em_tid;
			rv = casueword32(args->uaddr, old_owner, &owner, owner);
			if (rv == -1) {
				error = EFAULT;
				break;
			}
			if (rv == 1) {
				if (error == 0) {
					error = thread_check_susp(td, true);
					if (error != 0)
						break;
				}

				/*
				 * If this failed the lock could
				 * changed, restart.
				 */
				continue;
			}

			umtxq_lock(&uq->uq_key);
			umtxq_busy(&uq->uq_key);
			error = umtx_pi_claim(pi, td);
			umtxq_unbusy(&uq->uq_key);
			umtxq_unlock(&uq->uq_key);
			if (error != 0) {
				/*
				 * Since we're going to return an
				 * error, restore the futex to its
				 * previous, unowned state to avoid
				 * compounding the problem.
				 */
				(void)casuword32(args->uaddr, owner, old_owner);
			}
			break;
		}

		/*
		 * Inconsistent state: OWNER_DIED is set and tid is not 0.
		 * Linux does some checks of futex state, we return EINVAL,
		 * as the user space can take care of this.
		 */
		if ((owner & FUTEX_OWNER_DIED) != FUTEX_UNOWNED) {
			error = EINVAL;
			break;
		}

		if (try != 0) {
			error = EBUSY;
			break;
		}

		/*
		 * If we caught a signal, we have retried and now
		 * exit immediately.
		 */
		if (error != 0)
			break;

		umtxq_lock(&uq->uq_key);
		umtxq_busy(&uq->uq_key);
		umtxq_unlock(&uq->uq_key);

		/*
		 * Set the contested bit so that a release in user space knows
		 * to use the system call for unlock. If this fails either some
		 * one else has acquired the lock or it has been released.
		 */
		rv = casueword32(args->uaddr, owner, &owner,
		    owner | FUTEX_WAITERS);
		if (rv == -1) {
			umtxq_unbusy_unlocked(&uq->uq_key);
			error = EFAULT;
			break;
		}
		if (rv == 1) {
			umtxq_unbusy_unlocked(&uq->uq_key);
			error = thread_check_susp(td, true);
			if (error != 0)
				break;

			/*
			 * The lock changed and we need to retry or we
			 * lost a race to the thread unlocking the umtx.
			 */
			continue;
		}

		/*
		 * Substitute Linux thread id by native thread id to
		 * avoid refactoring code of umtxq_sleep_pi().
		 */
		td1 = linux_tdfind(td, owner & FUTEX_TID_MASK, -1);
		if (td1 != NULL) {
			owner = td1->td_tid;
			PROC_UNLOCK(td1->td_proc);
		} else {
			umtxq_unbusy_unlocked(&uq->uq_key);
			error = EINVAL;
			break;
		}

		umtxq_lock(&uq->uq_key);

		/* We set the contested bit, sleep. */
		error = umtxq_sleep_pi(uq, pi, owner, "futexp",
		    args->ts == NULL ? NULL : &timo,
		    (args->flags & FUTEX_SHARED) != 0);
		if (error != 0)
			continue;

		error = thread_check_susp(td, false);
		if (error != 0)
			break;
	}

	umtxq_lock(&uq->uq_key);
	umtx_pi_unref(pi);
	umtxq_unlock(&uq->uq_key);
	umtx_key_release(&uq->uq_key);
	return (error);
}

static int
linux_futex_unlock_pi(struct thread *td, bool rb, struct linux_futex_args *args)
{
	struct linux_emuldata *em;
	struct umtx_key key;
	uint32_t old, owner, new_owner;
	int count, error;

	em = em_find(td);

	/*
	 * Make sure we own this mtx.
	 */
	error = fueword32(args->uaddr, &owner);
	if (error == -1)
		return (EFAULT);
	if (!rb && (owner & FUTEX_TID_MASK) != em->em_tid)
		return (EPERM);

	error = futex_key_get(args->uaddr, TYPE_PI_FUTEX, GET_SHARED(args), &key);
	if (error != 0)
		return (error);
	umtxq_lock(&key);
	umtxq_busy(&key);
	error = umtx_pi_drop(td, &key, rb, &count);
	if (error != 0 || rb) {
		umtxq_unbusy(&key);
		umtxq_unlock(&key);
		umtx_key_release(&key);
		return (error);
	}
	umtxq_unlock(&key);

	/*
	 * When unlocking the futex, it must be marked as unowned if
	 * there is zero or one thread only waiting for it.
	 * Otherwise, it must be marked as contested.
	 */
	if (count > 1)
		new_owner = FUTEX_WAITERS;
	else
		new_owner = FUTEX_UNOWNED;

again:
	error = casueword32(args->uaddr, owner, &old, new_owner);
	if (error == 1) {
		error = thread_check_susp(td, false);
		if (error == 0)
			goto again;
	}
	umtxq_unbusy_unlocked(&key);
	umtx_key_release(&key);
	if (error == -1)
		return (EFAULT);
	if (error == 0 && old != owner)
		return (EINVAL);
	return (error);
}

static int
linux_futex_wakeop(struct thread *td, struct linux_futex_args *args)
{
	struct umtx_key key, key2;
	int nrwake, op_ret, ret;
	int error, count;

	if (args->uaddr == args->uaddr2)
		return (EINVAL);

	error = futex_key_get(args->uaddr, TYPE_FUTEX, GET_SHARED(args), &key);
	if (error != 0)
		return (error);
	error = futex_key_get(args->uaddr2, TYPE_FUTEX, GET_SHARED(args), &key2);
	if (error != 0) {
		umtx_key_release(&key);
		return (error);
	}
	umtxq_lock(&key);
	umtxq_busy(&key);
	umtxq_unlock(&key);
	error = futex_atomic_op(td, args->val3, args->uaddr2, &op_ret);
	umtxq_lock(&key);
	umtxq_unbusy(&key);
	if (error != 0)
		goto out;
	ret = umtxq_signal_mask(&key, args->val, args->val3);
	if (op_ret > 0) {
		nrwake = (int)(unsigned long)args->ts;
		umtxq_lock(&key2);
		count = umtxq_count(&key2);
		if (count > 0)
			ret += umtxq_signal_mask(&key2, nrwake, args->val3);
		else
			ret += umtxq_signal_mask(&key, nrwake, args->val3);
		umtxq_unlock(&key2);
	}
	td->td_retval[0] = ret;
out:
	umtxq_unlock(&key);
	umtx_key_release(&key2);
	umtx_key_release(&key);
	return (error);
}

static int
linux_futex_requeue(struct thread *td, struct linux_futex_args *args)
{
	int nrwake, nrrequeue;
	struct umtx_key key, key2;
	int error;
	uint32_t uval;

	/*
	 * Linux allows this, we would not, it is an incorrect
	 * usage of declared ABI, so return EINVAL.
	 */
	if (args->uaddr == args->uaddr2)
		return (EINVAL);

	nrrequeue = (int)(unsigned long)args->ts;
	nrwake = args->val;
	/*
	 * Sanity check to prevent signed integer overflow,
	 * see Linux CVE-2018-6927
	 */
	if (nrwake < 0 || nrrequeue < 0)
		return (EINVAL);

	error = futex_key_get(args->uaddr, TYPE_FUTEX, GET_SHARED(args), &key);
	if (error != 0)
		return (error);
	error = futex_key_get(args->uaddr2, TYPE_FUTEX, GET_SHARED(args), &key2);
	if (error != 0) {
		umtx_key_release(&key);
		return (error);
	}
	umtxq_lock(&key);
	umtxq_busy(&key);
	umtxq_unlock(&key);
	error = fueword32(args->uaddr, &uval);
	if (error != 0)
		error = EFAULT;
	else if (args->val3_compare == true && uval != args->val3)
		error = EWOULDBLOCK;
	umtxq_lock(&key);
	umtxq_unbusy(&key);
	if (error == 0) {
		umtxq_lock(&key2);
		td->td_retval[0] = umtxq_requeue(&key, nrwake, &key2, nrrequeue);
		umtxq_unlock(&key2);
	}
	umtxq_unlock(&key);
	umtx_key_release(&key2);
	umtx_key_release(&key);
	return (error);
}

static int
linux_futex_wake(struct thread *td, struct linux_futex_args *args)
{
	struct umtx_key key;
	int error;

	if (args->val3 == 0)
		return (EINVAL);

	error = futex_key_get(args->uaddr, TYPE_FUTEX, GET_SHARED(args), &key);
	if (error != 0)
		return (error);
	umtxq_lock(&key);
	td->td_retval[0] = umtxq_signal_mask(&key, args->val, args->val3);
	umtxq_unlock(&key);
	umtx_key_release(&key);
	return (0);
}

static int
linux_futex_wait(struct thread *td, struct linux_futex_args *args)
{
	struct umtx_abs_timeout timo;
	struct umtx_q *uq;
	uint32_t uval;
	int error;

	if (args->val3 == 0)
		error = EINVAL;

	uq = td->td_umtxq;
	error = futex_key_get(args->uaddr, TYPE_FUTEX, GET_SHARED(args),
	    &uq->uq_key);
	if (error != 0)
		return (error);
	if (args->ts != NULL)
		linux_umtx_abs_timeout_init(&timo, args);
	umtxq_lock(&uq->uq_key);
	umtxq_busy(&uq->uq_key);
	uq->uq_bitset = args->val3;
	umtxq_insert(uq);
	umtxq_unlock(&uq->uq_key);
	error = fueword32(args->uaddr, &uval);
	if (error != 0)
		error = EFAULT;
	else if (uval != args->val)
		error = EWOULDBLOCK;
	umtxq_lock(&uq->uq_key);
	umtxq_unbusy(&uq->uq_key);
	if (error == 0) {
		error = umtxq_sleep(uq, "futex",
		    args->ts == NULL ? NULL : &timo);
		if ((uq->uq_flags & UQF_UMTXQ) == 0)
			error = 0;
		else
			umtxq_remove(uq);
	} else if ((uq->uq_flags & UQF_UMTXQ) != 0) {
		umtxq_remove(uq);
	}
	umtxq_unlock(&uq->uq_key);
	umtx_key_release(&uq->uq_key);
	return (error);
}

static void
linux_umtx_abs_timeout_init(struct umtx_abs_timeout *timo,
    struct linux_futex_args *args)
{
	int clockid, absolute;

	/*
	 * The FUTEX_CLOCK_REALTIME option bit can be employed only with the
	 * FUTEX_WAIT_BITSET, FUTEX_WAIT_REQUEUE_PI, FUTEX_LOCK_PI2.
	 * For FUTEX_WAIT, timeout is interpreted as a relative value, for other
	 * futex operations timeout is interpreted as an absolute value.
	 * If FUTEX_CLOCK_REALTIME option bit is set, the Linux kernel measures
	 * the timeout against the CLOCK_REALTIME clock, otherwise the kernel
	 * measures the timeout against the CLOCK_MONOTONIC clock.
	 */
	clockid = args->clockrt ? CLOCK_REALTIME : CLOCK_MONOTONIC;
	absolute = args->op == LINUX_FUTEX_WAIT ? false : true;
	umtx_abs_timeout_init(timo, clockid, absolute, args->ts);
}

int
linux_sys_futex(struct thread *td, struct linux_sys_futex_args *args)
{
	struct linux_futex_args fargs = {
		.uaddr = args->uaddr,
		.op = args->op,
		.val = args->val,
		.ts = NULL,
		.uaddr2 = args->uaddr2,
		.val3 = args->val3,
		.val3_compare = true,
	};
	int error;

	switch (args->op & LINUX_FUTEX_CMD_MASK) {
	case LINUX_FUTEX_WAIT:
	case LINUX_FUTEX_WAIT_BITSET:
	case LINUX_FUTEX_LOCK_PI:
	case LINUX_FUTEX_LOCK_PI2:
		if (args->timeout != NULL) {
			error = linux_get_timespec(&fargs.kts, args->timeout);
			if (error != 0)
				return (error);
			fargs.ts = &fargs.kts;
		}
		break;
	default:
		fargs.ts = PTRIN(args->timeout);
	}
	return (linux_futex(td, &fargs));
}

#if defined(__i386__) || (defined(__amd64__) && defined(COMPAT_LINUX32))
int
linux_sys_futex_time64(struct thread *td,
    struct linux_sys_futex_time64_args *args)
{
	struct linux_futex_args fargs = {
		.uaddr = args->uaddr,
		.op = args->op,
		.val = args->val,
		.ts = NULL,
		.uaddr2 = args->uaddr2,
		.val3 = args->val3,
		.val3_compare = true,
	};
	int error;

	switch (args->op & LINUX_FUTEX_CMD_MASK) {
	case LINUX_FUTEX_WAIT:
	case LINUX_FUTEX_WAIT_BITSET:
	case LINUX_FUTEX_LOCK_PI:
	case LINUX_FUTEX_LOCK_PI2:
		if (args->timeout != NULL) {
			error = linux_get_timespec64(&fargs.kts, args->timeout);
			if (error != 0)
				return (error);
			fargs.ts = &fargs.kts;
		}
		break;
	default:
		fargs.ts = PTRIN(args->timeout);
	}
	return (linux_futex(td, &fargs));
}
#endif

int
linux_set_robust_list(struct thread *td, struct linux_set_robust_list_args *args)
{
	struct linux_emuldata *em;

	if (args->len != sizeof(struct linux_robust_list_head))
		return (EINVAL);

	em = em_find(td);
	em->robust_futexes = args->head;

	return (0);
}

int
linux_get_robust_list(struct thread *td, struct linux_get_robust_list_args *args)
{
	struct linux_emuldata *em;
	struct linux_robust_list_head *head;
	l_size_t len;
	struct thread *td2;
	int error;

	if (!args->pid) {
		em = em_find(td);
		KASSERT(em != NULL, ("get_robust_list: emuldata notfound.\n"));
		head = em->robust_futexes;
	} else {
		td2 = linux_tdfind(td, args->pid, -1);
		if (td2 == NULL)
			return (ESRCH);
		if (SV_PROC_ABI(td2->td_proc) != SV_ABI_LINUX) {
			PROC_UNLOCK(td2->td_proc);
			return (EPERM);
		}

		em = em_find(td2);
		KASSERT(em != NULL, ("get_robust_list: emuldata notfound.\n"));
		/* XXX: ptrace? */
		if (priv_check(td, PRIV_CRED_SETUID) ||
		    priv_check(td, PRIV_CRED_SETEUID) ||
		    p_candebug(td, td2->td_proc)) {
			PROC_UNLOCK(td2->td_proc);
			return (EPERM);
		}
		head = em->robust_futexes;

		PROC_UNLOCK(td2->td_proc);
	}

	len = sizeof(struct linux_robust_list_head);
	error = copyout(&len, args->len, sizeof(l_size_t));
	if (error != 0)
		return (EFAULT);

	return (copyout(&head, args->head, sizeof(l_uintptr_t)));
}

static int
handle_futex_death(struct thread *td, struct linux_emuldata *em, uint32_t *uaddr,
    unsigned int pi, bool pending_op)
{
	uint32_t uval, nval, mval;
	int error;

retry:
	error = fueword32(uaddr, &uval);
	if (error != 0)
		return (EFAULT);

	/*
	 * Special case for regular (non PI) futexes. The unlock path in
	 * user space has two race scenarios:
	 *
	 * 1. The unlock path releases the user space futex value and
	 *    before it can execute the futex() syscall to wake up
	 *    waiters it is killed.
	 *
	 * 2. A woken up waiter is killed before it can acquire the
	 *    futex in user space.
	 *
	 * In both cases the TID validation below prevents a wakeup of
	 * potential waiters which can cause these waiters to block
	 * forever.
	 *
	 * In both cases it is safe to attempt waking up a potential
	 * waiter without touching the user space futex value and trying
	 * to set the OWNER_DIED bit.
	 */
	if (pending_op && !pi && !uval) {
		(void)futex_wake(td, uaddr, 1, true);
		return (0);
	}

	if ((uval & FUTEX_TID_MASK) == em->em_tid) {
		mval = (uval & FUTEX_WAITERS) | FUTEX_OWNER_DIED;
		error = casueword32(uaddr, uval, &nval, mval);
		if (error == -1)
			return (EFAULT);
		if (error == 1) {
			error = thread_check_susp(td, false);
			if (error != 0)
				return (error);
			goto retry;
		}

		if (!pi && (uval & FUTEX_WAITERS)) {
			error = futex_wake(td, uaddr, 1, true);
			if (error != 0)
				return (error);
		} else if (pi && (uval & FUTEX_WAITERS)) {
			error = futex_wake_pi(td, uaddr, true);
			if (error != 0)
				return (error);
		}
	}

	return (0);
}

static int
fetch_robust_entry(struct linux_robust_list **entry,
    struct linux_robust_list **head, unsigned int *pi)
{
	l_ulong uentry;
	int error;

	error = copyin((const void *)head, &uentry, sizeof(uentry));
	if (error != 0)
		return (EFAULT);

	*entry = (void *)(uentry & ~1UL);
	*pi = uentry & 1;

	return (0);
}

#define	LINUX_HANDLE_DEATH_PENDING	true
#define	LINUX_HANDLE_DEATH_LIST		false

/* This walks the list of robust futexes releasing them. */
void
release_futexes(struct thread *td, struct linux_emuldata *em)
{
	struct linux_robust_list_head *head;
	struct linux_robust_list *entry, *next_entry, *pending;
	unsigned int limit = 2048, pi, next_pi, pip;
	uint32_t *uaddr;
	l_long futex_offset;
	int error;

	head = em->robust_futexes;
	if (head == NULL)
		return;

	if (fetch_robust_entry(&entry, PTRIN(&head->list.next), &pi))
		return;

	error = copyin(&head->futex_offset, &futex_offset,
	    sizeof(futex_offset));
	if (error != 0)
		return;

	if (fetch_robust_entry(&pending, PTRIN(&head->pending_list), &pip))
		return;

	while (entry != &head->list) {
		error = fetch_robust_entry(&next_entry, PTRIN(&entry->next),
		    &next_pi);

		/*
		 * A pending lock might already be on the list, so
		 * don't process it twice.
		 */
		if (entry != pending) {
			uaddr = (uint32_t *)((caddr_t)entry + futex_offset);
			if (handle_futex_death(td, em, uaddr, pi,
			    LINUX_HANDLE_DEATH_LIST))
				return;
		}
		if (error != 0)
			return;

		entry = next_entry;
		pi = next_pi;

		if (!--limit)
			break;

		sched_relinquish(curthread);
	}

	if (pending) {
		uaddr = (uint32_t *)((caddr_t)pending + futex_offset);
		(void)handle_futex_death(td, em, uaddr, pip,
		    LINUX_HANDLE_DEATH_PENDING);
	}
}
