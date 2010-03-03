/*-
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
#include <sys/systm.h>
#include <sys/proc.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>

#include <machine/pcb.h>
#include <machine/vmparam.h>


int	setfault(faultbuf);	/* defined in locore.S */

static int
is_uaddr(const void *addr)
{
	int rv = ((vm_offset_t)addr <= VM_MAXUSER_ADDRESS) ? 1 : 0;

	return rv;
}

int
copyout(const void *kaddr, void *udaddr, size_t len)
{
	struct		thread *td;
	faultbuf	env;

	if (!is_uaddr(udaddr))
		return (EFAULT);

	td = PCPU_GET(curthread);

	if (setfault(env)) {
		td->td_pcb->pcb_onfault = NULL;
		return (EFAULT);
	}

	bcopy(kaddr, udaddr, len);

	td->td_pcb->pcb_onfault = NULL;
	return (0);
}

int
copyin(const void *udaddr, void *kaddr, size_t len)
{
	struct		thread *td;
	faultbuf	env;

	if (!is_uaddr(udaddr) || is_uaddr(kaddr))
		return (EFAULT);

	td = PCPU_GET(curthread);

	if (setfault(env)) {
		td->td_pcb->pcb_onfault = NULL;
		return (EFAULT);
	}

	bcopy(udaddr, kaddr, len);

	td->td_pcb->pcb_onfault = NULL;
	return (0);
}

int
copyinstr(const void *udaddr, void *kaddr, size_t len, size_t *done)
{
	struct		thread *td;
	faultbuf	env;
	const char	*up;
	char		*kp;
	size_t		l;
	int		rv, c;

	if (!is_uaddr(udaddr) || is_uaddr(kaddr))
		return (EFAULT);

	td = PCPU_GET(curthread);

	if (setfault(env)) {
		td->td_pcb->pcb_onfault = NULL;
		return (EFAULT);
	}

	kp = kaddr;
	up = udaddr;

	rv = ENAMETOOLONG;

	for (l = 0; len-- > 0; l++) {

		c = *up++;
		
		if (!(*kp++ = c)) {
			l++;
			rv = 0;
			break;
		}
	}

	if (done != NULL) {
		*done = l;
	}

	td->td_pcb->pcb_onfault = NULL;
	return (rv);
}

int
subyte(void *addr, int byte)
{
	struct		thread *td;
	faultbuf	env;

	if (!is_uaddr(addr))
		return (EFAULT);

	td = PCPU_GET(curthread);

	if (setfault(env)) {
		td->td_pcb->pcb_onfault = NULL;
		return (EFAULT);
	}

	*(char *)addr = (char)byte;

	td->td_pcb->pcb_onfault = NULL;
	return (0);
}

int
suword(void *addr, long word)
{
	struct		thread *td;
	faultbuf	env;

	if (!is_uaddr(addr))
		return (EFAULT);

	td = PCPU_GET(curthread);

	if (setfault(env)) {
		td->td_pcb->pcb_onfault = NULL;
		return (EFAULT);
	}

	*(long *)addr = word;

	td->td_pcb->pcb_onfault = NULL;
	return (0);
}

int
suword32(void *addr, int32_t word)
{

	return (suword(addr, (long)word));
}


int
fubyte(const void *addr)
{
	struct		thread *td;
	faultbuf	env;
	int		val;

	if (!is_uaddr(addr))
		return (EFAULT);

	td = PCPU_GET(curthread);

	if (setfault(env)) {
		td->td_pcb->pcb_onfault = NULL;
		return (EFAULT);
	}

	val = *(const u_char *)addr;

	td->td_pcb->pcb_onfault = NULL;
	return (val);
}

long
fuword(const void *addr)
{
	struct		thread *td;
	faultbuf	env;
	long		val;

	if (!is_uaddr(addr))
		return (EFAULT);

	td = PCPU_GET(curthread);

	if (setfault(env)) {
		td->td_pcb->pcb_onfault = NULL;
		return (EFAULT);
	}

	val = *(const long *)addr;

	td->td_pcb->pcb_onfault = NULL;
	return (val);
}

int32_t
fuword32(const void *addr)
{

	return ((int32_t)fuword(addr));
}

uint32_t
casuword32(volatile uint32_t *base, uint32_t oldval, uint32_t newval)
{

	return (casuword((volatile u_long *)base, oldval, newval));
}

u_long
casuword(volatile u_long *addr, u_long old, u_long new)
{
	struct thread *td;
	faultbuf env;
	u_long val;

	if (!((vm_offset_t)addr <= VM_MAXUSER_ADDRESS))
		return (EFAULT);

	td = PCPU_GET(curthread);

	if (setfault(env)) {
		td->td_pcb->pcb_onfault = NULL;
		return (EFAULT);
	}

	__asm __volatile (
		"1:\tlwarx %0, 0, %2\n\t"	/* load old value */
		"cmplw %3, %0\n\t"		/* compare */
		"bne 2f\n\t"			/* exit if not equal */
		"stwcx. %4, 0, %2\n\t"      	/* attempt to store */
		"bne- 1b\n\t"			/* spin if failed */
		"b 3f\n\t"			/* we've succeeded */
		"2:\n\t"
		"stwcx. %0, 0, %2\n\t"       	/* clear reservation (74xx) */
		"3:\n\t"
		: "=&r" (val), "=m" (*addr)
		: "r" (addr), "r" (old), "r" (new), "m" (*addr)
		: "cc", "memory");

	td->td_pcb->pcb_onfault = NULL;

	return (val);
}
