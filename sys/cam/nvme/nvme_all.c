/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2015 Netflix, Inc.
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

#include <sys/param.h>

#ifdef _KERNEL
#include "opt_scsi.h"

#include <sys/systm.h>
#include <sys/libkern.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>
#else
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef min
#define min(a,b) (((a)<(b))?(a):(b))
#endif
#endif

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_queue.h>
#include <cam/cam_xpt.h>
#include <cam/nvme/nvme_all.h>
#include <dev/nvme/nvme.h>
#include <sys/sbuf.h>
#include <sys/endian.h>

#ifdef _KERNEL
#include <cam/cam_periph.h>
#include <cam/cam_xpt_sim.h>
#include <cam/cam_xpt_periph.h>
#include <cam/cam_xpt_internal.h>
#endif

void
nvme_ns_cmd(struct ccb_nvmeio *nvmeio, uint8_t cmd, uint32_t nsid,
    uint32_t cdw10, uint32_t cdw11, uint32_t cdw12, uint32_t cdw13,
    uint32_t cdw14, uint32_t cdw15)
{
	bzero(&nvmeio->cmd, sizeof(struct nvme_command));
	nvmeio->cmd.opc = cmd;
	nvmeio->cmd.nsid = htole32(nsid);
	nvmeio->cmd.cdw10 = htole32(cdw10);
	nvmeio->cmd.cdw11 = htole32(cdw11);
	nvmeio->cmd.cdw12 = htole32(cdw12);
	nvmeio->cmd.cdw13 = htole32(cdw13);
	nvmeio->cmd.cdw14 = htole32(cdw14);
	nvmeio->cmd.cdw15 = htole32(cdw15);
}

int
nvme_identify_match(caddr_t identbuffer, caddr_t table_entry)
{
	return 0;
}

void
nvme_print_ident(const struct nvme_controller_data *cdata,
    const struct nvme_namespace_data *data, struct sbuf *sb)
{
	nvme_print_ident_short(cdata, data, sb);
	sbuf_putc(sb, '\n');
}

void
nvme_print_ident_short(const struct nvme_controller_data *cdata,
    const struct nvme_namespace_data *data, struct sbuf *sb)
{
	sbuf_putc(sb, '<');
	cam_strvis_sbuf(sb, cdata->mn, sizeof(cdata->mn),
	    CAM_STRVIS_FLAG_NONASCII_SPC);
	sbuf_putc(sb, ' ');
	cam_strvis_sbuf(sb, cdata->fr, sizeof(cdata->fr),
	    CAM_STRVIS_FLAG_NONASCII_SPC);
	sbuf_putc(sb, ' ');
	cam_strvis_sbuf(sb, cdata->sn, sizeof(cdata->sn),
	    CAM_STRVIS_FLAG_NONASCII_SPC);
	sbuf_putc(sb, '>');
}

const char *
nvme_command_string(struct ccb_nvmeio *nvmeio, char *cmd_string, size_t len)
{
	struct sbuf sb;
	int error;

	if (len == 0)
		return ("");

	sbuf_new(&sb, cmd_string, len, SBUF_FIXEDLEN);
	nvme_command_sbuf(nvmeio, &sb);

	error = sbuf_finish(&sb);
	if (error != 0 &&
#ifdef _KERNEL
	    error != ENOMEM)
#else
	    errno != ENOMEM)
#endif
		return ("");

	return(sbuf_data(&sb));
}

void
nvme_cmd_sbuf(const struct nvme_command *cmd, struct sbuf *sb)
{

	/*
	 * cid, rsvd areas and mptr not printed, since they are used
	 * only internally by the SIM.
	 */
	sbuf_printf(sb,
	    "opc=%x fuse=%x nsid=%x prp1=%llx prp2=%llx cdw=%x %x %x %x %x %x",
	    cmd->opc, cmd->fuse, cmd->nsid,
	    (unsigned long long)cmd->prp1, (unsigned long long)cmd->prp2,
	    cmd->cdw10, cmd->cdw11, cmd->cdw12,
	    cmd->cdw13, cmd->cdw14, cmd->cdw15);
}

/*
 * nvme_command_sbuf() returns 0 for success and -1 for failure.
 */
int
nvme_command_sbuf(struct ccb_nvmeio *nvmeio, struct sbuf *sb)
{

	nvme_opcode_sbuf(nvmeio->ccb_h.func_code == XPT_NVME_ADMIN,
	    nvmeio->cmd.opc, sb);
	sbuf_cat(sb, ". NCB: ");
	nvme_cmd_sbuf(&nvmeio->cmd, sb);
	return(0);
}

