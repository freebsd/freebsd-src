/*
 * Copyright (c) 1982, 1986, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *
 *	@(#)subr_xxx.c	8.1 (Berkeley) 6/10/93
 */

/*
 * Miscellaneous trivial functions.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>

/*
 * Return error for operation not supported
 * on a specific object or file type.
 */
int
eopnotsupp()
{

	return (EOPNOTSUPP);
}

/*
 * Generic null operation, always returns success.
 */
int
nullop()
{

	return (0);
}

#include <sys/conf.h>

/*
 * Unsupported devswitch functions (e.g. for writing to read-only device).
 * XXX may belong elsewhere.
 */

int
noopen(dev, flags, fmt, td)
	dev_t dev;
	int flags;
	int fmt;
	struct thread *td;
{

	return (ENODEV);
}

int
noclose(dev, flags, fmt, td)
	dev_t dev;
	int flags;
	int fmt;
	struct thread *td;
{

	return (ENODEV);
}

int
noread(dev, uio, ioflag)
	dev_t dev;
	struct uio *uio;
	int ioflag;
{

	return (ENODEV);
}

int
nowrite(dev, uio, ioflag)
	dev_t dev;
	struct uio *uio;
	int ioflag;
{

	return (ENODEV);
}

int
noioctl(dev, cmd, data, flags, td)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flags;
	struct thread *td;
{

	return (ENODEV);
}

int
nokqfilter(dev, kn)
	dev_t dev;
	struct knote *kn;
{

	return (1);
}

int
nommap(dev, offset, paddr, nprot)
	dev_t dev;
	vm_offset_t offset;
	vm_paddr_t *paddr;
	int nprot;
{

	return (ENODEV);
}

int
nodump(void *arg, void *virtual __unused, vm_offset_t physical __unused, off_t offset __unused, size_t length __unused)
{

	return (ENODEV);
}

/*
 * Null devswitch functions (for when the operation always succeeds).
 * XXX may belong elsewhere.
 * XXX not all are here (e.g., seltrue() isn't).
 */

/*
 * XXX this is probably bogus.  Any device that uses it isn't checking the
 * minor number.
 */
int
nullopen(dev, flags, fmt, td)
	dev_t dev;
	int flags;
	int fmt;
	struct thread *td;
{

	return (0);
}

int
nullclose(dev, flags, fmt, td)
	dev_t dev;
	int flags;
	int fmt;
	struct thread *td;
{

	return (0);
}
