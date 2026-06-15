/*
 * Broadcom NetXtreme-C/E network driver.
 *
 * Copyright (c) 2022 Broadcom, All Rights Reserved.
 * The term Broadcom refers to Broadcom Limited and/or its subsidiaries
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS'
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "bnxt_mgmt.h"
#include "bnxt.h"
#include "bnxt_hwrm.h"
#include "bnxt_coredump.h"
#include "bnxt_log.h"
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <sys/endian.h>
#include <sys/lock.h>

/* Function prototypes */
static d_open_t      bnxt_mgmt_open;
static d_close_t     bnxt_mgmt_close;
static d_ioctl_t     bnxt_mgmt_ioctl;

/* Character device entry points */
static struct cdevsw bnxt_mgmt_cdevsw = {
	.d_version = D_VERSION,
	.d_open = bnxt_mgmt_open,
	.d_close = bnxt_mgmt_close,
	.d_ioctl = bnxt_mgmt_ioctl,
	.d_name = "bnxt_mgmt",
};

/* Global vars */
static struct cdev *bnxt_mgmt_dev;
struct mtx		mgmt_lock;

MALLOC_DEFINE(M_BNXT, "bnxt_mgmt_buffer", "buffer for bnxt_mgmt module");


static uint32_t
bnxt_get_driver_coredump_len(struct bnxt_softc *softc)
{

	uint32_t type, i, j, n;
	uint32_t buf_size = 0;
	int ctx_page_count = 0;
	int segment_len = 0;
	int driver_segment_record_len = 0;
	uint32_t dump_len = 0;
	int record_len = sizeof(struct bnxt_driver_segment_record);
	struct bnxt_ctx_mem_info *ctx = softc->ctx_mem;

	if (!ctx)
		return (dump_len);

	for (type = BNXT_CTX_SRT_TRACE; type <= BNXT_CTX_ROCE_HWRM_TRACE;
	     type++) {
		struct bnxt_ctx_mem_type *ctxm = &ctx->ctx_arr[type];
		struct bnxt_ctx_pg_info *ctx_pg = ctxm->pg_info;

		if (!ctx_pg)
			continue;

		if (ctxm->instance_bmap)
			n = bitcount32(ctxm->instance_bmap);
		else
			n = 1;

		for (i = 0; i < n; i++) {
			struct bnxt_ring_mem_info *rmem = &ctx_pg->ring_mem;

			if (ctx_pg->nr_pages > MAX_CTX_PAGES ||
			    ctx_pg->ctx_pg_tbl) {
				int k = 0, nr_tbls = rmem->nr_pages;

				for (k = 0; k < nr_tbls; k++) {
					struct bnxt_ctx_pg_info *pg_tbl;
					struct bnxt_ring_mem_info *rmem2;

					pg_tbl = ctx_pg->ctx_pg_tbl[k];
					if (!pg_tbl)
						continue;
					rmem2 = &pg_tbl->ring_mem;
					for (j = 0; j < rmem2->nr_pages; j++) {
						if (!rmem2->pg_arr[j].idi_vaddr)
							continue;
						ctx_page_count++;
					}
				}
			} else {
				struct bnxt_ring_mem_info *rmem2 = rmem;

				for (j = 0; j < rmem2->nr_pages; j++) {
					if (!rmem2->pg_arr[j].idi_vaddr)
						continue;
					ctx_page_count++;
				}
			}
		}
		segment_len += 64;
		driver_segment_record_len += record_len;
	}

	buf_size = driver_segment_record_len + segment_len +
	    (ctx_page_count * 4096);

	return (buf_size);
}

inline void
bnxt_bs_trace_check_wrapping(struct bnxt_bs_trace_info *bs_trace,
    u32 offset)
{
        if (!bs_trace->wrapped &&
            *bs_trace->magic_byte != BNXT_TRACE_BUF_MAGIC_BYTE)
                bs_trace->wrapped = 1;
        bs_trace->last_offset = offset;
}



