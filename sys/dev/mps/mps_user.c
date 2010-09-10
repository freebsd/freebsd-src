/*-
 * Copyright (c) 2008 Yahoo!, Inc.
 * All rights reserved.
 * Written by: John Baldwin <jhb@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of any co-contributors
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
 *
 * LSI MPS-Fusion Host Adapter FreeBSD userland interface
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/selinfo.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/bio.h>
#include <sys/malloc.h>
#include <sys/uio.h>
#include <sys/sysctl.h>
#include <sys/ioccom.h>
#include <sys/endian.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <cam/scsi/scsi_all.h>

#include <dev/mps/mpi/mpi2_type.h>
#include <dev/mps/mpi/mpi2.h>
#include <dev/mps/mpi/mpi2_ioc.h>
#include <dev/mps/mpi/mpi2_cnfg.h>
#include <dev/mps/mpsvar.h>
#include <dev/mps/mps_table.h>
#include <dev/mps/mps_ioctl.h>

static d_open_t		mps_open;
static d_close_t	mps_close;
static d_ioctl_t	mps_ioctl;

static struct cdevsw mps_cdevsw = {
	.d_version =	D_VERSION,
	.d_flags =	0,
	.d_open =	mps_open,
	.d_close =	mps_close,
	.d_ioctl =	mps_ioctl,
	.d_name =	"mps",
};

static MALLOC_DEFINE(M_MPSUSER, "mps_user", "Buffers for mps(4) ioctls");

int
mps_attach_user(struct mps_softc *sc)
{
	int unit;

	unit = device_get_unit(sc->mps_dev);
	sc->mps_cdev = make_dev(&mps_cdevsw, unit, UID_ROOT, GID_OPERATOR, 0640,
	    "mps%d", unit);
	if (sc->mps_cdev == NULL) {
		return (ENOMEM);
	}
	sc->mps_cdev->si_drv1 = sc;
	return (0);
}

void
mps_detach_user(struct mps_softc *sc)
{

	/* XXX: do a purge of pending requests? */
	destroy_dev(sc->mps_cdev);

}

static int
mps_open(struct cdev *dev, int flags, int fmt, struct thread *td)
{

	return (0);
}

static int
mps_close(struct cdev *dev, int flags, int fmt, struct thread *td)
{

	return (0);
}

static int
mps_user_read_cfg_header(struct mps_softc *sc,
    struct mps_cfg_page_req *page_req)
{
	MPI2_CONFIG_PAGE_HEADER *hdr;
	struct mps_config_params params;
	int	    error;

	hdr = &params.hdr.Struct;
	params.action = MPI2_CONFIG_ACTION_PAGE_HEADER;
	params.page_address = le32toh(page_req->page_address);
	hdr->PageVersion = 0;
	hdr->PageLength = 0;
	hdr->PageNumber = page_req->header.PageNumber;
	hdr->PageType = page_req->header.PageType;
	params.buffer = NULL;
	params.length = 0;
	params.callback = NULL;

	if ((error = mps_read_config_page(sc, &params)) != 0) {
		/*
		 * Leave the request. Without resetting the chip, it's
		 * still owned by it and we'll just get into trouble
		 * freeing it now. Mark it as abandoned so that if it
		 * shows up later it can be freed.
		 */
		mps_printf(sc, "read_cfg_header timed out\n");
		return (ETIMEDOUT);
	}

	page_req->ioc_status = htole16(params.status);
	if ((page_req->ioc_status & MPI2_IOCSTATUS_MASK) ==
	    MPI2_IOCSTATUS_SUCCESS) {
		bcopy(hdr, &page_req->header, sizeof(page_req->header));
	}

	return (0);
}

