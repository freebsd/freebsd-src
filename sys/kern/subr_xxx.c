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
 * $Id: subr_xxx.c,v 1.8 1997/02/22 09:39:18 peter Exp $
 */

/*
 * Miscellaneous trivial functions.
 */
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
 * Return error for an inval operation
 * on a specific object or file type.
 */
int
einval()
{

	return (EINVAL);
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
noopen(dev, flags, fmt, p)
	dev_t dev;
	int flags;
	int fmt;
	struct proc *p;
{

	return (ENODEV);
}

int
noclose(dev, flags, fmt, p)
	dev_t dev;
	int flags;
	int fmt;
	struct proc *p;
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
noioctl(dev, cmd, data, flags, p)
	dev_t dev;
	int cmd;
	caddr_t data;
	int flags;
	struct proc *p;
{

	return (ENODEV);
}

void
nostop(tp, rw)
	struct tty *tp;
	int rw;
{

}

int
noreset(dev)
	dev_t dev;
{

	printf("noreset(0x%x) called\n", dev);
	return (ENODEV);
}

struct tty *
nodevtotty(dev)
	dev_t dev;
{

	return (NULL);
}

int
nommap(dev, offset, nprot)
	dev_t dev;
	int offset;
	int nprot;
{

	/* Don't return ENODEV.  That would allow mapping address ENODEV! */
	return (-1);
}

int
nodump(dev)
	dev_t dev;
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
nullopen(dev, flags, fmt, p)
	dev_t dev;
	int flags;
	int fmt;
	struct proc *p;
{

	return (0);
}

int
nullclose(dev, flags, fmt, p)
	dev_t dev;
	int flags;
	int fmt;
	struct proc *p;
{

	return (0);
}

/*
 * Unconfigured devswitch functions (for unconfigured drivers).
 * XXX may belong elsewhere.
 */

int
nxopen(dev, flags, fmt, p)
	dev_t dev;
	int flags;
	int fmt;
	struct proc *p;
{

	return (ENXIO);
}

/*
 * XXX all nx functions except nxopen() should probably go away.  They
 * probably can't be called for non-open devices.
 */

int
nxclose(dev, flags, fmt, p)
	dev_t dev;
	int flags;
	int fmt;
	struct proc *p;
{

	printf("nxclose(0x%x) called\n", dev);
	return (ENXIO);
}

int
nxread(dev, uio, ioflag)
	dev_t dev;
	struct uio *uio;
	int ioflag;
{

	printf("nxread(0x%x) called\n", dev);
	return (ENXIO);
}

int
nxwrite(dev, uio, ioflag)
	dev_t dev;
	struct uio *uio;
	int ioflag;
{

	printf("nxwrite(0x%x) called\n", dev);
	return (ENXIO);
}

int
nxioctl(dev, cmd, data, flags, p)
	dev_t dev;
	int cmd;
	caddr_t data;
	int flags;
	struct proc *p;
{

	printf("nxioctl(0x%x) called\n", dev);
	return (ENXIO);
}

int
nxdump(dev)
	dev_t dev;
{

	printf("nxdump(0x%x) called\n", dev);
	return (ENXIO);
}
