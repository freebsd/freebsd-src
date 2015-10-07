/*-
 * Copyright (c) 2008 David E. O'Brien
 * Copyright (c) 2015 SRI International
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-10-C-0237
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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
#include <sys/capsicum.h>
#include <sys/cdio.h>
#include <sys/fcntl.h>
#include <sys/filio.h>
#include <sys/file.h>
#include <sys/ioccom.h>
#include <sys/malloc.h>
#include <sys/mdioctl.h>
#include <sys/memrange.h>
#include <sys/pciio.h>
#include <sys/proc.h>
#include <sys/syscall.h>
#include <sys/syscallsubr.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/systm.h>

#include <compat/cheriabi/cheriabi.h>
#include <compat/cheriabi/cheriabi_ioctl.h>
#include <compat/cheriabi/cheriabi_proto.h>

#if 0
/* Cannot get exact size in 64-bit due to alignment issue of entire struct. */
CTASSERT((sizeof(struct md_ioctl32)+4) == 436);
CTASSERT(sizeof(struct ioc_read_toc_entry32) == 8);
CTASSERT(sizeof(struct ioc_toc_header32) == 4);
CTASSERT(sizeof(struct mem_range_op32) == 12);
CTASSERT(sizeof(struct pci_conf_io32) == 36);
CTASSERT(sizeof(struct pci_match_conf32) == 44);
CTASSERT(sizeof(struct pci_conf32) == 44);
#endif


static int
cheriabi_ioctl_md(struct thread *td, struct cheriabi_ioctl_args *uap,
    struct file *fp)
{
	struct md_ioctl mdv;
	struct md_ioctl_c md_c;
	u_long com = 0;
	int i, error;

	if (uap->com & IOC_IN) {
		if ((error = copyincap(uap->data, &md_c, sizeof(md_c)))) {
			return (error);
		}
		CP(md_c, mdv, md_version);
		CP(md_c, mdv, md_unit);
		CP(md_c, mdv, md_type);
		PTRIN_CP(md_c, mdv, md_file);
		CP(md_c, mdv, md_mediasize);
		CP(md_c, mdv, md_sectorsize);
		CP(md_c, mdv, md_options);
		CP(md_c, mdv, md_base);
		CP(md_c, mdv, md_fwheads);
		CP(md_c, mdv, md_fwsectors);
	} else if (uap->com & IOC_OUT) {
		/*
		 * Zero the buffer so the user always
		 * gets back something deterministic.
		 */
		bzero(&mdv, sizeof mdv);
	}

	switch (uap->com) {
	case MDIOCATTACH_C:
		com = MDIOCATTACH;
		break;
	case MDIOCDETACH_C:
		com = MDIOCDETACH;
		break;
	case MDIOCQUERY_C:
		com = MDIOCQUERY;
		break;
	case MDIOCLIST_C:
		com = MDIOCLIST;
		break;
	default:
		panic("%s: unknown MDIOC %lx", __func__, uap->com);
	}
	error = fo_ioctl(fp, com, (caddr_t)&mdv, td->td_ucred, td);
	if (error == 0 && (com & IOC_OUT)) {
		CP(mdv, md_c, md_version);
		CP(mdv, md_c, md_unit);
		CP(mdv, md_c, md_type);
		/*
		 * Don't copy out a new value for md_file.  Either we've
		 * used the one that was copied in or there wasn't one.
		 */
		CP(mdv, md_c, md_mediasize);
		CP(mdv, md_c, md_sectorsize);
		CP(mdv, md_c, md_options);
		CP(mdv, md_c, md_base);
		CP(mdv, md_c, md_fwheads);
		CP(mdv, md_c, md_fwsectors);
		if (com == MDIOCLIST) {
			/* Use MDNPAD, and not MDNPAD_C. */
			for (i = 0; i < MDNPAD; i++)
				CP(mdv, md_c, md_pad[i]);
		}
		error = copyoutcap(&md_c, uap->data, sizeof(md_c));
	}
	return error;
}


