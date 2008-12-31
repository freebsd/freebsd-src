/*-
 * Copyright (c) 1990 The Regents of the University of California.
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
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/ia64/ia64/sys_machdep.c,v 1.8.28.1 2008/11/25 02:59:29 kensmith Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/sysent.h>

#include <machine/cpu.h>
#include <machine/sysarch.h>

#ifndef _SYS_SYSPROTO_H_
struct sysarch_args {
	int op;
	char *parms;
};
#endif

int
sysarch(struct thread *td, struct sysarch_args *uap)
{
	struct ia64_iodesc iod;
	int error;

	error = 0;
	switch(uap->op) {
	case IA64_IORD:
		copyin(uap->parms, &iod, sizeof(iod));
		switch (iod.width) {
		case 1:
			iod.val = inb(iod.port);
			break;
		case 2:
			if (iod.port & 1) {
				iod.val = inb(iod.port);
				iod.val |= inb(iod.port + 1) << 8;
			} else
				iod.val = inw(iod.port);
			break;
		case 4:
			if (iod.port & 3) {
				if (iod.port & 1) {
					iod.val = inb(iod.port);
					iod.val |= inw(iod.port + 1) << 8;
					iod.val |= inb(iod.port + 3) << 24;
				} else {
					iod.val = inw(iod.port);
					iod.val |= inw(iod.port + 2) << 16;
				}
			} else
				iod.val = inl(iod.port);
			break;
		default:
			error = EINVAL;
		}
		copyout(&iod, uap->parms, sizeof(iod));
		break;
	case IA64_IOWR:
		copyin(uap->parms, &iod, sizeof(iod));
		switch (iod.width) {
		case 1:
			outb(iod.port, iod.val);
			break;
		case 2:
			if (iod.port & 1) {
				outb(iod.port, iod.val);
				outb(iod.port + 1, iod.val >> 8);
			} else
				outw(iod.port, iod.val);
			break;
		case 4:
			if (iod.port & 3) {
				if (iod.port & 1) {
					outb(iod.port, iod.val);
					outw(iod.port + 1, iod.val >> 8);
					outb(iod.port + 3, iod.val >> 24);
				} else {
					outw(iod.port, iod.val);
					outw(iod.port + 2, iod.val >> 16);
				}
			} else
				outl(iod.port, iod.val);
			break;
		default:
			error = EINVAL;
		}
		break;
	default:
		error = EINVAL;
		break;
	}
	return (error);
}