static int
bnxt_hwrm_dbg_log_buffer_flush(struct bnxt_softc *bp, u16 type, u32 flags,
    u32 *offset)
{
        int  status = 0;

        hwrm_dbg_log_buffer_flush_input_t buff_flush_req;
        hwrm_dbg_log_buffer_flush_output_t *buff_flush_resp =
            (hwrm_dbg_log_buffer_flush_output_t *)(void *)
            bp->hwrm_cmd_resp.idi_vaddr;
        bnxt_hwrm_cmd_hdr_init(bp, &buff_flush_req, HWRM_DBG_LOG_BUFFER_FLUSH);
        buff_flush_req.type = type;
        buff_flush_req.flags = flags;

        status = hwrm_send_message(bp, &buff_flush_req, sizeof(buff_flush_req));
        if (!status)
                *offset = buff_flush_resp->current_buffer_offset;
        return (status);
}

static void
bnxt_fill_driver_segment_record(struct bnxt_softc *bp,
    struct bnxt_driver_segment_record *drv_seg_rec,
    struct bnxt_ctx_mem_type *ctxm, uint16_t type)
{
        struct bnxt_bs_trace_info *bs_trace = &bp->bs_trace[type];
        uint32_t offset;

        if (bnxt_hwrm_dbg_log_buffer_flush(bp, type, 0, &offset) == 0) {
                bnxt_bs_trace_check_wrapping(bs_trace, offset);
        }
        drv_seg_rec->max_entries = ctxm->max_entries;
        drv_seg_rec->entry_size = ctxm->entry_size;
        drv_seg_rec->offset = bs_trace->last_offset;
        drv_seg_rec->wrapped = bs_trace->wrapped;
}

static void
bnxt_retrieve_driver_coredump(struct bnxt_softc *softc, void *buf,
    uint16_t type, uint32_t *seg_len)
{
	struct bnxt_driver_segment_record drv_seg_rec = {0};
	struct bnxt_ctx_mem_info *ctx = softc->ctx_mem;
	struct bnxt_ctx_mem_type *ctxm = &ctx->ctx_arr[type];
	struct bnxt_ctx_pg_info *ctx_pg = ctxm->pg_info;
	uint32_t dump_len, data_offset, record_len, seg_hdr_len;
	uint32_t i, j, k, n = 1, nr_tbls;

	dump_len = 0;

	record_len = sizeof(struct bnxt_driver_segment_record);
	seg_hdr_len = sizeof(struct bnxt_coredump_segment_hdr);
	data_offset = seg_hdr_len + record_len;

	bnxt_fill_driver_segment_record(softc, &drv_seg_rec, ctxm,
	    (type - BNXT_CTX_SRT_TRACE));

	for (i = 0; i < n; i++) {
		struct bnxt_ring_mem_info *rmem = &ctx_pg->ring_mem;

		if (ctx_pg->nr_pages > MAX_CTX_PAGES || ctx_pg->ctx_pg_tbl) {
			nr_tbls = rmem->nr_pages;
			for (j = 0; j < nr_tbls; j++) {
				struct bnxt_ctx_pg_info *pg_tbl;
				struct bnxt_ring_mem_info *rmem2;

				pg_tbl = ctx_pg->ctx_pg_tbl[j];
				if (!pg_tbl)
					continue;
				rmem2 = &pg_tbl->ring_mem;
				for (k = 0; k < rmem2->nr_pages; k++) {
					if (!rmem2->pg_arr[k].idi_vaddr)
						continue;
					memcpy((uint8_t *)buf + data_offset,
					    rmem2->pg_arr[k].idi_vaddr,
					    rmem2->page_size);
					data_offset += rmem2->page_size;
					dump_len += rmem2->page_size;
				}
			}
		} else {
			for (k = 0; k < rmem->nr_pages; k++) {
				if (!rmem->pg_arr[k].idi_vaddr)
					continue;
				memcpy((uint8_t *)buf + data_offset,
				    rmem->pg_arr[k].idi_vaddr,
				    rmem->page_size);
				data_offset += rmem->page_size;
				dump_len += rmem->page_size;
			}
		}
	}
	memcpy((uint8_t *)buf + seg_hdr_len, &drv_seg_rec, record_len);
	*seg_len = dump_len + record_len;
}