static int
cheriabi_ioctl_ioc_read_toc(struct thread *td,
    struct cheriabi_ioctl_args *uap, struct file *fp)
{
	struct ioc_read_toc_entry toce;
	struct ioc_read_toc_entry_c toce_c;
	int error;

	if ((error = copyincap(uap->data, &toce_c, sizeof(toce_c))))
		return (error);
	CP(toce_c, toce, address_format);
	CP(toce_c, toce, starting_track);
	CP(toce_c, toce, data_len);
	PTRIN_CP(toce_c, toce, data);

	if ((error = fo_ioctl(fp, CDIOREADTOCENTRYS, (caddr_t)&toce,
	    td->td_ucred, td))) {
		CP(toce, toce_c, address_format);
		CP(toce, toce_c, starting_track);
		CP(toce, toce_c, data_len);
		/* Don't update data pointer */
		error = copyoutcap(&toce_c, uap->data, sizeof(toce_c));
	}
	return error;
}

static int
cheriabi_ioctl_fiodgname(struct thread *td,
    struct cheriabi_ioctl_args *uap, struct file *fp)
{
	struct fiodgname_arg fgn;
	struct fiodgname_arg_c fgn_c;
	int error;

	if ((error = copyincap(uap->data, &fgn_c, sizeof fgn_c)) != 0)
		return (error);
	CP(fgn_c, fgn, len);
	PTRIN_CP(fgn_c, fgn, buf);
	error = fo_ioctl(fp, FIODGNAME, (caddr_t)&fgn, td->td_ucred, td);
	return (error);
}

static int
cheriabi_ioctl_memrange(struct thread *td,
    struct cheriabi_ioctl_args *uap, struct file *fp)
{
	struct mem_range_op mro;
	struct mem_range_op_c mro_c;
	int error;
	u_long com;

	if ((error = copyincap(uap->data, &mro_c, sizeof(mro_c))) != 0)
		return (error);

	PTRIN_CP(mro_c, mro, mo_desc);
	CP(mro_c, mro, mo_arg[0]);
	CP(mro_c, mro, mo_arg[1]);

	com = 0;
	switch (uap->com) {
	case MEMRANGE_GET_C:
		com = MEMRANGE_GET;
		break;

	case MEMRANGE_SET_C:
		com = MEMRANGE_SET;
		break;

	default:
		panic("%s: unknown MEMRANGE %lx", __func__, uap->com);
	}

	if ((error = fo_ioctl(fp, com, (caddr_t)&mro, td->td_ucred, td)) != 0)
		return (error);

	if ( (com & IOC_OUT) ) {
		CP(mro, mro_c, mo_arg[0]);
		CP(mro, mro_c, mo_arg[1]);

		error = copyoutcap(&mro_c, uap->data, sizeof(mro_c));
	}

	return (error);
}

static int
cheriabi_ioctl_pciocgetconf(struct thread *td,
    struct cheriabi_ioctl_args *uap, struct file *fp)
{
	struct pci_conf_io pci;
	struct pci_conf_io_c pci_c;
	int error;

	if ((error = copyincap(uap->data, &pci_c, sizeof(pci_c))) != 0)
		return (error);

	CP(pci_c, pci, pat_buf_len);
	CP(pci_c, pci, num_patterns);
	PTRIN_CP(pci_c, pci, patterns);
	CP(pci_c, pci, match_buf_len);
	/* num_matches is an output parameter */
	PTRIN_CP(pci_c, pci, matches);
	CP(pci_c, pci, offset);
	CP(pci_c, pci, generation);
	/* status is an output parameter */

	if ((error = fo_ioctl(fp, PCIOCGETCONF, (caddr_t)&pci, td->td_ucred,
	    td)) != 0)
		return (error);

	CP(pci, pci_c, num_matches);
	CP(pci, pci_c, offset);
	CP(pci, pci_c, generation);
	CP(pci, pci_c, status);

	error = copyoutcap(&pci_c, uap->data, sizeof(pci_c));

	return (error);
}