static int
mps_user_read_cfg_page(struct mps_softc *sc, struct mps_cfg_page_req *page_req,
    void *buf)
{
	MPI2_CONFIG_PAGE_HEADER *reqhdr, *hdr;
	struct mps_config_params params;
	int	      error;

	reqhdr = buf;
	hdr = &params.hdr.Struct;
	hdr->PageVersion = reqhdr->PageVersion;
	hdr->PageLength = reqhdr->PageLength;
	hdr->PageNumber = reqhdr->PageNumber;
	hdr->PageType = reqhdr->PageType & MPI2_CONFIG_PAGETYPE_MASK;
	params.action = MPI2_CONFIG_ACTION_PAGE_READ_CURRENT;
	params.page_address = le32toh(page_req->page_address);
	params.buffer = buf;
	params.length = le32toh(page_req->len);
	params.callback = NULL;

	if ((error = mps_read_config_page(sc, &params)) != 0) {
		mps_printf(sc, "mps_user_read_cfg_page timed out\n");
		return (ETIMEDOUT);
	}

	page_req->ioc_status = htole16(params.status);
	return (0);
}

static int
mps_user_read_extcfg_header(struct mps_softc *sc,
    struct mps_ext_cfg_page_req *ext_page_req)
{
	MPI2_CONFIG_EXTENDED_PAGE_HEADER *hdr;
	struct mps_config_params params;
	int	    error;

	hdr = &params.hdr.Ext;
	params.action = MPI2_CONFIG_ACTION_PAGE_HEADER;
	hdr->PageVersion = ext_page_req->header.PageVersion;
	hdr->ExtPageLength = 0;
	hdr->PageNumber = ext_page_req->header.PageNumber;
	hdr->ExtPageType = ext_page_req->header.ExtPageType;
	params.page_address = le32toh(ext_page_req->page_address);
	if ((error = mps_read_config_page(sc, &params)) != 0) {
		/*
		 * Leave the request. Without resetting the chip, it's
		 * still owned by it and we'll just get into trouble
		 * freeing it now. Mark it as abandoned so that if it
		 * shows up later it can be freed.
		 */
		mps_printf(sc, "mps_user_read_extcfg_header timed out\n");
		return (ETIMEDOUT);
	}

	ext_page_req->ioc_status = htole16(params.status);
	if ((ext_page_req->ioc_status & MPI2_IOCSTATUS_MASK) ==
	    MPI2_IOCSTATUS_SUCCESS) {
		ext_page_req->header.PageVersion = hdr->PageVersion;
		ext_page_req->header.PageNumber = hdr->PageNumber;
		ext_page_req->header.PageType = hdr->PageType;
		ext_page_req->header.ExtPageLength = hdr->ExtPageLength;
		ext_page_req->header.ExtPageType = hdr->ExtPageType;
	}

	return (0);
}

static int
mps_user_read_extcfg_page(struct mps_softc *sc,
    struct mps_ext_cfg_page_req *ext_page_req, void *buf)
{
	MPI2_CONFIG_EXTENDED_PAGE_HEADER *reqhdr, *hdr;
	struct mps_config_params params;
	int error;

	reqhdr = buf;
	hdr = &params.hdr.Ext;
	params.action = MPI2_CONFIG_ACTION_PAGE_READ_CURRENT;
	params.page_address = le32toh(ext_page_req->page_address);
	hdr->PageVersion = reqhdr->PageVersion;
	hdr->PageNumber = reqhdr->PageNumber;
	hdr->ExtPageType = reqhdr->ExtPageType;
	hdr->ExtPageLength = reqhdr->ExtPageLength;
	params.buffer = buf;
	params.length = le32toh(ext_page_req->len);
	params.callback = NULL;

	if ((error = mps_read_config_page(sc, &params)) != 0) {
		mps_printf(sc, "mps_user_read_extcfg_page timed out\n");
		return (ETIMEDOUT);
	}

	ext_page_req->ioc_status = htole16(params.status);
	return (0);
}

static int
mps_user_write_cfg_page(struct mps_softc *sc,
    struct mps_cfg_page_req *page_req, void *buf)
{
	MPI2_CONFIG_PAGE_HEADER *reqhdr, *hdr;
	struct mps_config_params params;
	u_int	      hdr_attr;
	int	      error;