void
bnxt_get_ctx_coredump(struct bnxt_softc *softc, void *buf)
{
	struct bnxt_ctx_mem_info *ctx = softc->ctx_mem;
	struct bnxt_coredump_segment_hdr seg_hdr;
	uint32_t type = 0, i = 0;
	uint32_t seg_hdr_len = 0;

	seg_hdr_len = sizeof(seg_hdr);
	for (type = BNXT_CTX_SRT_TRACE, i = DRV_SRT_TRACE_SEG_ID;
	     type <= BNXT_CTX_ROCE_HWRM_TRACE; type++, i++) {
		struct bnxt_ctx_mem_type *ctxm = &ctx->ctx_arr[type];
		uint16_t comp_id = DRV_COREDUMP_COMP_ID;
		uint16_t seg_id = i;
		uint32_t seg_len = 0;

		ctxm = &ctx->ctx_arr[type];

		if (!(ctxm->flags & BNXT_CTX_MEM_TYPE_VALID) ||
		    !ctxm->mem_valid)
			continue;

		bnxt_retrieve_driver_coredump(softc, buf, type, &seg_len);

		bnxt_fill_coredump_seg_hdr(softc, &seg_hdr, NULL, seg_len,
		    0, 0, 0, comp_id, seg_id);

		memcpy((uint8_t *)buf, &seg_hdr, seg_hdr_len);
		buf = (uint8_t *)buf + seg_hdr_len + seg_len;
	}
}

/* DDR Crash Dump IOCTL handler */
static int
bnxt_mgmt_crash_dump(struct cdev *dev, u_long cmd, caddr_t data,
		       int flag, struct thread *td)
{
	struct bnxt_softc *softc = NULL;
	struct bnxt_mgmt_crash_dump mgmt_crash_dump = {0};
	void *user_ptr;
	int ret = 0;
	void *dump_buf = NULL;
	uint32_t dump_len;

	memcpy(&user_ptr, data, sizeof(user_ptr));
	if (copyin(user_ptr, &mgmt_crash_dump, sizeof(mgmt_crash_dump))) {
		printf("%s: %s:%d Failed to copy data from user\n",
			DRIVER_NAME, __func__, __LINE__);
		return (-EFAULT);
	}
	softc = bnxt_find_dev(mgmt_crash_dump.hdr.domain,
			      mgmt_crash_dump.hdr.bus,
			      mgmt_crash_dump.hdr.devfn, NULL);
	if (!softc) {
		printf("%s: %s:%d unable to find softc reference\n",
			DRIVER_NAME, __func__, __LINE__);
		return (-ENODEV);
	}

	switch (mgmt_crash_dump.op) {
	case BNXT_MGMT_SET_DUMP_FLAG:
		if (mgmt_crash_dump.req.set_flag.dump_flag >
		    BNXT_DUMP_LIVE_WITH_CTX_L1_CACHE) {
			device_printf(softc->dev,
			    "Supports only Live(0), Crash(1), Driver(2), "
			    "Live with cached context(3) dumps.\n");
			ret = -EINVAL;
			break;
		}

		if (mgmt_crash_dump.req.set_flag.dump_flag == BNXT_DUMP_CRASH) {
			if (softc->fw_dbg_cap & BNXT_FW_DBG_CAP_CRASHDUMP_SOC) {
				device_printf(softc->dev,
				    "Cannot collect crash dump as TEE is not supported.\n");
				ret = -ENOTSUP;
				break;
			} else if (!(softc->fw_dbg_cap &
				     BNXT_FW_DBG_CAP_CRASHDUMP_HOST)) {
				device_printf(softc->dev,
				    "FW does not support crash dump collection.\n");
				ret = -ENOTSUP;
				break;
			}
		}

		softc->dump_flag = mgmt_crash_dump.req.set_flag.dump_flag;
		break;

	case BNXT_MGMT_GET_DUMP_FLAG:
		if (softc->hwrm_spec_code < 0x10801) {
			ret = -ENOTSUP;
			break;
		}

		/* Build FW version - same as Linux bnxt_get_dump_flag() */
		mgmt_crash_dump.req.get_flag.version =
			(softc->ver_resp.hwrm_fw_maj_8b << 24) |
			(softc->ver_resp.hwrm_fw_min_8b << 16) |
			(softc->ver_resp.hwrm_fw_bld_8b << 8) |
			(softc->ver_resp.hwrm_fw_rsvd_8b);

		mgmt_crash_dump.req.get_flag.dump_flag = softc->dump_flag;
		mgmt_crash_dump.req.get_flag.dump_len =
			bnxt_get_coredump_length(softc, softc->dump_flag);
		break;

	case BNXT_MGMT_GET_DUMP_DATA:
		if (softc->hwrm_spec_code < 0x10801) {
			ret = -ENOTSUP;
			break;
		}

		dump_len = bnxt_get_coredump_length(softc,
			mgmt_crash_dump.req.get_data.dump_flag);
		if (dump_len == 0) {
			device_printf(softc->dev, "No dump data available\n");
			ret = -ENOENT;
			break;
		}

		if (mgmt_crash_dump.req.get_data.buffer_size < dump_len) {
			device_printf(softc->dev,
			    "Buffer too small: need %u bytes, got %zu bytes\n",
			    dump_len, mgmt_crash_dump.req.get_data.buffer_size);
			mgmt_crash_dump.req.get_data.dump_len = dump_len;
			ret = -ENOSPC;
			break;
		}

		dump_buf = malloc(dump_len, M_BNXT, M_WAITOK);
		if (!dump_buf) {
			ret = -ENOMEM;
			break;
		}

		ret = bnxt_get_coredump(softc,
		    mgmt_crash_dump.req.get_data.dump_flag,
		    dump_buf, &dump_len);
		if (ret) {
			device_printf(softc->dev,
			    "Failed to get coredump: %d\n", ret);
			free(dump_buf, M_BNXT);
			break;
		}

		if (copyout(dump_buf,
		    mgmt_crash_dump.req.get_data.dump_buffer, dump_len)) {
			device_printf(softc->dev,
			    "%s:%d Failed to copy dump data to user\n",
			    __func__, __LINE__);
			ret = -EFAULT;
			free(dump_buf, M_BNXT);
			break;
		}

		mgmt_crash_dump.req.get_data.dump_len = dump_len;
		mgmt_crash_dump.req.get_data.dump_flag = softc->dump_flag;
		free(dump_buf, M_BNXT);
		break;

	default:
		device_printf(softc->dev, "%s:%d Invalid op 0x%x\n",
			      __func__, __LINE__, mgmt_crash_dump.op);
		ret = -EFAULT;
		break;
	}

	if (!ret && copyout(&mgmt_crash_dump, user_ptr,
	    sizeof(mgmt_crash_dump))) {
		device_printf(softc->dev,
		    "%s:%d Failed to copy response to user\n",
		    __func__, __LINE__);
		ret = -EFAULT;
	}

	return (ret);
}

