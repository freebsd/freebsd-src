/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2009 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*      Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T */
/*        All Rights Reserved   */

/*
 * University Copyright- Copyright (c) 1982, 1986, 1988
 * The Regents of the University of California
 * All Rights Reserved
 *
 * University Acknowledgment- Portions of this document are derived from
 * software developed by the University of California, Berkeley, and its
 * contributors.
 */

/*
 * $FreeBSD$
 */

#include <sys/types.h>
#include <sys/uio.h>

/*
 * same as uiomove() but doesn't modify uio structure.
 * return in cbytes how many bytes were copied.
 */
int
uiocopy(void *p, size_t n, enum uio_rw rw, struct uio *uio, size_t *cbytes)
{
	struct iovec *iov;
	ulong_t cnt;
	int error, iovcnt;

	iovcnt = uio->uio_iovcnt;
	*cbytes = 0;

	for (iov = uio->uio_iov; n > 0 && iovcnt > 0; iov++, iovcnt--) {
		cnt = MIN(iov->iov_len, n);
		if (cnt == 0)
			continue;

		switch (uio->uio_segflg) {
		case UIO_USERSPACE:
			if (rw == UIO_READ)
				error = copyout(p, iov->iov_base, cnt);
			else
				error = copyin(iov->iov_base, p, cnt);
			if (error)
				return (error);
			break;
		case UIO_SYSSPACE:
			if (uio->uio_rw == UIO_READ)
				bcopy(p, iov->iov_base, cnt);
			else
				bcopy(iov->iov_base, p, cnt);
			break;
		}

		p = (caddr_t)p + cnt;
		n -= cnt;
		*cbytes += cnt;
	}
	return (0);
}

/*
 * Drop the next n chars out of *uiop.
 */
void
uioskip(uio_t *uiop, size_t n)
{
	if (n > uiop->uio_resid)
		return;
	while (n != 0) {
		register iovec_t	*iovp = uiop->uio_iov;
		register size_t		niovb = MIN(iovp->iov_len, n);

		if (niovb == 0) {
			uiop->uio_iov++;
			uiop->uio_iovcnt--;
			continue;
		}
		iovp->iov_base += niovb;
		uiop->uio_loffset += niovb;
		iovp->iov_len -= niovb;
		uiop->uio_resid -= niovb;
		n -= niovb;
	}
}