void
nvme_resv_report(struct ccb_nvmeio *nvmeio, uint32_t retries,
		 void (*cbfcnp)(struct cam_periph *, union ccb *),
		 uint32_t nsid, uint32_t report_type, uint8_t *data_ptr,
		 uint32_t dxfer_len, uint32_t timeout)
{
	cam_fill_nvmeio(/*nvmeio*/ nvmeio,
			/*retries*/ retries,
			/*cbfcnp*/ cbfcnp,
			/*flags*/ CAM_DIR_IN,
			/*data_ptr*/ data_ptr,
			/*dxfer_len*/ dxfer_len,
			/*timeout*/ timeout);

	nvme_ns_resv_report_cmd(/*cmd*/ &nvmeio->cmd,
				/*nsid*/ nsid,
				/*length*/ dxfer_len,
				/*report_type*/ report_type);
}

void
nvme_resv_register(struct ccb_nvmeio *nvmeio, uint32_t retries,
		   void (*cbfcnp)(struct cam_periph *, union ccb *),
		   uint32_t nsid, int cptpl, int iekey, int rrega,
		   uint8_t *data_ptr, uint32_t dxfer_len, uint32_t timeout)
{
	cam_fill_nvmeio(/*nvmeio*/ nvmeio,
			/*retries*/ retries,
			/*cbfcnp*/ cbfcnp,
			/*flags*/ CAM_DIR_OUT,
			/*data_ptr*/ data_ptr,
			/*dxfer_len*/ dxfer_len,
			/*timeout*/ timeout);

	nvme_ns_resv_register_cmd(/*cmd*/ &nvmeio->cmd,
				  /*nsid*/ nsid,
				  /*cptpl*/ cptpl,
				  /*iekey*/ iekey,
				  /*rrega*/ rrega);
}

void
nvme_resv_acquire(struct ccb_nvmeio *nvmeio, uint32_t retries,
		  void (*cbfcnp)(struct cam_periph *, union ccb *),
		  uint32_t nsid, int rtype, int iekey, int racqa,
		  uint8_t *data_ptr, uint32_t dxfer_len, uint32_t timeout)
{
	cam_fill_nvmeio(/*nvmeio*/ nvmeio,
			/*retries*/ retries,
			/*cbfcnp*/ cbfcnp,
			/*flags*/ CAM_DIR_OUT,
			/*data_ptr*/ data_ptr,
			/*dxfer_len*/ dxfer_len,
			/*timeout*/ timeout);

	nvme_ns_resv_acquire_cmd(/*cmd*/ &nvmeio->cmd,
				 /*nsid*/ nsid,
				 /*rtype*/ rtype,
				 /*iekey*/ iekey,
				 /*racqa*/ racqa);
}

void
nvme_resv_release(struct ccb_nvmeio *nvmeio, uint32_t retries,
		  void (*cbfcnp)(struct cam_periph *, union ccb *),
		  uint32_t nsid, int rtype, int iekey, int rrela,
		  uint8_t *data_ptr, uint32_t dxfer_len, uint32_t timeout)
{
	cam_fill_nvmeio(/*nvmeio*/ nvmeio,
			/*retries*/ retries,
			/*cbfcnp*/ cbfcnp,
			/*flags*/ CAM_DIR_OUT,
			/*data_ptr*/ data_ptr,
			/*dxfer_len*/ dxfer_len,
			/*timeout*/ timeout);

	nvme_ns_resv_release_cmd(/*cmd*/ &nvmeio->cmd,
				 /*nsid*/ nsid,
				 /*rtype*/ rtype,
				 /*iekey*/ iekey,
				 /*rrela*/ rrela);
}

void
nvme_format(struct ccb_nvmeio *nvmeio, uint32_t retries,
	    void (*cbfcnp)(struct cam_periph *, union ccb *),
	    uint32_t nsid, int ses, int pil, int pi, int mset, int lbaf,
	    uint32_t timeout)
{
	cam_fill_nvmeadmin(/*nvmeio*/ nvmeio,
			   /*retries*/ retries,
			   /*cbfcnp*/ cbfcnp,
			   /*flags*/ CAM_DIR_NONE,
			   /*data_ptr*/ NULL,
			   /*dxfer_len*/ 0,
			   /*timeout*/ timeout);

	nvme_admin_format_cmd(/*cmd*/ &nvmeio->cmd,
			      /*nsid*/ nsid,
			      /*ses*/ ses,
			      /*pil*/ pil,
			      /*pi*/ pi,
			      /*mset*/ mset,
			      /*lbaf*/ lbaf);
}