/*
 * This function is called by the kld[un]load(2) system calls to
 * determine what actions to take when a module is loaded or unloaded.
 */
static int
bnxt_mgmt_loader(struct module *m, int what, void *arg)
{
	int error = 0;

	switch (what) {
	case MOD_LOAD:
		error = make_dev_p(MAKEDEV_CHECKNAME | MAKEDEV_WAITOK,
		    &bnxt_mgmt_dev,
		    &bnxt_mgmt_cdevsw,
		    0,
		    UID_ROOT,
		    GID_WHEEL,
		    0600,
		    "bnxt_mgmt");
		if (error != 0) {
			printf("%s: %s:%s:%d Failed to create the"
			       "bnxt_mgmt device node\n", DRIVER_NAME,
			       __FILE__, __func__, __LINE__);
			return (error);
		}

		mtx_init(&mgmt_lock, "BNXT MGMT Lock", NULL, MTX_DEF);

		break;
	case MOD_UNLOAD:
		mtx_destroy(&mgmt_lock);
		destroy_dev(bnxt_mgmt_dev);
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}

	return (error);
}

static int
bnxt_mgmt_process_dcb(struct cdev *dev, u_long cmd, caddr_t data,
		       int flag, struct thread *td)
{
	struct bnxt_softc *softc = NULL;
	struct bnxt_mgmt_dcb mgmt_dcb = {};
	void *user_ptr;
	int ret = 0;

	memcpy(&user_ptr, data, sizeof(user_ptr));
	if (copyin(user_ptr, &mgmt_dcb, sizeof(mgmt_dcb))) {
		printf("%s: %s:%d Failed to copy data from user\n",
			DRIVER_NAME, __func__, __LINE__);
		return -EFAULT;
	}
	softc = bnxt_find_dev(mgmt_dcb.hdr.domain, mgmt_dcb.hdr.bus,
			      mgmt_dcb.hdr.devfn, NULL);
	if (!softc) {
		printf("%s: %s:%d unable to find softc reference\n",
			DRIVER_NAME, __func__, __LINE__);
		return -ENODEV;
	}

	switch (mgmt_dcb.op) {
	case BNXT_MGMT_DCB_GET_ETS:
		bnxt_dcb_ieee_getets(softc, &mgmt_dcb.req.ets);
		break;
	case BNXT_MGMT_DCB_SET_ETS:
		bnxt_dcb_ieee_setets(softc, &mgmt_dcb.req.ets);
		break;
	case BNXT_MGMT_DCB_GET_PFC:
		bnxt_dcb_ieee_getpfc(softc, &mgmt_dcb.req.pfc);
		break;
	case BNXT_MGMT_DCB_SET_PFC:
		bnxt_dcb_ieee_setpfc(softc, &mgmt_dcb.req.pfc);
		break;
	case BNXT_MGMT_DCB_SET_APP:
		bnxt_dcb_ieee_setapp(softc, &mgmt_dcb.req.app_tlv.app[0]);
		break;
	case BNXT_MGMT_DCB_DEL_APP:
		bnxt_dcb_ieee_delapp(softc, &mgmt_dcb.req.app_tlv.app[0]);
		break;
	case BNXT_MGMT_DCB_LIST_APP:
		bnxt_dcb_ieee_listapp(softc, &mgmt_dcb.req.app_tlv.app[0],
				      nitems(mgmt_dcb.req.app_tlv.app),
				      &mgmt_dcb.req.app_tlv.num_app);
		break;
	default:
		device_printf(softc->dev, "%s:%d Invalid op 0x%x\n",
			      __func__, __LINE__, mgmt_dcb.op);
		ret = -EFAULT;
		goto end;
	}

	if (copyout(&mgmt_dcb, user_ptr, sizeof(mgmt_dcb))) {
		device_printf(softc->dev, "%s:%d Failed to copy response to user\n",
			      __func__, __LINE__);
		ret = -EFAULT;
		goto end;
	}

end:
	return ret;
}