static int
cheriabi_ioctl_sg(struct thread *td,
    struct cheriabi_ioctl_args *uap, struct file *fp)
{
	struct sg_io_hdr io;
	struct sg_io_hdr_c io_c;
	int error;

	if ((error = copyincap(uap->data, &io_c, sizeof(io_c))) != 0)
		return (error);

	CP(io_c, io, interface_id);
	CP(io_c, io, dxfer_direction);
	CP(io_c, io, cmd_len);
	CP(io_c, io, mx_sb_len);
	CP(io_c, io, iovec_count);
	CP(io_c, io, dxfer_len);
	PTRIN_CP(io_c, io, dxferp);
	PTRIN_CP(io_c, io, cmdp);
	PTRIN_CP(io_c, io, sbp);
	CP(io_c, io, timeout);
	CP(io_c, io, flags);
	CP(io_c, io, pack_id);
	PTRIN_CP(io_c, io, usr_ptr);
	CP(io_c, io, status);
	CP(io_c, io, masked_status);
	CP(io_c, io, msg_status);
	CP(io_c, io, sb_len_wr);
	CP(io_c, io, host_status);
	CP(io_c, io, driver_status);
	CP(io_c, io, resid);
	CP(io_c, io, duration);
	CP(io_c, io, info);

	if ((error = fo_ioctl(fp, SG_IO, (caddr_t)&io, td->td_ucred, td)) != 0)
		return (error);

	CP(io, io_c, interface_id);
	CP(io, io_c, dxfer_direction);
	CP(io, io_c, cmd_len);
	CP(io, io_c, mx_sb_len);
	CP(io, io_c, iovec_count);
	CP(io, io_c, dxfer_len);
	/* Don't change dxferp, cmdp, or sbp */
	CP(io, io_c, timeout);
	CP(io, io_c, flags);
	CP(io, io_c, pack_id);
	/* Don't change usr_ptr */
	CP(io, io_c, status);
	CP(io, io_c, masked_status);
	CP(io, io_c, msg_status);
	CP(io, io_c, sb_len_wr);
	CP(io, io_c, host_status);
	CP(io, io_c, driver_status);
	CP(io, io_c, resid);
	CP(io, io_c, duration);
	CP(io, io_c, info);

	error = copyoutcap(&io_c, uap->data, sizeof(io_c));

	return (error);
}

int
cheriabi_ioctl(struct thread *td, struct cheriabi_ioctl_args *uap)
{
	struct file *fp;
	cap_rights_t rights;
	int error;

	error = fget(td, uap->fd, cap_rights_init(&rights, CAP_IOCTL), &fp);
	if (error != 0)
		return (error);
	if ((fp->f_flag & (FREAD | FWRITE)) == 0) {
		fdrop(fp, td);
		return (EBADF);
	}

	switch (uap->com) {
	case MDIOCATTACH_C:	/* FALLTHROUGH */
	case MDIOCDETACH_C:	/* FALLTHROUGH */
	case MDIOCQUERY_C:	/* FALLTHROUGH */
	case MDIOCLIST_C:
		error = cheriabi_ioctl_md(td, uap, fp);
		break;

	case CDIOREADTOCENTRYS_C:
		error = cheriabi_ioctl_ioc_read_toc(td, uap, fp);
		break;

	case FIODGNAME_C:
		error = cheriabi_ioctl_fiodgname(td, uap, fp);
		break;

	case MEMRANGE_GET_C:	/* FALLTHROUGH */
	case MEMRANGE_SET_C:
		error = cheriabi_ioctl_memrange(td, uap, fp);
		break;

	case PCIOCGETCONF_C:
		error = cheriabi_ioctl_pciocgetconf(td, uap, fp);
		break;

	case SG_IO_C:
		error = cheriabi_ioctl_sg(td, uap, fp);
		break;

	default:
		fdrop(fp, td);
		/*
		 * Unlike on freebsd32, uap contains a 64-bit data pointer
		 * already so we can just cast the struct pointer.
		 */
		return sys_ioctl(td, (struct ioctl_args *)uap);
	}

	fdrop(fp, td);
	return error;
}