void
nvme_get_feature(struct ccb_nvmeio *nvmeio, uint32_t retries,
		 void (*cbfcnp)(struct cam_periph *, union ccb *),
		 uint32_t nsid, int sel, uint8_t feature, uint32_t cdw11,
		 int uuid_idx, uint8_t *data_ptr, uint32_t dxfer_len,
		 uint32_t timeout)
{
	cam_fill_nvmeadmin(/*nvmeio*/ nvmeio,
			   /*retries*/ retries,
			   /*cbfcnp*/ cbfcnp,
			   /*flags*/ CAM_DIR_IN,
			   /*data_ptr*/ data_ptr,
			   /*dxfer_len*/ dxfer_len,
			   /*timeout*/ timeout);

	nvme_admin_get_feature_cmd(/*cmd*/ &nvmeio->cmd,
				   /*nsid*/ nsid,
				   /*sel*/ sel,
				   /*feature*/ feature,
				   /*cdw11*/ cdw11,
				   /*uuid_idx*/ uuid_idx);
}

void
nvme_set_feature(struct ccb_nvmeio *nvmeio, uint32_t retries,
		 void (*cbfcnp)(struct cam_periph *, union ccb *),
		 uint32_t nsid, int sv, uint8_t feature, uint32_t cdw11,
		 uint32_t cdw12, uint32_t cdw13, uint32_t cdw14, uint32_t cdw15,
		 uint8_t *data_ptr, uint32_t dxfer_len, uint32_t timeout)
{
	cam_fill_nvmeadmin(/*nvmeio*/ nvmeio,
			   /*retries*/ retries,
			   /*cbfcnp*/ cbfcnp,
			   /*flags*/ CAM_DIR_OUT,
			   /*data_ptr*/ data_ptr,
			   /*dxfer_len*/ dxfer_len,
			   /*timeout*/ timeout);

	nvme_admin_set_feature_cmd(/*cmd*/ &nvmeio->cmd,
				   /*nsid*/ nsid,
				   /*sv*/ sv,
				   /*feature*/ feature,
				   /*cdw11*/ cdw11,
				   /*cdw12*/ cdw12,
				   /*cdw13*/ cdw13,
				   /*cdw14*/ cdw14,
				   /*cdw15*/ cdw15);
}

void
nvme_get_feature_hostid(struct ccb_nvmeio *nvmeio, uint32_t retries,
			void (*cbfcnp)(struct cam_periph *, union ccb *),
			int sel, int exhid, int uuid_idx, uint8_t *data_ptr,
			uint32_t dxfer_len, uint32_t timeout)
{
	uint32_t cdw11 = 0;

	if (exhid)
		cdw11 = 0x01;

	nvme_get_feature(/*nvmeio*/ nvmeio,
			 /*retries*/ retries,
			 /*cbfcnp*/ cbfcnp,
			 /*nsid*/ 0,
			 /*sel*/ sel,
			 /*feature*/ NVME_FEAT_HOST_IDENTIFIER,
			 /*cdw11*/ cdw11,
			 /*uuid_idx*/ uuid_idx,
			 /*data_ptr*/ data_ptr,
			 /*dxfer_len*/ dxfer_len,
			 /*timeout*/ timeout);
}

void
nvme_set_feature_hostid(struct ccb_nvmeio *nvmeio, uint32_t retries,
			void (*cbfcnp)(struct cam_periph *, union ccb *),
			int sv, int exhid, uint8_t *data_ptr,
			uint32_t dxfer_len, uint32_t timeout)
{
	uint32_t cdw11 = 0;
	if (exhid)
		cdw11 = 0x01;

	nvme_set_feature(/*nvmeio*/ nvmeio,
			 /*retries*/ retries,
			 /*cbfcnp*/ cbfcnp,
			 /*nsid*/ 0,
			 /*sv*/ sv,
			 /*feature*/ NVME_FEAT_HOST_IDENTIFIER,
			 /*cdw11*/ cdw11,
			 /*cdw12*/ 0,
			 /*cdw13*/ 0,
			 /*cdw14*/ 0,
			 /*cdw15*/ 0,
			 /*data_ptr*/ data_ptr,
			 /*dxfer_len*/ dxfer_len,
			 /*timeout*/ timeout);
}

/*
 * nvme_status_sbuf() returns 0 for success and -1 for failure.
 */
int
nvme_status_sbuf(struct ccb_nvmeio *nvmeio, struct sbuf *sb)
{
	nvme_cpl_sbuf(&nvmeio->cpl, sb);
	return (0);
}

#ifdef _KERNEL
const void *
nvme_get_identify_cntrl(struct cam_periph *periph)
{
	struct cam_ed *device;

	device = periph->path->device;

	return device->nvme_cdata;
}

const void *
nvme_get_identify_ns(struct cam_periph *periph)
{
	struct cam_ed *device;

	device = periph->path->device;

	return device->nvme_data;
}
#endif
