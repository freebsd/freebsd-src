/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
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

#include <sys/param.h>
#include <sys/capsicum.h>
#include <sys/cdio.h>
#include <sys/fcntl.h>
#include <sys/filio.h>
#include <sys/file.h>
#include <sys/ioccom.h>
#include <sys/malloc.h>
#include <sys/memrange.h>
#include <sys/pciio.h>
#include <sys/proc.h>
#include <sys/syscall.h>
#include <sys/syscallsubr.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/systm.h>
#include <sys/uio.h>

#include <compat/freebsd32/freebsd32.h>
#include <compat/freebsd32/freebsd32_ioctl.h>
#include <compat/freebsd32/freebsd32_misc.h>
#include <compat/freebsd32/freebsd32_proto.h>

CTASSERT(sizeof(struct ioc_read_toc_entry32) == 8);
CTASSERT(sizeof(struct mem_range_op32) == 12);
CTASSERT(sizeof(struct pci_conf_io32) == 36);
CTASSERT(sizeof(struct pci_match_conf32) == 44);
CTASSERT(sizeof(struct pci_conf32) == 44);

static int
freebsd32_ioctl_ioc_read_toc(struct thread *td,
    struct freebsd32_ioctl_args *uap, struct file *fp)
{
	struct ioc_read_toc_entry toce;
	struct ioc_read_toc_entry32 toce32;
	int error;

	if ((error = copyin(uap->data, &toce32, sizeof(toce32))))
		return (error);
	CP(toce32, toce, address_format);
	CP(toce32, toce, starting_track);
	CP(toce32, toce, data_len);
	PTRIN_CP(toce32, toce, data);

	if ((error = fo_ioctl(fp, CDIOREADTOCENTRYS, (caddr_t)&toce,
	    td->td_ucred, td))) {
		CP(toce, toce32, address_format);
		CP(toce, toce32, starting_track);
		CP(toce, toce32, data_len);
		PTROUT_CP(toce, toce32, data);
		error = copyout(&toce32, uap->data, sizeof(toce32));
	}
	return error;
}

static int
freebsd32_ioctl_fiodgname(struct thread *td,
    struct freebsd32_ioctl_args *uap, struct file *fp)
{
	struct fiodgname_arg fgn;
	struct fiodgname_arg32 fgn32;
	int error;

	if ((error = copyin(uap->data, &fgn32, sizeof fgn32)) != 0)
		return (error);
	CP(fgn32, fgn, len);
	PTRIN_CP(fgn32, fgn, buf);
	error = fo_ioctl(fp, FIODGNAME, (caddr_t)&fgn, td->td_ucred, td);
	return (error);
}

static int
freebsd32_ioctl_memrange(struct thread *td,
    struct freebsd32_ioctl_args *uap, struct file *fp)
{
	struct mem_range_op mro;
	struct mem_range_op32 mro32;
	int error;
	u_long com;

	if ((error = copyin(uap->data, &mro32, sizeof(mro32))) != 0)
		return (error);

	PTRIN_CP(mro32, mro, mo_desc);
	CP(mro32, mro, mo_arg[0]);
	CP(mro32, mro, mo_arg[1]);

	com = 0;
	switch (uap->com) {
	case MEMRANGE_GET32:
		com = MEMRANGE_GET;
		break;

	case MEMRANGE_SET32:
		com = MEMRANGE_SET;
		break;

	default:
		panic("%s: unknown MEMRANGE %#x", __func__, uap->com);
	}

	if ((error = fo_ioctl(fp, com, (caddr_t)&mro, td->td_ucred, td)) != 0)
		return (error);

	if ( (com & IOC_OUT) ) {
		CP(mro, mro32, mo_arg[0]);
		CP(mro, mro32, mo_arg[1]);

		error = copyout(&mro32, uap->data, sizeof(mro32));
	}

	return (error);
}

static int
freebsd32_ioctl_pciocgetconf(struct thread *td,
    struct freebsd32_ioctl_args *uap, struct file *fp)
{
	struct pci_conf_io pci;
	struct pci_conf_io32 pci32;
	struct pci_match_conf32 pmc32;
	struct pci_match_conf32 *pmc32p;
	struct pci_match_conf pmc;
	struct pci_match_conf *pmcp;
	struct pci_conf32 pc32;
	struct pci_conf32 *pc32p;
	struct pci_conf pc;
	struct pci_conf *pcp;
	u_int32_t i;
	u_int32_t npat_to_convert;
	u_int32_t nmatch_to_convert;
	vm_offset_t addr;
	int error;