static int
bnxt_mgmt_process_hwrm(struct cdev *dev, u_long cmd, caddr_t data,
		       int flag, struct thread *td)
{
	struct bnxt_softc *softc = NULL;
	struct bnxt_mgmt_req mgmt_req = {};
	struct bnxt_mgmt_fw_msg msg_temp, *msg, *msg2 = NULL;
	struct iflib_dma_info dma_data = {};
	void *user_ptr, *req, *resp;
	int ret = 0;
	uint16_t num_ind = 0;

	memcpy(&user_ptr, data, sizeof(user_ptr));
	if (copyin(user_ptr, &mgmt_req, sizeof(struct bnxt_mgmt_req))) {
		printf("%s: %s:%d Failed to copy data from user\n",
			DRIVER_NAME, __func__, __LINE__);
		return -EFAULT;
	}
	softc = bnxt_find_dev(mgmt_req.hdr.domain, mgmt_req.hdr.bus,
			      mgmt_req.hdr.devfn, NULL);
	if (!softc) {
		printf("%s: %s:%d unable to find softc reference\n",
			DRIVER_NAME, __func__, __LINE__);
		return -ENODEV;
	}

	if (copyin((void*)mgmt_req.req.hreq, &msg_temp, sizeof(msg_temp))) {
		device_printf(softc->dev, "%s:%d Failed to copy data from user\n",
			      __func__, __LINE__);
		return -EFAULT;
	}

	if (msg_temp.len_req > BNXT_MGMT_MAX_HWRM_REQ_LENGTH ||
			msg_temp.len_resp > BNXT_MGMT_MAX_HWRM_RESP_LENGTH) {
		device_printf(softc->dev, "%s:%d Invalid length\n",
			      __func__, __LINE__);
		return -EINVAL;
	}

	if (msg_temp.num_dma_indications > 1) {
		device_printf(softc->dev, "%s:%d Max num_dma_indications "
			      "supported is 1\n", __func__, __LINE__);
		return -EINVAL;
	}

	req = malloc(msg_temp.len_req, M_BNXT, M_WAITOK | M_ZERO);
	resp = malloc(msg_temp.len_resp, M_BNXT, M_WAITOK | M_ZERO);

	if (copyin((void *)msg_temp.usr_req, req, msg_temp.len_req)) {
		device_printf(softc->dev, "%s:%d Failed to copy data from user\n",
			      __func__, __LINE__);
		ret = -EFAULT;
		goto end;
	}

	msg = &msg_temp;
	num_ind = msg_temp.num_dma_indications;
	if (num_ind) {
		int size;
		void *dma_ptr;
		uint64_t *dmap;

		size = sizeof(struct bnxt_mgmt_fw_msg) +
			     (num_ind * sizeof(struct dma_info));

		msg2 = malloc(size, M_BNXT, M_WAITOK | M_ZERO);

		if (copyin((void *)mgmt_req.req.hreq, msg2, size)) {
			device_printf(softc->dev, "%s:%d Failed to copy"
				      "data from user\n", __func__, __LINE__);
			ret = -EFAULT;
			goto end;
		}
		msg = msg2;

		ret = iflib_dma_alloc(softc->ctx, msg->dma[0].length, &dma_data,
				    BUS_DMA_NOWAIT);
		if (ret) {
			device_printf(softc->dev, "%s:%d iflib_dma_alloc"
				      "failed with ret = 0x%x\n", __func__,
				      __LINE__, ret);
			ret = -ENOMEM;
			goto end;
		}

		if (!(msg->dma[0].read_or_write)) {
			if (copyin((void *)msg->dma[0].data,
				   dma_data.idi_vaddr,
				   msg->dma[0].length)) {
				device_printf(softc->dev, "%s:%d Failed to copy"
					      "data from user\n", __func__,
					      __LINE__);
				ret = -EFAULT;
				goto end;
			}
		}
		dma_ptr = (void *) ((uint64_t) req + msg->dma[0].offset);
		dmap = dma_ptr;
		*dmap = htole64(dma_data.idi_paddr);
	}

	ret = bnxt_hwrm_passthrough(softc, req, msg->len_req, resp, msg->len_resp, msg->timeout);
	if(ret)
		goto end;

	if (num_ind) {
		if ((msg->dma[0].read_or_write)) {
			if (copyout(dma_data.idi_vaddr,
				    (void *)msg->dma[0].data,
				    msg->dma[0].length)) {
				device_printf(softc->dev, "%s:%d Failed to copy data"
					      "to user\n", __func__, __LINE__);
				ret = -EFAULT;
				goto end;
			}
		}
	}

	if (copyout(resp, (void *) msg->usr_resp, msg->len_resp)) {
		device_printf(softc->dev, "%s:%d Failed to copy response to user\n",
			      __func__, __LINE__);
		ret = -EFAULT;
		goto end;
	}

end:
	if (req)
		free(req, M_BNXT);
	if (resp)
		free(resp, M_BNXT);
	if (msg2)
		free(msg2, M_BNXT);
	if (dma_data.idi_paddr)
		iflib_dma_free(&dma_data);
	return ret;
}