	reqhdr = buf;
	hdr = &params.hdr.Struct;
	hdr_attr = reqhdr->PageType & MPI2_CONFIG_PAGEATTR_MASK;
	if (hdr_attr != MPI2_CONFIG_PAGEATTR_CHANGEABLE &&
	    hdr_attr != MPI2_CONFIG_PAGEATTR_PERSISTENT) {
		mps_printf(sc, "page type 0x%x not changeable\n",
			reqhdr->PageType & MPI2_CONFIG_PAGETYPE_MASK);
		return (EINVAL);
	}

	/*
	 * There isn't any point in restoring stripped out attributes
	 * if you then mask them going down to issue the request.
	 */

	hdr->PageVersion = reqhdr->PageVersion;
	hdr->PageLength = reqhdr->PageLength;
	hdr->PageNumber = reqhdr->PageNumber;
	hdr->PageType = reqhdr->PageType;
	params.action = MPI2_CONFIG_ACTION_PAGE_WRITE_CURRENT;
	params.page_address = le32toh(page_req->page_address);
	params.buffer = buf;
	params.length = le32toh(page_req->len);
	params.callback = NULL;

	if ((error = mps_write_config_page(sc, &params)) != 0) {
		mps_printf(sc, "mps_write_cfg_page timed out\n");
		return (ETIMEDOUT);
	}

	page_req->ioc_status = htole16(params.status);
	return (0);
}

struct mps_user_func {
	U8 Func;
	U8 SgOff;
} mps_user_func_list[] = {
	{ MPI2_FUNCTION_IOC_FACTS,	0 },
	{ MPI2_FUNCTION_PORT_FACTS,	0 },
	{ MPI2_FUNCTION_FW_DOWNLOAD, 	offsetof(Mpi2FWDownloadRequest,SGL)},
	{ MPI2_FUNCTION_FW_UPLOAD,	offsetof(Mpi2FWUploadRequest_t,SGL)},
	{ MPI2_FUNCTION_SATA_PASSTHROUGH,offsetof(Mpi2SataPassthroughRequest_t,SGL)},
	{ MPI2_FUNCTION_SMP_PASSTHROUGH, offsetof(Mpi2SmpPassthroughRequest_t,SGL)},
	{ MPI2_FUNCTION_CONFIG,		offsetof(Mpi2ConfigRequest_t,PageBufferSGE)},
	{ MPI2_FUNCTION_SAS_IO_UNIT_CONTROL,	0 },
};	

static int
mps_user_verify_request(MPI2_REQUEST_HEADER *hdr, MPI2_SGE_IO_UNION **psgl)
{
	int i, err = EINVAL;

	for (i = 0; i < sizeof(mps_user_func_list) /
	    sizeof(mps_user_func_list[0]); i++ ) {
		struct mps_user_func *func = &mps_user_func_list[i];
		
		if (hdr->Function == func->Func) {
			if (psgl != NULL) {
				if (func->SgOff != 0)
					*psgl = (PTR_MPI2_SGE_IO_UNION)
					    ((char*)hdr + func->SgOff);
				else
					*psgl = NULL;
				err = 0;
				break;
			}
		}
	}	

	return err;
}	

