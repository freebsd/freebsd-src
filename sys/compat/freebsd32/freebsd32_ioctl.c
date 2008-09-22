/*-
 * Copyright (c) 2008 David E. O'Brien
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
 * 3. Neither the name of the author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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

#include "opt_compat.h"

#include <sys/param.h>
#include <sys/cdio.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/ioccom.h>
#include <sys/mdioctl.h>
#include <sys/proc.h>
#include <sys/syscall.h>
#include <sys/syscallsubr.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/systm.h>

#include <compat/freebsd32/freebsd32.h>
#include <compat/freebsd32/freebsd32_ioctl.h>
#include <compat/freebsd32/freebsd32_proto.h>

/* Cannot get exact size in 64-bit due to alignment issue of entire struct. */
CTASSERT((sizeof(struct md_ioctl32)+4) == 436);


static int
freebsd32_ioctl_md(struct thread *td, struct freebsd32_ioctl_args *uap,
    struct file *fp)
{
	struct md_ioctl mdv;
	struct md_ioctl32 md32;
	u_long com = 0;
	int error;

	if (uap->data == NULL)
		panic("%s: where is my ioctl data??", __func__);
	if (uap->com & IOC_IN) {
		if ((error = copyin(uap->data, &md32, sizeof(md32)))) {
			fdrop(fp, td);
			return (error);
		}
		CP(md32, mdv, md_version);
		CP(md32, mdv, md_unit);
		CP(md32, mdv, md_type);
		PTRIN_CP(md32, mdv, md_file);
		CP(md32, mdv, md_mediasize);
		CP(md32, mdv, md_sectorsize);
		CP(md32, mdv, md_options);
		CP(md32, mdv, md_base);
		CP(md32, mdv, md_fwheads);
		CP(md32, mdv, md_fwsectors);
	} else if (uap->com & IOC_OUT) {
		/*
		 * Zero the buffer so the user always
		 * gets back something deterministic.
		 */
		bzero(&mdv, sizeof mdv);
	}

	switch (uap->com) {
	case MDIOCATTACH_32:
		com = MDIOCATTACH;
		break;
	case MDIOCDETACH_32:
		com = MDIOCDETACH;
		break;
	case MDIOCQUERY_32:
		com = MDIOCQUERY;
		break;
	case MDIOCLIST_32:
		com = MDIOCLIST;
		break;
	default:
		panic("%s: unknown MDIOC %#x", __func__, uap->com);
	}
	error = fo_ioctl(fp, com, (caddr_t)&mdv, td->td_ucred, td);
	if (error == 0 && (com & IOC_OUT)) {
		CP(mdv, md32, md_version);
		CP(mdv, md32, md_unit);
		CP(mdv, md32, md_type);
		PTROUT_CP(mdv, md32, md_file);
		CP(mdv, md32, md_mediasize);
		CP(mdv, md32, md_sectorsize);
		CP(mdv, md32, md_options);
		CP(mdv, md32, md_base);
		CP(mdv, md32, md_fwheads);
		CP(mdv, md32, md_fwsectors);
		error = copyout(&md32, uap->data, sizeof(md32));
	}
	fdrop(fp, td);
	return error;
}


int
freebsd32_ioctl(struct thread *td, struct freebsd32_ioctl_args *uap)
{
	struct ioctl_args ap /*{
		int	fd;
		u_long	com;
		caddr_t	data;
	}*/ ;
	struct file *fp;
	int error;

	if ((error = fget(td, uap->fd, &fp)) != 0)
		return (error);
	if ((fp->f_flag & (FREAD | FWRITE)) == 0) {
		fdrop(fp, td);
		return (EBADF);
	}

	switch (uap->com) {
	case MDIOCATTACH_32:	/* FALLTHROUGH */
	case MDIOCDETACH_32:	/* FALLTHROUGH */
	case MDIOCQUERY_32:	/* FALLTHROUGH */
	case MDIOCLIST_32:
		return freebsd32_ioctl_md(td, uap, fp);

	default:
		fdrop(fp, td);
		ap.fd = uap->fd;
		ap.com = uap->com;
		PTRIN_CP(*uap, ap, data);
		return ioctl(td, &ap);
	}
}
