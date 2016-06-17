/* mostly borrowed from kernel/signal.c */
#include <linux/config.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/unistd.h>
#include <linux/smp_lock.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/types.h>

#include <asm/uaccess.h>
#include "sys32.h"

struct k_sigaction32 {
	struct sigaction32 sa;
};

typedef unsigned int old_sigset_t32;

static int
put_old_sigset32(old_sigset_t32 *up, old_sigset_t *set)
{
	old_sigset_t32 set32 = *set;
	return put_user(set32, up);
}

static int
get_old_segset32(old_sigset_t32 *up, old_sigset_t *set)
{
	old_sigset_t32 set32;
	int r;

	if ((r = get_user(set32, up)) == 0)
		*set = set32;

	return r;
}

long
sys32_sigpending(old_sigset_t32 *set)
{
	extern long sys_sigpending(old_sigset_t *set);
	old_sigset_t pending;
	int ret;

	KERNEL_SYSCALL(ret, sys_sigpending, &pending);

	/* can't put_user an old_sigset_t -- it is too big */
	if (put_old_sigset32(set, &pending))
		return -EFAULT;

	return ret;
}

int sys32_sigprocmask(int how, old_sigset_t32 *set, 
				 old_sigset_t32 *oset)
{
	extern int sys_sigprocmask(int how, old_sigset_t *set, 
				 old_sigset_t *oset);
	old_sigset_t s;
	int ret;

	if (set && get_old_segset32 (set, &s))
		return -EFAULT;
	KERNEL_SYSCALL(ret, sys_sigprocmask, how, set ? &s : NULL, oset ? &s : NULL);
	if (!ret && oset && put_old_sigset32(oset, &s))
		return -EFAULT;
	return ret;
}

static inline void
sigset_32to64(sigset_t *s64, sigset_t32 *s32)
{
	s64->sig[0] = s32->sig[0] | ((unsigned long)s32->sig[1] << 32);
}

static inline void
sigset_64to32(sigset_t32 *s32, sigset_t *s64)
{
	s32->sig[0] = s64->sig[0] & 0xffffffffUL;
	s32->sig[1] = (s64->sig[0] >> 32) & 0xffffffffUL;
}

static int
put_sigset32(sigset_t32 *up, sigset_t *set, size_t sz)
{
	sigset_t32 s;

	if (sz != sizeof *set) panic("put_sigset32()");
	sigset_64to32(&s, set);

	return copy_to_user(up, &s, sizeof s);
}

static int
get_sigset32(sigset_t32 *up, sigset_t *set, size_t sz)
{
	sigset_t32 s;
	int r;

	if (sz != sizeof *set) panic("put_sigset32()");

	if ((r = copy_from_user(&s, up, sz)) == 0) {
		sigset_32to64(set, &s);
	}

	return r;
}

int sys32_rt_sigprocmask(int how, sigset_t32 *set, sigset_t32 *oset,
				    unsigned int sigsetsize)
{
	extern long sys_rt_sigprocmask(int how,
				    sigset_t *set, sigset_t *oset,
				   size_t sigsetsize);
	sigset_t old_set, new_set;
	int ret;

	if (set && get_sigset32(set, &new_set, sigsetsize))
		return -EFAULT;
	
	KERNEL_SYSCALL(ret, sys_rt_sigprocmask, how, set ? &new_set : NULL,
				 oset ? &old_set : NULL, sigsetsize);

	if (!ret && oset && put_sigset32(oset, &old_set, sigsetsize))
		return -EFAULT;

	return ret;
}


int sys32_rt_sigpending(sigset_t32 *uset, unsigned int sigsetsize)
{
	int ret;
	sigset_t set;
	extern long sys_rt_sigpending(sigset_t *set, size_t sigsetsize);

	KERNEL_SYSCALL(ret, sys_rt_sigpending, &set, sigsetsize);

	if (!ret && put_sigset32(uset, &set, sigsetsize))
		return -EFAULT;

	return ret;
}

long
sys32_rt_sigaction(int sig, const struct sigaction32 *act, struct sigaction32 *oact,
                 size_t sigsetsize)
{
	struct k_sigaction32 new_sa32, old_sa32;
	struct k_sigaction new_sa, old_sa;
	int ret = -EINVAL;

	if (act) {
		if (copy_from_user(&new_sa32.sa, act, sizeof new_sa32.sa))
			return -EFAULT;
		new_sa.sa.sa_handler = (__sighandler_t)(unsigned long)new_sa32.sa.sa_handler;
		new_sa.sa.sa_flags = new_sa32.sa.sa_flags;
		sigset_32to64(&new_sa.sa.sa_mask, &new_sa32.sa.sa_mask);
	}

	ret = do_sigaction(sig, act ? &new_sa : NULL, oact ? &old_sa : NULL);

	if (!ret && oact) {
		sigset_64to32(&old_sa32.sa.sa_mask, &old_sa.sa.sa_mask);
		old_sa32.sa.sa_flags = old_sa.sa.sa_flags;
		old_sa32.sa.sa_handler = (__sighandler_t32)(unsigned long)old_sa.sa.sa_handler;
		if (copy_to_user(oact, &old_sa32.sa, sizeof old_sa32.sa))
			return -EFAULT;
	}
	return ret;
}

typedef struct {
	unsigned int ss_sp;
	int ss_flags;
	__kernel_size_t32 ss_size;
} stack_t32;

int 
do_sigaltstack32 (const stack_t32 *uss32, stack_t32 *uoss32, unsigned long sp)
{
	stack_t32 ss32, oss32;
	stack_t ss, oss;
	stack_t *ssp = NULL, *ossp = NULL;
	int ret;

	if (uss32) {
		if (copy_from_user(&ss32, uss32, sizeof ss32))
			return -EFAULT;

		ss.ss_sp = (void *)ss32.ss_sp;
		ss.ss_flags = ss32.ss_flags;
		ss.ss_size = ss32.ss_size;

		ssp = &ss;
	}

	if (uoss32)
		ossp = &oss;

	KERNEL_SYSCALL(ret, do_sigaltstack, ssp, ossp, sp);

	if (!ret && uoss32) {
		oss32.ss_sp = (unsigned int)(unsigned long)oss.ss_sp;
		oss32.ss_flags = oss.ss_flags;
		oss32.ss_size = oss.ss_size;
		if (copy_to_user(uoss32, &oss32, sizeof *uoss32))
			return -EFAULT;
	}

	return ret;
}
