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

#ifndef CAM_NVME_NVME_ALL_H
#define CAM_NVME_NVME_ALL_H 1

#include <dev/nvme/nvme.h>

struct ccb_nvmeio;

__BEGIN_DECLS
void	nvme_ns_cmd(struct ccb_nvmeio *nvmeio, uint8_t cmd, uint32_t nsid,
    uint32_t cdw10, uint32_t cdw11, uint32_t cdw12, uint32_t cdw13,
    uint32_t cdw14, uint32_t cdw15);

int	nvme_identify_match(caddr_t identbuffer, caddr_t table_entry);

struct sbuf;
void	nvme_print_ident(const struct nvme_controller_data *, const struct nvme_namespace_data *, struct sbuf *);
void	nvme_print_ident_short(const struct nvme_controller_data *,
    const struct nvme_namespace_data *, struct sbuf *);
void nvme_cmd_sbuf(const struct nvme_command *, struct sbuf *sb);
int nvme_command_sbuf(struct ccb_nvmeio *nvmeio, struct sbuf *sb);
void nvme_resv_report(struct ccb_nvmeio *nvmeio, uint32_t retries,
		      void (*cbfcnp)(struct cam_periph *, union ccb *),
		      uint32_t nsid, uint32_t report_type, uint8_t *data_ptr,
		      uint32_t dxfer_len, uint32_t timeout);
void nvme_resv_register(struct ccb_nvmeio *nvmeio, uint32_t retries,
			void (*cbfcnp)(struct cam_periph *, union ccb *),
			uint32_t nsid, int cptpl, int iekey, int rrega,
			uint8_t *data_ptr, uint32_t dxfer_len,uint32_t timeout);
void nvme_resv_acquire(struct ccb_nvmeio *nvmeio, uint32_t retries,
		       void (*cbfcnp)(struct cam_periph *, union ccb *),
		       uint32_t nsid, int rtype, int iekey, int racqa,
		       uint8_t *data_ptr, uint32_t dxfer_len, uint32_t timeout);
void nvme_resv_release(struct ccb_nvmeio *nvmeio, uint32_t retries,
		       void (*cbfcnp)(struct cam_periph *, union ccb *),
		       uint32_t nsid, int rtype, int iekey, int rrela,
		       uint8_t *data_ptr, uint32_t dxfer_len, uint32_t timeout);
void nvme_format(struct ccb_nvmeio *nvmeio, uint32_t retries,
		 void (*cbfcnp)(struct cam_periph *, union ccb *),
		 uint32_t nsid, int ses, int pil, int pi, int mset, int lbaf,
		 uint32_t timeout);
void nvme_get_feature(struct ccb_nvmeio *nvmeio, uint32_t retries,
		      void (*cbfcnp)(struct cam_periph *, union ccb *),
		      uint32_t nsid, int sel, uint8_t feature, uint32_t cdw11,
		      int uuid_idx, uint8_t *data_ptr, uint32_t dxfer_len,
		      uint32_t timeout);
void nvme_set_feature(struct ccb_nvmeio *nvmeio, uint32_t retries,
		      void (*cbfcnp)(struct cam_periph *, union ccb *),
		      uint32_t nsid, int sv, uint8_t feature, uint32_t cdw11,
		      uint32_t cdw12, uint32_t cdw13, uint32_t cdw14,
		      uint32_t cdw15, uint8_t *data_ptr, uint32_t dxfer_len,
		      uint32_t timeout);
void nvme_get_feature_hostid(struct ccb_nvmeio *nvmeio, uint32_t retries,
			     void (*cbfcnp)(struct cam_periph *, union ccb *),
			     int sel, int exhid, int uuid_idx,
			     uint8_t *data_ptr, uint32_t dxfer_len,
			     uint32_t timeout);
void nvme_set_feature_hostid(struct ccb_nvmeio *nvmeio, uint32_t retries,
			     void (*cbfcnp)(struct cam_periph *, union ccb *),
			     int sv, int exhid, uint8_t *data_ptr,
			     uint32_t dxfer_len, uint32_t timeout);
const char *nvme_command_string(struct ccb_nvmeio *nvmeio, char *, size_t);
int nvme_status_sbuf(struct ccb_nvmeio *nvmeio, struct sbuf *sb);
const void *nvme_get_identify_cntrl(struct cam_periph *);
const void *nvme_get_identify_ns(struct cam_periph *);
__END_DECLS

#endif /* CAM_NVME_NVME_ALL_H */