	if ((error = copyin(uap->data, &pci32, sizeof(pci32))) != 0)
		return (error);

	CP(pci32, pci, num_patterns);
	CP(pci32, pci, offset);
	CP(pci32, pci, generation);

	npat_to_convert = pci32.pat_buf_len / sizeof(struct pci_match_conf32);
	pci.pat_buf_len = npat_to_convert * sizeof(struct pci_match_conf);
	pci.patterns = NULL;
	nmatch_to_convert = pci32.match_buf_len / sizeof(struct pci_conf32);
	pci.match_buf_len = nmatch_to_convert * sizeof(struct pci_conf);
	pci.matches = NULL;

	if ((error = copyout_map(td, &addr, pci.pat_buf_len)) != 0)
		goto cleanup;
	pci.patterns = (struct pci_match_conf *)addr;
	if ((error = copyout_map(td, &addr, pci.match_buf_len)) != 0)
		goto cleanup;
	pci.matches = (struct pci_conf *)addr;

	npat_to_convert = min(npat_to_convert, pci.num_patterns);

	for (i = 0, pmc32p = (struct pci_match_conf32 *)PTRIN(pci32.patterns),
	     pmcp = pci.patterns;
	     i < npat_to_convert; i++, pmc32p++, pmcp++) {
		if ((error = copyin(pmc32p, &pmc32, sizeof(pmc32))) != 0)
			goto cleanup;
		CP(pmc32,pmc,pc_sel);
		strlcpy(pmc.pd_name, pmc32.pd_name, sizeof(pmc.pd_name));
		CP(pmc32,pmc,pd_unit);
		CP(pmc32,pmc,pc_vendor);
		CP(pmc32,pmc,pc_device);
		CP(pmc32,pmc,pc_class);
		CP(pmc32,pmc,flags);
		if ((error = copyout(&pmc, pmcp, sizeof(pmc))) != 0)
			goto cleanup;
	}

	if ((error = fo_ioctl(fp, PCIOCGETCONF, (caddr_t)&pci,
			      td->td_ucred, td)) != 0)
		goto cleanup;

	nmatch_to_convert = min(nmatch_to_convert, pci.num_matches);

	for (i = 0, pcp = pci.matches,
	     pc32p = (struct pci_conf32 *)PTRIN(pci32.matches);
	     i < nmatch_to_convert; i++, pcp++, pc32p++) {
		if ((error = copyin(pcp, &pc, sizeof(pc))) != 0)
			goto cleanup;
		CP(pc,pc32,pc_sel);
		CP(pc,pc32,pc_hdr);
		CP(pc,pc32,pc_subvendor);
		CP(pc,pc32,pc_subdevice);
		CP(pc,pc32,pc_vendor);
		CP(pc,pc32,pc_device);
		CP(pc,pc32,pc_class);
		CP(pc,pc32,pc_subclass);
		CP(pc,pc32,pc_progif);
		CP(pc,pc32,pc_revid);
		strlcpy(pc32.pd_name, pc.pd_name, sizeof(pc32.pd_name));
		CP(pc,pc32,pd_unit);
		if ((error = copyout(&pc32, pc32p, sizeof(pc32))) != 0)
			goto cleanup;
	}

	CP(pci, pci32, num_matches);
	CP(pci, pci32, offset);
	CP(pci, pci32, generation);
	CP(pci, pci32, status);

	error = copyout(&pci32, uap->data, sizeof(pci32));

cleanup:
	if (pci.patterns)
		copyout_unmap(td, (vm_offset_t)pci.patterns, pci.pat_buf_len);
	if (pci.matches)
		copyout_unmap(td, (vm_offset_t)pci.matches, pci.match_buf_len);

	return (error);
}

static int
freebsd32_ioctl_barmmap(struct thread *td,
    struct freebsd32_ioctl_args *uap, struct file *fp)
{
	struct pci_bar_mmap32 pbm32;
	struct pci_bar_mmap pbm;
	int error;

