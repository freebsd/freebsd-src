/*
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

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>

/*
 * Makes sure that the right segment of userspace is mapped in.
 */
static __inline void
set_user_sr(register_t vsid)
{

	isync();
	__asm __volatile ("mtsr %0,%1" :: "n"(USER_SR), "r"(vsid));
	isync();
}

int
copyout(const void *kaddr, void *udaddr, size_t len)
{
	struct		thread *td;
	pmap_t		pm;
	const char	*kp;
	char		*up, *p;
	size_t		l;

	td = PCPU_GET(curthread);
	pm = &td->td_proc->p_vmspace->vm_pmap;

	kp = kaddr;
	up = udaddr;

	while (len > 0) {
		p = (char *)USER_ADDR + ((u_int)up & ~SEGMENT_MASK);

		l = ((char *)USER_ADDR + SEGMENT_LENGTH) - p;
		if (l > len)
			l = len;

		set_user_sr(pm->pm_sr[(u_int)up >> ADDR_SR_SHFT]);

		bcopy(kp, p, l);

		up += l;
		kp += l;
		len -= l;
	}

	return (0);
}

int
copyin(const void *udaddr, void *kaddr, size_t len)
{
	struct		thread *td;
	pmap_t		pm;
	const char	*up;
	char		*kp, *p;
	size_t		l;

	td = PCPU_GET(curthread);
	pm = &td->td_proc->p_vmspace->vm_pmap;

	kp = kaddr;
	up = udaddr;

	while (len > 0) {
		p = (char *)USER_ADDR + ((u_int)up & ~SEGMENT_MASK);

		l = ((char *)USER_ADDR + SEGMENT_LENGTH) - p;
		if (l > len)
			l = len;

		set_user_sr(pm->pm_sr[(u_int)up >> ADDR_SR_SHFT]);

		bcopy(p, kp, l);

		up += l;
		kp += l;
		len -= l;
	}

	return (0);
}

int
copyinstr(const void *udaddr, void *kaddr, size_t len, size_t *done)
{
	struct		thread *td;
	pmap_t		pm;
	const char	*up;
	char		*kp;
	size_t		l;
	int		rv, c;

	td = PCPU_GET(curthread);
	pm = &td->td_proc->p_vmspace->vm_pmap;

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
subyte(void *addr, int byte)
{
	struct	thread *td;
	pmap_t	pm;
	char	*p;

	td = PCPU_GET(curthread);
	pm = &td->td_proc->p_vmspace->vm_pmap;
	p = (char *)((u_int)USER_ADDR + ((u_int)addr & ~SEGMENT_MASK));

	set_user_sr(pm->pm_sr[(u_int)addr >> ADDR_SR_SHFT]);

	*p = (char)byte;

	return (0);
}

int
suibyte(void *addr, int byte)
{

	return (subyte(addr, byte));
}

int
suword(void *addr, long word)
{
	struct	thread *td;
	pmap_t	pm;
	long	*p;

	td = PCPU_GET(curthread);
	pm = &td->td_proc->p_vmspace->vm_pmap;
	p = (long *)((u_int)USER_ADDR + ((u_int)addr & ~SEGMENT_MASK));

	set_user_sr(pm->pm_sr[(u_int)addr >> ADDR_SR_SHFT]);

	*p = word;

	return (0);
}

int
fubyte(const void *addr)
{
	struct	thread *td;
	pmap_t	pm;
	char	*p;

	td = PCPU_GET(curthread);
	pm = &td->td_proc->p_vmspace->vm_pmap;
	p = (char *)((u_int)USER_ADDR + ((u_int)addr & ~SEGMENT_MASK));

	set_user_sr(pm->pm_sr[(u_int)addr >> ADDR_SR_SHFT]);

	return ((int)*p);
}

long
fuword(const void *addr)
{
	struct	thread *td;
	pmap_t	pm;
	long	*p;

	td = PCPU_GET(curthread);
	pm = &td->td_proc->p_vmspace->vm_pmap;
	p = (long *)((u_int)USER_ADDR + ((u_int)addr & ~SEGMENT_MASK));

	set_user_sr(pm->pm_sr[(u_int)addr >> ADDR_SR_SHFT]);

	return (*p);
}