static int
mps_user_command(struct mps_softc *sc, struct mps_usr_command *cmd)
{
	MPI2_REQUEST_HEADER *hdr;	
	MPI2_DEFAULT_REPLY *rpl;
	MPI2_SGE_IO_UNION *sgl;	
	void *buf;
	struct mps_command *cm;
	int err = 0;
	int sz;

	mps_lock(sc);
	cm = mps_alloc_command(sc);

	if (cm == NULL) {
		mps_printf(sc, "mps_user_command: no mps requests\n");
		err = ENOMEM;
		goto Ret;
	}
	mps_unlock(sc);

	hdr = (MPI2_REQUEST_HEADER *)cm->cm_req;

	mps_dprint(sc, MPS_INFO, "mps_user_command: req %p %d  rpl %p %d\n",
		    cmd->req, cmd->req_len, cmd->rpl, cmd->rpl_len );

	copyin(cmd->req, hdr, cmd->req_len);

	mps_dprint(sc, MPS_INFO, "mps_user_command: Function %02X  "
	    "MsgFlags %02X\n", hdr->Function, hdr->MsgFlags );

	err = mps_user_verify_request(hdr, &sgl);
	if (err != 0) {
		mps_printf(sc, "mps_user_command: unsupported function 0x%X\n",
		    hdr->Function );
		goto RetFree;
	}

	if (cmd->len > 0) {
		buf = malloc(cmd->len, M_MPSUSER, M_WAITOK|M_ZERO);
		cm->cm_data = buf;
		cm->cm_length = cmd->len;
	} else {
		buf = NULL;
		cm->cm_data = NULL;
		cm->cm_length = 0;
	}

	cm->cm_sge = sgl;
	cm->cm_sglsize = sizeof(MPI2_SGE_IO_UNION);
	cm->cm_flags = MPS_CM_FLAGS_SGE_SIMPLE | MPS_CM_FLAGS_WAKEUP;
	cm->cm_desc.Default.RequestFlags = MPI2_REQ_DESCRIPT_FLAGS_DEFAULT_TYPE;

	mps_lock(sc);
	err = mps_map_command(sc, cm);

	if (err != 0) {
		mps_printf(sc, "mps_user_command: request timed out\n");
		goto Ret;
	}
	msleep(cm, &sc->mps_mtx, 0, "mpsuser", 0); /* 30 seconds */

	rpl = (MPI2_DEFAULT_REPLY *)cm->cm_reply;
	sz = rpl->MsgLength * 4;
	
	if (sz > cmd->rpl_len) {
		mps_printf(sc,
		    "mps_user_command: reply buffer too small %d required %d\n",
		    cmd->rpl_len, sz );
		err = EINVAL;
		sz = cmd->rpl_len;
	}	

	mps_unlock(sc);
	copyout(rpl, cmd->rpl, sz);
	if (buf != NULL) {
		copyout(buf, cmd->buf, cmd->len);
		free(buf, M_MPSUSER);
	}
	mps_lock(sc);

	mps_dprint(sc, MPS_INFO, "mps_user_command: reply size %d\n", sz );

RetFree:	   
	mps_free_command(sc, cm);

Ret:
	mps_unlock(sc);
	return err;
}	

#ifdef __amd64__
#define	PTRIN(p)		((void *)(uintptr_t)(p))
#define PTROUT(v)		((u_int32_t)(uintptr_t)(v))
#endif

static int
mps_ioctl(struct cdev *dev, u_long cmd, caddr_t arg, int flag,
    struct thread *td)
{
	struct mps_softc *sc;
	struct mps_cfg_page_req *page_req;
	struct mps_ext_cfg_page_req *ext_page_req;
	void *mps_page;
#ifdef __amd64__
	struct mps_cfg_page_req32 *page_req32;
	struct mps_cfg_page_req page_req_swab;
	struct mps_ext_cfg_page_req32 *ext_page_req32;
	struct mps_ext_cfg_page_req ext_page_req_swab;
#endif
	int error;

	mps_page = NULL;
	sc = dev->si_drv1;
	page_req = (void *)arg;
	ext_page_req = (void *)arg;

#ifdef __amd64__
	/* Convert 32-bit structs to native ones. */
	page_req32 = (void *)arg;
	ext_page_req32 = (void *)arg;
	switch (cmd) {
	case MPSIO_READ_CFG_HEADER32:
	case MPSIO_READ_CFG_PAGE32:
	case MPSIO_WRITE_CFG_PAGE32:
		page_req = &page_req_swab;
		page_req->header = page_req32->header;
		page_req->page_address = page_req32->page_address;
		page_req->buf = PTRIN(page_req32->buf);
		page_req->len = page_req32->len;
		page_req->ioc_status = page_req32->ioc_status;
		break;
	case MPSIO_READ_EXT_CFG_HEADER32:
	case MPSIO_READ_EXT_CFG_PAGE32:
		ext_page_req = &ext_page_req_swab;
		ext_page_req->header = ext_page_req32->header;
		ext_page_req->page_address = ext_page_req32->page_address;
		ext_page_req->buf = PTRIN(ext_page_req32->buf);
		ext_page_req->len = ext_page_req32->len;
		ext_page_req->ioc_status = ext_page_req32->ioc_status;
		break;
	default:
		return (ENOIOCTL);
	}
#endif

