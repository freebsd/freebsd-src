/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD AND BSD-4-Clause
 *
 * Copyright (C) 2002 Benno Rice
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY Benno Rice ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/*-
 * Copyright (C) 1993 Wolfgang Solfrank.
 * Copyright (C) 1993 TooLs GmbH.
 * All rights reserved.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/systm.h>
#include <sys/proc.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>
#include <vm/vm_map.h>

#include <machine/mmuvar.h>
#include <machine/pcb.h>
#include <machine/vmparam.h>
#include <machine/ifunc.h>

/*
 * On powerpc64 (AIM only) the copy functions are IFUNCs, selecting the best
 * option based on the PMAP in use.
 *
 * There are two options for copy functions on powerpc64:
 * - 'remap' copies, which remap userspace segments into kernel space for
 *   copying.  This is used by the 'oea64' pmap.
 * - 'direct' copies, which copy directly from userspace.  This does not require
 *   remapping user segments into kernel.  This is used by the 'radix' pmap for
 *   performance.
 *
 * Book-E does not use the C 'remap' functions, opting instead to use the
 * 'direct' copies, directly, avoiding the IFUNC overhead.
 *
 * On 32-bit AIM these functions bypass the IFUNC machinery for performance.
 */
#ifdef __powerpc64__
int subyte_remap(volatile void *addr, int byte);
int subyte_direct(volatile void *addr, int byte);
int copyinstr_remap(const void *udaddr, void *kaddr, size_t len, size_t *done);
int copyinstr_direct(const void *udaddr, void *kaddr, size_t len, size_t *done);
int copyout_remap(const void *kaddr, void *udaddr, size_t len);
int copyout_direct(const void *kaddr, void *udaddr, size_t len);
int copyin_remap(const void *uaddr, void *kaddr, size_t len);
int copyin_direct(const void *uaddr, void *kaddr, size_t len);
int suword32_remap(volatile void *addr, int word);
int suword32_direct(volatile void *addr, int word);
int suword_remap(volatile void *addr, long word);
int suword_direct(volatile void *addr, long word);
int suword64_remap(volatile void *addr, int64_t word);
int suword64_direct(volatile void *addr, int64_t word);
int fubyte_remap(volatile const void *addr);
int fubyte_direct(volatile const void *addr);
int fuword16_remap(volatile const void *addr);
int fuword16_direct(volatile const void *addr);
int fueword32_remap(volatile const void *addr, int32_t *val);
int fueword32_direct(volatile const void *addr, int32_t *val);
int fueword64_remap(volatile const void *addr, int64_t *val);
int fueword64_direct(volatile const void *addr, int64_t *val);
int fueword_remap(volatile const void *addr, long *val);
int fueword_direct(volatile const void *addr, long *val);
int casueword32_remap(volatile uint32_t *addr, uint32_t old, uint32_t *oldvalp,
	uint32_t new);
int casueword32_direct(volatile uint32_t *addr, uint32_t old, uint32_t *oldvalp,
	uint32_t new);
int casueword_remap(volatile u_long *addr, u_long old, u_long *oldvalp,
	u_long new);
int casueword_direct(volatile u_long *addr, u_long old, u_long *oldvalp,
	u_long new);

/*
 * The IFUNC resolver determines the copy based on whether the PMAP
 * implementation includes a pmap_map_user_ptr function.
 */