static int
bnxt_mgmt_get_dev_info(struct cdev *dev, u_long cmd, caddr_t data,
		       int flag, struct thread *td)
{
	struct bnxt_softc *softc = NULL;
	struct bnxt_dev_info dev_info;
	void *user_ptr;
	uint32_t dev_sn_lo, dev_sn_hi;
	int dev_sn_offset = 0;
	char dsn[16];
	uint16_t lnk;
	int capreg;

	memcpy(&user_ptr, data, sizeof(user_ptr));
	if (copyin(user_ptr, &dev_info, sizeof(dev_info))) {
		printf("%s: %s:%d Failed to copy data from user\n",
			DRIVER_NAME, __func__, __LINE__);
		return -EFAULT;
	}

	softc = bnxt_find_dev(0, 0, 0, dev_info.nic_info.dev_name);
	if (!softc) {
		printf("%s: %s:%d unable to find softc reference\n",
			DRIVER_NAME, __func__, __LINE__);
		return -ENODEV;
	}

	strncpy(dev_info.nic_info.driver_version, bnxt_driver_version, 64);
	strncpy(dev_info.nic_info.driver_name, device_get_name(softc->dev), 64);
	dev_info.pci_info.domain_no = softc->domain;
	dev_info.pci_info.bus_no = softc->bus;
	dev_info.pci_info.device_no = softc->slot;
	dev_info.pci_info.function_no = softc->function;
	dev_info.pci_info.vendor_id = pci_get_vendor(softc->dev);
	dev_info.pci_info.device_id = pci_get_device(softc->dev);
	dev_info.pci_info.sub_system_vendor_id = pci_get_subvendor(softc->dev);
	dev_info.pci_info.sub_system_device_id = pci_get_subdevice(softc->dev);
	dev_info.pci_info.revision = pci_read_config(softc->dev, PCIR_REVID, 1);
	dev_info.pci_info.chip_rev_id = (dev_info.pci_info.device_id << 16);
	dev_info.pci_info.chip_rev_id |= dev_info.pci_info.revision;
	if (pci_find_extcap(softc->dev, PCIZ_SERNUM, &dev_sn_offset)) {
		device_printf(softc->dev, "%s:%d device serial number is not found"
			      "or not supported\n", __func__, __LINE__);
	} else {
		dev_sn_lo = pci_read_config(softc->dev, dev_sn_offset + 4, 4);
		dev_sn_hi = pci_read_config(softc->dev, dev_sn_offset + 8, 4);
		snprintf(dsn, sizeof(dsn), "%02x%02x%02x%02x%02x%02x%02x%02x",
			 (dev_sn_lo & 0x000000FF),
			 (dev_sn_lo >> 8) & 0x0000FF,
			 (dev_sn_lo >> 16) & 0x00FF,
			 (dev_sn_lo >> 24 ) & 0xFF,
			 (dev_sn_hi & 0x000000FF),
			 (dev_sn_hi >> 8) & 0x0000FF,
			 (dev_sn_hi >> 16) & 0x00FF,
			 (dev_sn_hi >> 24 ) & 0xFF);
		strncpy(dev_info.nic_info.device_serial_number, dsn, sizeof(dsn));
	}

	if_t ifp = iflib_get_ifp(softc->ctx);
	dev_info.nic_info.mtu = if_getmtu(ifp);
	memcpy(dev_info.nic_info.mac, softc->func.mac_addr, ETHER_ADDR_LEN);

	if (pci_find_cap(softc->dev, PCIY_EXPRESS, &capreg)) {
		device_printf(softc->dev, "%s:%d pci link capability is not found"
			      "or not supported\n", __func__, __LINE__);
	} else {
		lnk = pci_read_config(softc->dev, capreg + PCIER_LINK_STA, 2);
		dev_info.nic_info.pci_link_speed = (lnk & PCIEM_LINK_STA_SPEED);
		dev_info.nic_info.pci_link_width = (lnk & PCIEM_LINK_STA_WIDTH) >> 4;
	}

	if (copyout(&dev_info, user_ptr, sizeof(dev_info))) {
		device_printf(softc->dev, "%s:%d Failed to copy data to user\n",
			      __func__, __LINE__);
		return (-EFAULT);
	}

	return (0);
}