	switch (cmd) {
#ifdef __amd64__
	case MPSIO_READ_CFG_HEADER32:
#endif
	case MPSIO_READ_CFG_HEADER:
		mps_lock(sc);
		error = mps_user_read_cfg_header(sc, page_req);
		mps_unlock(sc);
		break;
#ifdef __amd64__
	case MPSIO_READ_CFG_PAGE32:
#endif
	case MPSIO_READ_CFG_PAGE:
		mps_page = malloc(page_req->len, M_MPSUSER, M_WAITOK | M_ZERO);
		error = copyin(page_req->buf, mps_page,
		    sizeof(MPI2_CONFIG_PAGE_HEADER));
		if (error)
			break;
		mps_lock(sc);
		error = mps_user_read_cfg_page(sc, page_req, mps_page);
		mps_unlock(sc);
		if (error)
			break;
		error = copyout(mps_page, page_req->buf, page_req->len);
		break;
#ifdef __amd64__
	case MPSIO_READ_EXT_CFG_HEADER32:
#endif
	case MPSIO_READ_EXT_CFG_HEADER:
		mps_lock(sc);
		error = mps_user_read_extcfg_header(sc, ext_page_req);
		mps_unlock(sc);
		break;
#ifdef __amd64__
	case MPSIO_READ_EXT_CFG_PAGE32:
#endif
	case MPSIO_READ_EXT_CFG_PAGE:
		mps_page = malloc(ext_page_req->len, M_MPSUSER, M_WAITOK|M_ZERO);
		error = copyin(ext_page_req->buf, mps_page,
		    sizeof(MPI2_CONFIG_EXTENDED_PAGE_HEADER));
		if (error)
			break;
		mps_lock(sc);
		error = mps_user_read_extcfg_page(sc, ext_page_req, mps_page);
		mps_unlock(sc);
		if (error)
			break;
		error = copyout(mps_page, ext_page_req->buf, ext_page_req->len);
		break;
#ifdef __amd64__
	case MPSIO_WRITE_CFG_PAGE32:
#endif
	case MPSIO_WRITE_CFG_PAGE:
		mps_page = malloc(page_req->len, M_MPSUSER, M_WAITOK|M_ZERO);
		error = copyin(page_req->buf, mps_page, page_req->len);
		if (error)
			break;
		mps_lock(sc);
		error = mps_user_write_cfg_page(sc, page_req, mps_page);
		mps_unlock(sc);
		break;
	case MPSIO_MPS_COMMAND:
		error = mps_user_command(sc, (struct mps_usr_command *)arg);
		break;
	default:
		error = ENOIOCTL;
		break;
	}

	if (mps_page != NULL)
		free(mps_page, M_MPSUSER);

	if (error)
		return (error);

#ifdef __amd64__
	/* Convert native structs to 32-bit ones. */
	switch (cmd) {
	case MPSIO_READ_CFG_HEADER32:
	case MPSIO_READ_CFG_PAGE32:
	case MPSIO_WRITE_CFG_PAGE32:
		page_req32->header = page_req->header;
		page_req32->page_address = page_req->page_address;
		page_req32->buf = PTROUT(page_req->buf);
		page_req32->len = page_req->len;
		page_req32->ioc_status = page_req->ioc_status;
		break;
	case MPSIO_READ_EXT_CFG_HEADER32:
	case MPSIO_READ_EXT_CFG_PAGE32:		
		ext_page_req32->header = ext_page_req->header;
		ext_page_req32->page_address = ext_page_req->page_address;
		ext_page_req32->buf = PTROUT(ext_page_req->buf);
		ext_page_req32->len = ext_page_req->len;
		ext_page_req32->ioc_status = ext_page_req->ioc_status;
		break;
	default:
		return (ENOIOCTL);
	}
#endif

	return (0);
}