	error = copyin(uap->data, &pbm32, sizeof(pbm32));
	if (error != 0)
		return (error);
	PTRIN_CP(pbm32, pbm, pbm_map_base);
	CP(pbm32, pbm, pbm_sel);
	CP(pbm32, pbm, pbm_reg);
	CP(pbm32, pbm, pbm_flags);
	CP(pbm32, pbm, pbm_memattr);
	pbm.pbm_bar_length = PAIR32TO64(uint64_t, pbm32.pbm_bar_length);
	error = fo_ioctl(fp, PCIOCBARMMAP, (caddr_t)&pbm, td->td_ucred, td);
	if (error == 0) {
		PTROUT_CP(pbm, pbm32, pbm_map_base);
		CP(pbm, pbm32, pbm_map_length);
#if BYTE_ORDER == LITTLE_ENDIAN
		pbm32.pbm_bar_length1 = pbm.pbm_bar_length;
		pbm32.pbm_bar_length2 = pbm.pbm_bar_length >> 32;
#else
		pbm32.pbm_bar_length1 = pbm.pbm_bar_length >> 32;
		pbm32.pbm_bar_length2 = pbm.pbm_bar_length;
#endif
		CP(pbm, pbm32, pbm_bar_off);
		error = copyout(&pbm32, uap->data, sizeof(pbm32));
	}
	return (error);
}

static int
freebsd32_ioctl_sg(struct thread *td,
    struct freebsd32_ioctl_args *uap, struct file *fp)
{
	struct sg_io_hdr io;
	struct sg_io_hdr32 io32;
	int error;

	if ((error = copyin(uap->data, &io32, sizeof(io32))) != 0)
		return (error);

	CP(io32, io, interface_id);
	CP(io32, io, dxfer_direction);
	CP(io32, io, cmd_len);
	CP(io32, io, mx_sb_len);
	CP(io32, io, iovec_count);
	CP(io32, io, dxfer_len);
	PTRIN_CP(io32, io, dxferp);
	PTRIN_CP(io32, io, cmdp);
	PTRIN_CP(io32, io, sbp);
	CP(io32, io, timeout);
	CP(io32, io, flags);
	CP(io32, io, pack_id);
	PTRIN_CP(io32, io, usr_ptr);
	CP(io32, io, status);
	CP(io32, io, masked_status);
	CP(io32, io, msg_status);
	CP(io32, io, sb_len_wr);
	CP(io32, io, host_status);
	CP(io32, io, driver_status);
	CP(io32, io, resid);
	CP(io32, io, duration);
	CP(io32, io, info);

	if ((error = fo_ioctl(fp, SG_IO, (caddr_t)&io, td->td_ucred, td)) != 0)
		return (error);

	CP(io, io32, interface_id);
	CP(io, io32, dxfer_direction);
	CP(io, io32, cmd_len);
	CP(io, io32, mx_sb_len);
	CP(io, io32, iovec_count);
	CP(io, io32, dxfer_len);
	PTROUT_CP(io, io32, dxferp);
	PTROUT_CP(io, io32, cmdp);
	PTROUT_CP(io, io32, sbp);
	CP(io, io32, timeout);
	CP(io, io32, flags);
	CP(io, io32, pack_id);
	PTROUT_CP(io, io32, usr_ptr);
	CP(io, io32, status);
	CP(io, io32, masked_status);
	CP(io, io32, msg_status);
	CP(io, io32, sb_len_wr);
	CP(io, io32, host_status);
	CP(io, io32, driver_status);
	CP(io, io32, resid);
	CP(io, io32, duration);
	CP(io, io32, info);

	error = copyout(&io32, uap->data, sizeof(io32));

	return (error);
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
	case CDIOREADTOCENTRYS_32:
		error = freebsd32_ioctl_ioc_read_toc(td, uap, fp);
		break;

	case FIODGNAME_32:
		error = freebsd32_ioctl_fiodgname(td, uap, fp);
		break;

	case MEMRANGE_GET32:	/* FALLTHROUGH */
	case MEMRANGE_SET32:
		error = freebsd32_ioctl_memrange(td, uap, fp);
		break;

	case PCIOCGETCONF_32:
		error = freebsd32_ioctl_pciocgetconf(td, uap, fp);
		break;

	case SG_IO_32:
		error = freebsd32_ioctl_sg(td, uap, fp);
		break;

	case PCIOCBARMMAP_32:
		error = freebsd32_ioctl_barmmap(td, uap, fp);
		break;

	default:
		fdrop(fp, td);
		ap.fd = uap->fd;
		ap.com = uap->com;
		PTRIN_CP(*uap, ap, data);
		return sys_ioctl(td, &ap);
	}

	fdrop(fp, td);
	return (error);
}