static int
bnxt_mgmt_drv_dump(struct cdev *dev, u_long cmd, caddr_t data,
		       int flag, struct thread *td)
{
	struct bnxt_softc *softc = NULL;
	void *buf = NULL;
	struct bnxt_logger *logger = NULL, *lg_tmp;
	int buf_sz = 0;
	struct bnxt_mgmt_drv_dump mgmt_drv_dump = {};
	void *user_ptr;
	int ret = 0, offset = 0;

	memcpy(&user_ptr, data, sizeof(user_ptr));
	if (copyin(user_ptr, &mgmt_drv_dump, sizeof(mgmt_drv_dump))) {
		printf("%s: %s:%d Failed to copy data from user\n",
			DRIVER_NAME, __func__, __LINE__);
		return (-EFAULT);
	}
	softc = bnxt_find_dev(mgmt_drv_dump.hdr.domain, mgmt_drv_dump.hdr.bus,
			      mgmt_drv_dump.hdr.devfn, NULL);
	if (!softc) {
		printf("%s: %s:%d unable to find softc reference\n",
			DRIVER_NAME, __func__, __LINE__);
		return (-ENODEV);
	}

	switch (mgmt_drv_dump.op) {
	case BNXT_MGMT_GET_DRV_DUMP_SIZE:
		mtx_lock(&softc->log_lock);
		TAILQ_FOREACH_SAFE(logger, &softc->loggers_list, list, lg_tmp)
			buf_sz += logger->buffer_size;
		mtx_unlock(&softc->log_lock);

		mgmt_drv_dump.buf_size = buf_sz +
		    bnxt_get_driver_coredump_len(softc);
		if (copyout(&mgmt_drv_dump, user_ptr, sizeof(mgmt_drv_dump))) {
			device_printf(softc->dev,
			    "%s:%d Failed to copy response to user\n",
			    __func__, __LINE__);
			ret = -EFAULT;
		}
		break;
	case BNXT_MGMT_GET_DRV_DUMP:
		buf = malloc(mgmt_drv_dump.buf_size, M_BNXT, M_WAITOK);
		/*Dump the driver logs */
		memset(buf, 0, mgmt_drv_dump.buf_size);
		offset = bnxt_start_logging_driver_coredump(softc, buf);

		if (!offset) {
			device_printf(softc->dev,
			    "%s:%d Drivers logs are empty\n",
			    __func__, __LINE__);
		}

		/* Dump the ctx logs*/
		if (softc->ctx_mem)
			bnxt_get_ctx_coredump(softc, (uint8_t *)buf + offset);

		if (copyout(buf, mgmt_drv_dump.buf, mgmt_drv_dump.buf_size)) {
			device_printf(softc->dev,
			    "%s:%d Failed to copy response to user\n",
			    __func__, __LINE__);
			ret = -EFAULT;
		}

		free(buf, M_BNXT);
		break;
	default:
		device_printf(softc->dev, "%s:%d Invalid op 0x%x\n",
			      __func__, __LINE__, mgmt_drv_dump.op);
		ret = -EFAULT;
	}

	return (ret);
}