#define DEFINE_COPY_FUNC(ret, func, args)			\
	DEFINE_IFUNC(, ret, func, args)				\
	{							\
		return (PMAP_RESOLVE_FUNC(map_user_ptr) ?	\
		    func##_remap : func##_direct);		\
	}
DEFINE_COPY_FUNC(int, subyte, (volatile void *, int))
DEFINE_COPY_FUNC(int, copyinstr, (const void *, void *, size_t, size_t *))
DEFINE_COPY_FUNC(int, copyin, (const void *, void *, size_t))
DEFINE_COPY_FUNC(int, copyout, (const void *, void *, size_t))
DEFINE_COPY_FUNC(int, suword, (volatile void *, long))
DEFINE_COPY_FUNC(int, suword32, (volatile void *, int))
DEFINE_COPY_FUNC(int, suword64, (volatile void *, int64_t))
DEFINE_COPY_FUNC(int, fubyte, (volatile const void *))
DEFINE_COPY_FUNC(int, fuword16, (volatile const void *))
DEFINE_COPY_FUNC(int, fueword32, (volatile const void *, int32_t *))
DEFINE_COPY_FUNC(int, fueword64, (volatile const void *, int64_t *))
DEFINE_COPY_FUNC(int, fueword, (volatile const void *, long *))
DEFINE_COPY_FUNC(int, casueword32,
    (volatile uint32_t *, uint32_t, uint32_t *, uint32_t))
DEFINE_COPY_FUNC(int, casueword, (volatile u_long *, u_long, u_long *, u_long))

#define REMAP(x)	x##_remap
#else
#define	REMAP(x)	x
#endif

int
REMAP(copyout)(const void *kaddr, void *udaddr, size_t len)
{
	struct		thread *td;
	pmap_t		pm;
	jmp_buf		env;
	const char	*kp;
	char		*up, *p;
	size_t		l;

	td = curthread;
	pm = &td->td_proc->p_vmspace->vm_pmap;

	td->td_pcb->pcb_onfault = &env;
	if (setjmp(env)) {
		td->td_pcb->pcb_onfault = NULL;
		return (EFAULT);
	}

	kp = kaddr;
	up = udaddr;

	while (len > 0) {
		if (pmap_map_user_ptr(pm, up, (void **)&p, len, &l)) {
			td->td_pcb->pcb_onfault = NULL;
			return (EFAULT);
		}

		bcopy(kp, p, l);

		up += l;
		kp += l;
		len -= l;
	}

	td->td_pcb->pcb_onfault = NULL;
	return (0);
}

int
REMAP(copyin)(const void *udaddr, void *kaddr, size_t len)
{
	struct		thread *td;
	pmap_t		pm;
	jmp_buf		env;
	const char	*up;
	char		*kp, *p;
	size_t		l;

	td = curthread;
	pm = &td->td_proc->p_vmspace->vm_pmap;

	td->td_pcb->pcb_onfault = &env;
	if (setjmp(env)) {
		td->td_pcb->pcb_onfault = NULL;
		return (EFAULT);
	}

	kp = kaddr;
	up = udaddr;

	while (len > 0) {
		if (pmap_map_user_ptr(pm, up, (void **)&p, len, &l)) {
			td->td_pcb->pcb_onfault = NULL;
			return (EFAULT);
		}

		bcopy(p, kp, l);

		up += l;
		kp += l;
		len -= l;
	}

	td->td_pcb->pcb_onfault = NULL;
	return (0);
}

int
REMAP(copyinstr)(const void *udaddr, void *kaddr, size_t len, size_t *done)
{
	const char	*up;
	char		*kp;
	size_t		l;
	int		rv, c;

	kp = kaddr;
	up = udaddr;

	rv = ENAMETOOLONG;

	for (l = 0; len-- > 0; l++) {
		if ((c = fubyte(up++)) < 0) {
			rv = EFAULT;
			break;
		}

		if (!(*kp++ = c)) {
			l++;
			rv = 0;
			break;
		}
	}

	if (done != NULL) {
		*done = l;
	}

	return (rv);
}

int
REMAP(subyte)(volatile void *addr, int byte)
{
	struct		thread *td;
	pmap_t		pm;
	jmp_buf		env;
	char		*p;

	td = curthread;
	pm = &td->td_proc->p_vmspace->vm_pmap;

	td->td_pcb->pcb_onfault = &env;
	if (setjmp(env)) {
		td->td_pcb->pcb_onfault = NULL;
		return (-1);
	}

	if (pmap_map_user_ptr(pm, addr, (void **)&p, sizeof(*p), NULL)) {
		td->td_pcb->pcb_onfault = NULL;
		return (-1);
	}

	*p = (char)byte;

	td->td_pcb->pcb_onfault = NULL;
	return (0);
}

#ifdef __powerpc64__
int
REMAP(suword32)(volatile void *addr, int word)
{
	struct		thread *td;
	pmap_t		pm;
	jmp_buf		env;
	int		*p;

	td = curthread;
	pm = &td->td_proc->p_vmspace->vm_pmap;

	td->td_pcb->pcb_onfault = &env;
	if (setjmp(env)) {
		td->td_pcb->pcb_onfault = NULL;
		return (-1);
	}

	if (pmap_map_user_ptr(pm, addr, (void **)&p, sizeof(*p), NULL)) {
		td->td_pcb->pcb_onfault = NULL;
		return (-1);
	}

	*p = word;

	td->td_pcb->pcb_onfault = NULL;
	return (0);
}
#else
int
REMAP(suword32)(volatile void *addr, int32_t word)
{
REMAP(	return (suword)(addr, (long)word));
}
#endif

int
REMAP(suword)(volatile void *addr, long word)
{
	struct		thread *td;
	pmap_t		pm;
	jmp_buf		env;
	long		*p;

	td = curthread;
	pm = &td->td_proc->p_vmspace->vm_pmap;

	td->td_pcb->pcb_onfault = &env;
	if (setjmp(env)) {
		td->td_pcb->pcb_onfault = NULL;
		return (-1);
	}

	if (pmap_map_user_ptr(pm, addr, (void **)&p, sizeof(*p), NULL)) {
		td->td_pcb->pcb_onfault = NULL;
		return (-1);
	}

	*p = word;

	td->td_pcb->pcb_onfault = NULL;
	return (0);
}

#ifdef __powerpc64__
int
REMAP(suword64)(volatile void *addr, int64_t word)
{
	return (REMAP(suword)(addr, (long)word));
}
#endif

int
REMAP(fubyte)(volatile const void *addr)
{
	struct		thread *td;
	pmap_t		pm;
	jmp_buf		env;
	u_char		*p;
	int		val;

	td = curthread;
	pm = &td->td_proc->p_vmspace->vm_pmap;

	td->td_pcb->pcb_onfault = &env;
	if (setjmp(env)) {
		td->td_pcb->pcb_onfault = NULL;
		return (-1);
	}

	if (pmap_map_user_ptr(pm, addr, (void **)&p, sizeof(*p), NULL)) {
		td->td_pcb->pcb_onfault = NULL;
		return (-1);
	}

	val = *p;

	td->td_pcb->pcb_onfault = NULL;
	return (val);
}

int
REMAP(fuword16)(volatile const void *addr)
{
	struct		thread *td;
	pmap_t		pm;
	jmp_buf		env;
	uint16_t	*p, val;

	td = curthread;
	pm = &td->td_proc->p_vmspace->vm_pmap;

	td->td_pcb->pcb_onfault = &env;
	if (setjmp(env)) {
		td->td_pcb->pcb_onfault = NULL;
		return (-1);
	}

	if (pmap_map_user_ptr(pm, addr, (void **)&p, sizeof(*p), NULL)) {
		td->td_pcb->pcb_onfault = NULL;
		return (-1);
	}

	val = *p;

	td->td_pcb->pcb_onfault = NULL;
	return (val);
}

int
REMAP(fueword32)(volatile const void *addr, int32_t *val)
{
	struct		thread *td;
	pmap_t		pm;
	jmp_buf		env;
	int32_t		*p;

	td = curthread;
	pm = &td->td_proc->p_vmspace->vm_pmap;

	td->td_pcb->pcb_onfault = &env;
	if (setjmp(env)) {
		td->td_pcb->pcb_onfault = NULL;
		return (-1);
	}

	if (pmap_map_user_ptr(pm, addr, (void **)&p, sizeof(*p), NULL)) {
		td->td_pcb->pcb_onfault = NULL;
		return (-1);
	}

	*val = *p;

	td->td_pcb->pcb_onfault = NULL;
	return (0);
}

#ifdef __powerpc64__
int
REMAP(fueword64)(volatile const void *addr, int64_t *val)
{
	struct		thread *td;
	pmap_t		pm;
	jmp_buf		env;
	int64_t		*p;

	td = curthread;
	pm = &td->td_proc->p_vmspace->vm_pmap;

	td->td_pcb->pcb_onfault = &env;
	if (setjmp(env)) {
		td->td_pcb->pcb_onfault = NULL;
		return (-1);
	}

	if (pmap_map_user_ptr(pm, addr, (void **)&p, sizeof(*p), NULL)) {
		td->td_pcb->pcb_onfault = NULL;
		return (-1);
	}

	*val = *p;

	td->td_pcb->pcb_onfault = NULL;
	return (0);
}
#endif

int
REMAP(fueword)(volatile const void *addr, long *val)
{
	struct		thread *td;
	pmap_t		pm;
	jmp_buf		env;
	long		*p;

	td = curthread;
	pm = &td->td_proc->p_vmspace->vm_pmap;

	td->td_pcb->pcb_onfault = &env;
	if (setjmp(env)) {
		td->td_pcb->pcb_onfault = NULL;
		return (-1);
	}

	if (pmap_map_user_ptr(pm, addr, (void **)&p, sizeof(*p), NULL)) {
		td->td_pcb->pcb_onfault = NULL;
		return (-1);
	}

	*val = *p;

	td->td_pcb->pcb_onfault = NULL;
	return (0);
}

int
REMAP(casueword32)(volatile uint32_t *addr, uint32_t old, uint32_t *oldvalp,
    uint32_t new)
{
	struct thread *td;
	pmap_t pm;
	jmp_buf		env;
	uint32_t *p, val;
	int res;

	td = curthread;
	pm = &td->td_proc->p_vmspace->vm_pmap;

	td->td_pcb->pcb_onfault = &env;
	if (setjmp(env)) {
		td->td_pcb->pcb_onfault = NULL;
		return (-1);
	}

	if (pmap_map_user_ptr(pm, (void *)(uintptr_t)addr, (void **)&p,
	    sizeof(*p), NULL)) {
		td->td_pcb->pcb_onfault = NULL;
		return (-1);
	}

	res = 0;
	__asm __volatile (
		"lwarx %0, 0, %3\n\t"		/* load old value */
		"cmplw %4, %0\n\t"		/* compare */
		"bne 1f\n\t"			/* exit if not equal */
		"stwcx. %5, 0, %3\n\t"      	/* attempt to store */
		"bne- 2f\n\t"			/* if failed */
		"b 3f\n\t"			/* we've succeeded */
		"1:\n\t"
		"stwcx. %0, 0, %3\n\t"       	/* clear reservation (74xx) */
		"2:li %2, 1\n\t"
		"3:\n\t"
		: "=&r" (val), "=m" (*p), "+&r" (res)
		: "r" (p), "r" (old), "r" (new), "m" (*p)
		: "cr0", "memory");

	td->td_pcb->pcb_onfault = NULL;

	*oldvalp = val;
	return (res);
}

#ifndef __powerpc64__
int
REMAP(casueword)(volatile u_long *addr, u_long old, u_long *oldvalp, u_long new)
{

	return (casueword32((volatile uint32_t *)addr, old,
	    (uint32_t *)oldvalp, new));
}
#else
int
REMAP(casueword)(volatile u_long *addr, u_long old, u_long *oldvalp, u_long new)
{
	struct thread *td;
	pmap_t pm;
	jmp_buf		env;
	u_long *p, val;
	int res;

	td = curthread;
	pm = &td->td_proc->p_vmspace->vm_pmap;

	td->td_pcb->pcb_onfault = &env;
	if (setjmp(env)) {
		td->td_pcb->pcb_onfault = NULL;
		return (-1);
	}

	if (pmap_map_user_ptr(pm, (void *)(uintptr_t)addr, (void **)&p,
	    sizeof(*p), NULL)) {
		td->td_pcb->pcb_onfault = NULL;
		return (-1);
	}

	res = 0;
	__asm __volatile (
		"ldarx %0, 0, %3\n\t"		/* load old value */
		"cmpld %4, %0\n\t"		/* compare */
		"bne 1f\n\t"			/* exit if not equal */
		"stdcx. %5, 0, %3\n\t"      	/* attempt to store */
		"bne- 2f\n\t"			/* if failed */
		"b 3f\n\t"			/* we've succeeded */
		"1:\n\t"
		"stdcx. %0, 0, %3\n\t"       	/* clear reservation (74xx) */
		"2:li %2, 1\n\t"
		"3:\n\t"
		: "=&r" (val), "=m" (*p), "+&r" (res)
		: "r" (p), "r" (old), "r" (new), "m" (*p)
		: "cr0", "memory");

	td->td_pcb->pcb_onfault = NULL;

	*oldvalp = val;
	return (res);
}
#endif