/*
 * IOCTL entry point.
 */
static int
bnxt_mgmt_ioctl(struct cdev *dev, u_long cmd, caddr_t data, int flag,
		struct thread *td)
{
	int ret = 0;

	switch(cmd) {
	case IO_BNXT_MGMT_OPCODE_GET_DEV_INFO:
	case IOW_BNXT_MGMT_OPCODE_GET_DEV_INFO:
		ret = bnxt_mgmt_get_dev_info(dev, cmd, data, flag, td);
		break;
	case IO_BNXT_MGMT_OPCODE_PASSTHROUGH_HWRM:
	case IOW_BNXT_MGMT_OPCODE_PASSTHROUGH_HWRM:
		mtx_lock(&mgmt_lock);
		ret = bnxt_mgmt_process_hwrm(dev, cmd, data, flag, td);
		mtx_unlock(&mgmt_lock);
		break;
	case IO_BNXT_MGMT_OPCODE_DCB_OPS:
	case IOW_BNXT_MGMT_OPCODE_DCB_OPS:
		ret = bnxt_mgmt_process_dcb(dev, cmd, data, flag, td);
		break;
	case IO_BNXT_MGMT_OPCODE_DRV_DUMP:
	case IOW_BNXT_MGMT_OPCODE_DRV_DUMP:
		ret = bnxt_mgmt_drv_dump(dev, cmd, data, flag, td);
		break;
	case IO_BNXT_MGMT_OPCODE_CRASH_DUMP:
	case IOW_BNXT_MGMT_OPCODE_CRASH_DUMP:
		ret = bnxt_mgmt_crash_dump(dev, cmd, data, flag, td);
		break;
	default:
		printf("%s: Unknown command 0x%lx\n", DRIVER_NAME, cmd);
		ret = -EINVAL;
		break;
	}

	return (ret);
}

static int
bnxt_mgmt_close(struct cdev *dev, int flags, int devtype, struct thread *td)
{
	return (0);
}

static int
bnxt_mgmt_open(struct cdev *dev, int flags, int devtype, struct thread *td)
{
	return (0);
}

DEV_MODULE(bnxt_mgmt, bnxt_mgmt_loader, NULL);

