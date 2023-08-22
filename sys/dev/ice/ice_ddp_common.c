/* SPDX-License-Identifier: BSD-3-Clause */
/*  Copyright (c) 2023, Intel Corporation
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 *   3. Neither the name of the Intel Corporation nor the names of its
 *      contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

#include "ice_ddp_common.h"
#include "ice_type.h"
#include "ice_common.h"
#include "ice_sched.h"

/**
 * ice_aq_download_pkg
 * @hw: pointer to the hardware structure
 * @pkg_buf: the package buffer to transfer
 * @buf_size: the size of the package buffer
 * @last_buf: last buffer indicator
 * @error_offset: returns error offset
 * @error_info: returns error information
 * @cd: pointer to command details structure or NULL
 *
 * Download Package (0x0C40)
 */
static enum ice_status
ice_aq_download_pkg(struct ice_hw *hw, struct ice_buf_hdr *pkg_buf,
		    u16 buf_size, bool last_buf, u32 *error_offset,
		    u32 *error_info, struct ice_sq_cd *cd)
{
	struct ice_aqc_download_pkg *cmd;
	struct ice_aq_desc desc;
	enum ice_status status;

	if (error_offset)
		*error_offset = 0;
	if (error_info)
		*error_info = 0;

	cmd = &desc.params.download_pkg;
	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_download_pkg);
	desc.flags |= CPU_TO_LE16(ICE_AQ_FLAG_RD);

	if (last_buf)
		cmd->flags |= ICE_AQC_DOWNLOAD_PKG_LAST_BUF;

	status = ice_aq_send_cmd(hw, &desc, pkg_buf, buf_size, cd);
	if (status == ICE_ERR_AQ_ERROR) {
		/* Read error from buffer only when the FW returned an error */
		struct ice_aqc_download_pkg_resp *resp;

		resp = (struct ice_aqc_download_pkg_resp *)pkg_buf;
		if (error_offset)
			*error_offset = LE32_TO_CPU(resp->error_offset);
		if (error_info)
			*error_info = LE32_TO_CPU(resp->error_info);
	}

	return status;
}

/**
 * ice_aq_upload_section
 * @hw: pointer to the hardware structure
 * @pkg_buf: the package buffer which will receive the section
 * @buf_size: the size of the package buffer
 * @cd: pointer to command details structure or NULL
 *
 * Upload Section (0x0C41)
 */
enum ice_status
ice_aq_upload_section(struct ice_hw *hw, struct ice_buf_hdr *pkg_buf,
		      u16 buf_size, struct ice_sq_cd *cd)
{
	struct ice_aq_desc desc;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_upload_section);
	desc.flags |= CPU_TO_LE16(ICE_AQ_FLAG_RD);

	return ice_aq_send_cmd(hw, &desc, pkg_buf, buf_size, cd);
}

/**
 * ice_aq_update_pkg
 * @hw: pointer to the hardware structure
 * @pkg_buf: the package cmd buffer
 * @buf_size: the size of the package cmd buffer
 * @last_buf: last buffer indicator
 * @error_offset: returns error offset
 * @error_info: returns error information
 * @cd: pointer to command details structure or NULL
 *
 * Update Package (0x0C42)
 */
static enum ice_status
ice_aq_update_pkg(struct ice_hw *hw, struct ice_buf_hdr *pkg_buf, u16 buf_size,
		  bool last_buf, u32 *error_offset, u32 *error_info,
		  struct ice_sq_cd *cd)
{
	struct ice_aqc_download_pkg *cmd;
	struct ice_aq_desc desc;
	enum ice_status status;

	if (error_offset)
		*error_offset = 0;
	if (error_info)
		*error_info = 0;

	cmd = &desc.params.download_pkg;
	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_update_pkg);
	desc.flags |= CPU_TO_LE16(ICE_AQ_FLAG_RD);

	if (last_buf)
		cmd->flags |= ICE_AQC_DOWNLOAD_PKG_LAST_BUF;

	status = ice_aq_send_cmd(hw, &desc, pkg_buf, buf_size, cd);
	if (status == ICE_ERR_AQ_ERROR) {
		/* Read error from buffer only when the FW returned an error */
		struct ice_aqc_download_pkg_resp *resp;

		resp = (struct ice_aqc_download_pkg_resp *)pkg_buf;
		if (error_offset)
			*error_offset = LE32_TO_CPU(resp->error_offset);
		if (error_info)
			*error_info = LE32_TO_CPU(resp->error_info);
	}

	return status;
}

/**
 * ice_find_seg_in_pkg
 * @hw: pointer to the hardware structure
 * @seg_type: the segment type to search for (i.e., SEGMENT_TYPE_CPK)
 * @pkg_hdr: pointer to the package header to be searched
 *
 * This function searches a package file for a particular segment type. On
 * success it returns a pointer to the segment header, otherwise it will
 * return NULL.
 */
struct ice_generic_seg_hdr *
ice_find_seg_in_pkg(struct ice_hw *hw, u32 seg_type,
		    struct ice_pkg_hdr *pkg_hdr)
{
	u32 i;

	ice_debug(hw, ICE_DBG_PKG, "Package format version: %d.%d.%d.%d\n",
		  pkg_hdr->pkg_format_ver.major, pkg_hdr->pkg_format_ver.minor,
		  pkg_hdr->pkg_format_ver.update,
		  pkg_hdr->pkg_format_ver.draft);

	/* Search all package segments for the requested segment type */
	for (i = 0; i < LE32_TO_CPU(pkg_hdr->seg_count); i++) {
		struct ice_generic_seg_hdr *seg;

		seg = (struct ice_generic_seg_hdr *)
			((u8 *)pkg_hdr + LE32_TO_CPU(pkg_hdr->seg_offset[i]));

		if (LE32_TO_CPU(seg->seg_type) == seg_type)
			return seg;
	}

	return NULL;
}

/**
 * ice_get_pkg_seg_by_idx
 * @pkg_hdr: pointer to the package header to be searched
 * @idx: index of segment
 */
static struct ice_generic_seg_hdr *
ice_get_pkg_seg_by_idx(struct ice_pkg_hdr *pkg_hdr, u32 idx)
{
	struct ice_generic_seg_hdr *seg = NULL;

	if (idx < LE32_TO_CPU(pkg_hdr->seg_count))
		seg = (struct ice_generic_seg_hdr *)
			((u8 *)pkg_hdr +
			 LE32_TO_CPU(pkg_hdr->seg_offset[idx]));

	return seg;
}

/**
 * ice_is_signing_seg_at_idx - determine if segment is a signing segment
 * @pkg_hdr: pointer to package header
 * @idx: segment index
 */
static bool ice_is_signing_seg_at_idx(struct ice_pkg_hdr *pkg_hdr, u32 idx)
{
	struct ice_generic_seg_hdr *seg;
	bool retval = false;

	seg = ice_get_pkg_seg_by_idx(pkg_hdr, idx);
	if (seg)
		retval = LE32_TO_CPU(seg->seg_type) == SEGMENT_TYPE_SIGNING;

	return retval;
}

/**
 * ice_is_signing_seg_type_at_idx
 * @pkg_hdr: pointer to package header
 * @idx: segment index
 * @seg_id: segment id that is expected
 * @sign_type: signing type
 *
 * Determine if a segment is a signing segment of the correct type
 */
static bool
ice_is_signing_seg_type_at_idx(struct ice_pkg_hdr *pkg_hdr, u32 idx,
			       u32 seg_id, u32 sign_type)
{
	bool result = false;

	if (ice_is_signing_seg_at_idx(pkg_hdr, idx)) {
		struct ice_sign_seg *seg;

		seg = (struct ice_sign_seg *)ice_get_pkg_seg_by_idx(pkg_hdr,
								    idx);
		if (seg && LE32_TO_CPU(seg->seg_id) == seg_id &&
		    LE32_TO_CPU(seg->sign_type) == sign_type)
			result = true;
	}

	return result;
}

/**
 * ice_update_pkg_no_lock
 * @hw: pointer to the hardware structure
 * @bufs: pointer to an array of buffers
 * @count: the number of buffers in the array
 */
enum ice_status
ice_update_pkg_no_lock(struct ice_hw *hw, struct ice_buf *bufs, u32 count)
{
	enum ice_status status = ICE_SUCCESS;
	u32 i;

	for (i = 0; i < count; i++) {
		struct ice_buf_hdr *bh = (struct ice_buf_hdr *)(bufs + i);
		bool last = ((i + 1) == count);
		u32 offset, info;

		status = ice_aq_update_pkg(hw, bh, LE16_TO_CPU(bh->data_end),
					   last, &offset, &info, NULL);

		if (status) {
			ice_debug(hw, ICE_DBG_PKG, "Update pkg failed: err %d off %d inf %d\n",
				  status, offset, info);
			break;
		}
	}

	return status;
}

/**
 * ice_update_pkg
 * @hw: pointer to the hardware structure
 * @bufs: pointer to an array of buffers
 * @count: the number of buffers in the array
 *
 * Obtains change lock and updates package.
 */
enum ice_status
ice_update_pkg(struct ice_hw *hw, struct ice_buf *bufs, u32 count)
{
	enum ice_status status;

	status = ice_acquire_change_lock(hw, ICE_RES_WRITE);
	if (status)
		return status;

	status = ice_update_pkg_no_lock(hw, bufs, count);

	ice_release_change_lock(hw);

	return status;
}

static enum ice_ddp_state
ice_map_aq_err_to_ddp_state(enum ice_aq_err aq_err)
{
	switch (aq_err) {
	case ICE_AQ_RC_ENOSEC:
		return ICE_DDP_PKG_NO_SEC_MANIFEST;
	case ICE_AQ_RC_EBADSIG:
		return ICE_DDP_PKG_FILE_SIGNATURE_INVALID;
	case ICE_AQ_RC_ESVN:
		return ICE_DDP_PKG_SECURE_VERSION_NBR_TOO_LOW;
	case ICE_AQ_RC_EBADMAN:
		return ICE_DDP_PKG_MANIFEST_INVALID;
	case ICE_AQ_RC_EBADBUF:
		return ICE_DDP_PKG_BUFFER_INVALID;
	default:
		return ICE_DDP_PKG_ERR;
	}
}

/**
 * ice_is_buffer_metadata - determine if package buffer is a metadata buffer
 * @buf: pointer to buffer header
 */
static bool ice_is_buffer_metadata(struct ice_buf_hdr *buf)
{
	bool metadata = false;

	if (LE32_TO_CPU(buf->section_entry[0].type) & ICE_METADATA_BUF)
		metadata = true;

	return metadata;
}

/**
 * ice_is_last_download_buffer
 * @buf: pointer to current buffer header
 * @idx: index of the buffer in the current sequence
 * @count: the buffer count in the current sequence
 *
 * Note: this routine should only be called if the buffer is not the last buffer
 */
static bool
ice_is_last_download_buffer(struct ice_buf_hdr *buf, u32 idx, u32 count)
{
	bool last = ((idx + 1) == count);

	/* A set metadata flag in the next buffer will signal that the current
	 * buffer will be the last buffer downloaded
	 */
	if (!last) {
		struct ice_buf *next_buf = ((struct ice_buf *)buf) + 1;

		last = ice_is_buffer_metadata((struct ice_buf_hdr *)next_buf);
	}

	return last;
}

/**
 * ice_dwnld_cfg_bufs_no_lock
 * @hw: pointer to the hardware structure
 * @bufs: pointer to an array of buffers
 * @start: buffer index of first buffer to download
 * @count: the number of buffers to download
 * @indicate_last: if true, then set last buffer flag on last buffer download
 *
 * Downloads package configuration buffers to the firmware. Metadata buffers
 * are skipped, and the first metadata buffer found indicates that the rest
 * of the buffers are all metadata buffers.
 */
static enum ice_ddp_state
ice_dwnld_cfg_bufs_no_lock(struct ice_hw *hw, struct ice_buf *bufs, u32 start,
			   u32 count, bool indicate_last)
{
	enum ice_ddp_state state = ICE_DDP_PKG_SUCCESS;
	struct ice_buf_hdr *bh;
	enum ice_aq_err err;
	u32 offset, info, i;

	if (!bufs || !count)
		return ICE_DDP_PKG_ERR;

	/* If the first buffer's first section has its metadata bit set
	 * then there are no buffers to be downloaded, and the operation is
	 * considered a success.
	 */
	bh = (struct ice_buf_hdr *)(bufs + start);
	if (LE32_TO_CPU(bh->section_entry[0].type) & ICE_METADATA_BUF)
		return ICE_DDP_PKG_SUCCESS;

	for (i = 0; i < count; i++) {
		enum ice_status status;
		bool last = false;

		bh = (struct ice_buf_hdr *)(bufs + start + i);

		if (indicate_last)
			last = ice_is_last_download_buffer(bh, i, count);

		status = ice_aq_download_pkg(hw, bh, ICE_PKG_BUF_SIZE, last,
					     &offset, &info, NULL);

		/* Save AQ status from download package */
		if (status) {
			ice_debug(hw, ICE_DBG_PKG, "Pkg download failed: err %d off %d inf %d\n",
				  status, offset, info);
			err = hw->adminq.sq_last_status;
			state = ice_map_aq_err_to_ddp_state(err);
			break;
		}

		if (last)
			break;
	}

	return state;
}

/**
 * ice_aq_get_pkg_info_list
 * @hw: pointer to the hardware structure
 * @pkg_info: the buffer which will receive the information list
 * @buf_size: the size of the pkg_info information buffer
 * @cd: pointer to command details structure or NULL
 *
 * Get Package Info List (0x0C43)
 */
static enum ice_status
ice_aq_get_pkg_info_list(struct ice_hw *hw,
			 struct ice_aqc_get_pkg_info_resp *pkg_info,
			 u16 buf_size, struct ice_sq_cd *cd)
{
	struct ice_aq_desc desc;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_get_pkg_info_list);

	return ice_aq_send_cmd(hw, &desc, pkg_info, buf_size, cd);
}

/**
 * ice_has_signing_seg - determine if package has a signing segment
 * @hw: pointer to the hardware structure
 * @pkg_hdr: pointer to the driver's package hdr
 */
static bool ice_has_signing_seg(struct ice_hw *hw, struct ice_pkg_hdr *pkg_hdr)
{
	struct ice_generic_seg_hdr *seg_hdr;

	seg_hdr = (struct ice_generic_seg_hdr *)
		ice_find_seg_in_pkg(hw, SEGMENT_TYPE_SIGNING, pkg_hdr);

	return seg_hdr ? true : false;
}

/**
 * ice_get_pkg_segment_id - get correct package segment id, based on device
 * @mac_type: MAC type of the device
 */
static u32 ice_get_pkg_segment_id(enum ice_mac_type mac_type)
{
	u32 seg_id;

	switch (mac_type) {
	case ICE_MAC_GENERIC:
	case ICE_MAC_GENERIC_3K:
	default:
		seg_id = SEGMENT_TYPE_ICE_E810;
		break;
	}

	return seg_id;
}

/**
 * ice_get_pkg_sign_type - get package segment sign type, based on device
 * @mac_type: MAC type of the device
 */
static u32 ice_get_pkg_sign_type(enum ice_mac_type mac_type)
{
	u32 sign_type;

	switch (mac_type) {
	case ICE_MAC_GENERIC_3K:
		sign_type = SEGMENT_SIGN_TYPE_RSA3K;
		break;
	case ICE_MAC_GENERIC:
	default:
		sign_type = SEGMENT_SIGN_TYPE_RSA2K;
		break;
	}

	return sign_type;
}

/**
 * ice_get_signing_req - get correct package requirements, based on device
 * @hw: pointer to the hardware structure
 */
static void ice_get_signing_req(struct ice_hw *hw)
{
	hw->pkg_seg_id = ice_get_pkg_segment_id(hw->mac_type);
	hw->pkg_sign_type = ice_get_pkg_sign_type(hw->mac_type);
}

/**
 * ice_download_pkg_sig_seg - download a signature segment
 * @hw: pointer to the hardware structure
 * @seg: pointer to signature segment
 */
static enum ice_ddp_state
ice_download_pkg_sig_seg(struct ice_hw *hw, struct ice_sign_seg *seg)
{
	enum ice_ddp_state state;

	state = ice_dwnld_cfg_bufs_no_lock(hw, seg->buf_tbl.buf_array, 0,
					   LE32_TO_CPU(seg->buf_tbl.buf_count),
					   false);

	return state;
}

/**
 * ice_download_pkg_config_seg - download a config segment
 * @hw: pointer to the hardware structure
 * @pkg_hdr: pointer to package header
 * @idx: segment index
 * @start: starting buffer
 * @count: buffer count
 *
 * Note: idx must reference a ICE segment
 */
static enum ice_ddp_state
ice_download_pkg_config_seg(struct ice_hw *hw, struct ice_pkg_hdr *pkg_hdr,
			    u32 idx, u32 start, u32 count)
{
	struct ice_buf_table *bufs;
	enum ice_ddp_state state;
	struct ice_seg *seg;
	u32 buf_count;

	seg = (struct ice_seg *)ice_get_pkg_seg_by_idx(pkg_hdr, idx);
	if (!seg)
		return ICE_DDP_PKG_ERR;

	bufs = ice_find_buf_table(seg);
	buf_count = LE32_TO_CPU(bufs->buf_count);

	if (start >= buf_count || start + count > buf_count)
		return ICE_DDP_PKG_ERR;

	state = ice_dwnld_cfg_bufs_no_lock(hw, bufs->buf_array, start, count,
					   true);

	return state;
}

/**
 * ice_dwnld_sign_and_cfg_segs - download a signing segment and config segment
 * @hw: pointer to the hardware structure
 * @pkg_hdr: pointer to package header
 * @idx: segment index (must be a signature segment)
 *
 * Note: idx must reference a signature segment
 */
static enum ice_ddp_state
ice_dwnld_sign_and_cfg_segs(struct ice_hw *hw, struct ice_pkg_hdr *pkg_hdr,
			    u32 idx)
{
	enum ice_ddp_state state;
	struct ice_sign_seg *seg;
	u32 conf_idx;
	u32 start;
	u32 count;

	seg = (struct ice_sign_seg *)ice_get_pkg_seg_by_idx(pkg_hdr, idx);
	if (!seg) {
		state = ICE_DDP_PKG_ERR;
		goto exit;
	}

	conf_idx = LE32_TO_CPU(seg->signed_seg_idx);
	start = LE32_TO_CPU(seg->signed_buf_start);
	count = LE32_TO_CPU(seg->signed_buf_count);

	state = ice_download_pkg_sig_seg(hw, seg);
	if (state)
		goto exit;

	state = ice_download_pkg_config_seg(hw, pkg_hdr, conf_idx, start,
					    count);

exit:
	return state;
}

/**
 * ice_match_signing_seg - determine if a matching signing segment exists
 * @pkg_hdr: pointer to package header
 * @seg_id: segment id that is expected
 * @sign_type: signing type
 */
static bool
ice_match_signing_seg(struct ice_pkg_hdr *pkg_hdr, u32 seg_id, u32 sign_type)
{
	bool match = false;
	u32 i;

	for (i = 0; i < LE32_TO_CPU(pkg_hdr->seg_count); i++) {
		if (ice_is_signing_seg_type_at_idx(pkg_hdr, i, seg_id,
						   sign_type)) {
			match = true;
			break;
		}
	}

	return match;
}

/**
 * ice_post_dwnld_pkg_actions - perform post download package actions
 * @hw: pointer to the hardware structure
 */
static enum ice_ddp_state
ice_post_dwnld_pkg_actions(struct ice_hw *hw)
{
	enum ice_ddp_state state = ICE_DDP_PKG_SUCCESS;
	enum ice_status status;

	status = ice_set_vlan_mode(hw);
	if (status) {
		ice_debug(hw, ICE_DBG_PKG, "Failed to set VLAN mode: err %d\n",
			  status);
		state = ICE_DDP_PKG_ERR;
	}

	return state;
}

/**
 * ice_download_pkg_with_sig_seg - download package using signature segments
 * @hw: pointer to the hardware structure
 * @pkg_hdr: pointer to package header
 */
static enum ice_ddp_state
ice_download_pkg_with_sig_seg(struct ice_hw *hw, struct ice_pkg_hdr *pkg_hdr)
{
	enum ice_aq_err aq_err = hw->adminq.sq_last_status;
	enum ice_ddp_state state = ICE_DDP_PKG_ERR;
	enum ice_status status;
	u32 i;

	ice_debug(hw, ICE_DBG_INIT, "Segment ID %d\n", hw->pkg_seg_id);
	ice_debug(hw, ICE_DBG_INIT, "Signature type %d\n", hw->pkg_sign_type);

	status = ice_acquire_global_cfg_lock(hw, ICE_RES_WRITE);
	if (status) {
		if (status == ICE_ERR_AQ_NO_WORK)
			state = ICE_DDP_PKG_ALREADY_LOADED;
		else
			state = ice_map_aq_err_to_ddp_state(aq_err);
		return state;
	}

	for (i = 0; i < LE32_TO_CPU(pkg_hdr->seg_count); i++) {
		if (!ice_is_signing_seg_type_at_idx(pkg_hdr, i, hw->pkg_seg_id,
						    hw->pkg_sign_type))
			continue;

		state = ice_dwnld_sign_and_cfg_segs(hw, pkg_hdr, i);
		if (state)
			break;
	}

	if (!state)
		state = ice_post_dwnld_pkg_actions(hw);

	ice_release_global_cfg_lock(hw);

	return state;
}

/**
 * ice_dwnld_cfg_bufs
 * @hw: pointer to the hardware structure
 * @bufs: pointer to an array of buffers
 * @count: the number of buffers in the array
 *
 * Obtains global config lock and downloads the package configuration buffers
 * to the firmware.
 */
static enum ice_ddp_state
ice_dwnld_cfg_bufs(struct ice_hw *hw, struct ice_buf *bufs, u32 count)
{
	enum ice_ddp_state state = ICE_DDP_PKG_SUCCESS;
	enum ice_status status;
	struct ice_buf_hdr *bh;

	if (!bufs || !count)
		return ICE_DDP_PKG_ERR;

	/* If the first buffer's first section has its metadata bit set
	 * then there are no buffers to be downloaded, and the operation is
	 * considered a success.
	 */
	bh = (struct ice_buf_hdr *)bufs;
	if (LE32_TO_CPU(bh->section_entry[0].type) & ICE_METADATA_BUF)
		return ICE_DDP_PKG_SUCCESS;

	status = ice_acquire_global_cfg_lock(hw, ICE_RES_WRITE);
	if (status) {
		if (status == ICE_ERR_AQ_NO_WORK)
			return ICE_DDP_PKG_ALREADY_LOADED;
		return ice_map_aq_err_to_ddp_state(hw->adminq.sq_last_status);
	}

	state = ice_dwnld_cfg_bufs_no_lock(hw, bufs, 0, count, true);
	if (!state)
		state = ice_post_dwnld_pkg_actions(hw);

	ice_release_global_cfg_lock(hw);

	return state;
}

/**
 * ice_download_pkg_without_sig_seg
 * @hw: pointer to the hardware structure
 * @ice_seg: pointer to the segment of the package to be downloaded
 *
 * Handles the download of a complete package without signature segment.
 */
static enum ice_ddp_state
ice_download_pkg_without_sig_seg(struct ice_hw *hw, struct ice_seg *ice_seg)
{
	struct ice_buf_table *ice_buf_tbl;
	enum ice_ddp_state state;

	ice_debug(hw, ICE_DBG_PKG, "Segment format version: %d.%d.%d.%d\n",
		  ice_seg->hdr.seg_format_ver.major,
		  ice_seg->hdr.seg_format_ver.minor,
		  ice_seg->hdr.seg_format_ver.update,
		  ice_seg->hdr.seg_format_ver.draft);

	ice_debug(hw, ICE_DBG_PKG, "Seg: type 0x%X, size %d, name %s\n",
		  LE32_TO_CPU(ice_seg->hdr.seg_type),
		  LE32_TO_CPU(ice_seg->hdr.seg_size), ice_seg->hdr.seg_id);

	ice_buf_tbl = ice_find_buf_table(ice_seg);

	ice_debug(hw, ICE_DBG_PKG, "Seg buf count: %d\n",
		  LE32_TO_CPU(ice_buf_tbl->buf_count));

	state = ice_dwnld_cfg_bufs(hw, ice_buf_tbl->buf_array,
				   LE32_TO_CPU(ice_buf_tbl->buf_count));

	return state;
}

/**
 * ice_download_pkg
 * @hw: pointer to the hardware structure
 * @pkg_hdr: pointer to package header
 * @ice_seg: pointer to the segment of the package to be downloaded
 *
 * Handles the download of a complete package.
 */
static enum ice_ddp_state
ice_download_pkg(struct ice_hw *hw, struct ice_pkg_hdr *pkg_hdr,
		 struct ice_seg *ice_seg)
{
	enum ice_ddp_state state;

	if (hw->pkg_has_signing_seg)
		state = ice_download_pkg_with_sig_seg(hw, pkg_hdr);
	else
		state = ice_download_pkg_without_sig_seg(hw, ice_seg);

	ice_post_pkg_dwnld_vlan_mode_cfg(hw);

	return state;
}

/**
 * ice_init_pkg_info
 * @hw: pointer to the hardware structure
 * @pkg_hdr: pointer to the driver's package hdr
 *
 * Saves off the package details into the HW structure.
 */
static enum ice_ddp_state
ice_init_pkg_info(struct ice_hw *hw, struct ice_pkg_hdr *pkg_hdr)
{
	struct ice_generic_seg_hdr *seg_hdr;

	if (!pkg_hdr)
		return ICE_DDP_PKG_ERR;

	hw->pkg_has_signing_seg = ice_has_signing_seg(hw, pkg_hdr);
	ice_get_signing_req(hw);

	ice_debug(hw, ICE_DBG_INIT, "Pkg using segment id: 0x%08X\n",
		  hw->pkg_seg_id);

	seg_hdr = (struct ice_generic_seg_hdr *)
		ice_find_seg_in_pkg(hw, hw->pkg_seg_id, pkg_hdr);
	if (seg_hdr) {
		struct ice_meta_sect *meta;
		struct ice_pkg_enum state;

		ice_memset(&state, 0, sizeof(state), ICE_NONDMA_MEM);

		/* Get package information from the Metadata Section */
		meta = (struct ice_meta_sect *)
			ice_pkg_enum_section((struct ice_seg *)seg_hdr, &state,
					     ICE_SID_METADATA);
		if (!meta) {
			ice_debug(hw, ICE_DBG_INIT, "Did not find ice metadata section in package\n");
			return ICE_DDP_PKG_INVALID_FILE;
		}

		hw->pkg_ver = meta->ver;
		ice_memcpy(hw->pkg_name, meta->name, sizeof(meta->name),
			   ICE_NONDMA_TO_NONDMA);

		ice_debug(hw, ICE_DBG_PKG, "Pkg: %d.%d.%d.%d, %s\n",
			  meta->ver.major, meta->ver.minor, meta->ver.update,
			  meta->ver.draft, meta->name);

		hw->ice_seg_fmt_ver = seg_hdr->seg_format_ver;
		ice_memcpy(hw->ice_seg_id, seg_hdr->seg_id,
			   sizeof(hw->ice_seg_id), ICE_NONDMA_TO_NONDMA);

		ice_debug(hw, ICE_DBG_PKG, "Ice Seg: %d.%d.%d.%d, %s\n",
			  seg_hdr->seg_format_ver.major,
			  seg_hdr->seg_format_ver.minor,
			  seg_hdr->seg_format_ver.update,
			  seg_hdr->seg_format_ver.draft,
			  seg_hdr->seg_id);
	} else {
		ice_debug(hw, ICE_DBG_INIT, "Did not find ice segment in driver package\n");
		return ICE_DDP_PKG_INVALID_FILE;
	}

	return ICE_DDP_PKG_SUCCESS;
}

/**
 * ice_get_pkg_info
 * @hw: pointer to the hardware structure
 *
 * Store details of the package currently loaded in HW into the HW structure.
 */
enum ice_ddp_state ice_get_pkg_info(struct ice_hw *hw)
{
	enum ice_ddp_state state = ICE_DDP_PKG_SUCCESS;
	struct ice_aqc_get_pkg_info_resp *pkg_info;
	u16 size;
	u32 i;

	size = ice_struct_size(pkg_info, pkg_info, ICE_PKG_CNT);
	pkg_info = (struct ice_aqc_get_pkg_info_resp *)ice_malloc(hw, size);
	if (!pkg_info)
		return ICE_DDP_PKG_ERR;

	if (ice_aq_get_pkg_info_list(hw, pkg_info, size, NULL)) {
		state = ICE_DDP_PKG_ERR;
		goto init_pkg_free_alloc;
	}

	for (i = 0; i < LE32_TO_CPU(pkg_info->count); i++) {
#define ICE_PKG_FLAG_COUNT	4
		char flags[ICE_PKG_FLAG_COUNT + 1] = { 0 };
		u8 place = 0;

		if (pkg_info->pkg_info[i].is_active) {
			flags[place++] = 'A';
			hw->active_pkg_ver = pkg_info->pkg_info[i].ver;
			hw->active_track_id =
				LE32_TO_CPU(pkg_info->pkg_info[i].track_id);
			ice_memcpy(hw->active_pkg_name,
				   pkg_info->pkg_info[i].name,
				   sizeof(pkg_info->pkg_info[i].name),
				   ICE_NONDMA_TO_NONDMA);
			hw->active_pkg_in_nvm = pkg_info->pkg_info[i].is_in_nvm;
		}
		if (pkg_info->pkg_info[i].is_active_at_boot)
			flags[place++] = 'B';
		if (pkg_info->pkg_info[i].is_modified)
			flags[place++] = 'M';
		if (pkg_info->pkg_info[i].is_in_nvm)
			flags[place++] = 'N';

		ice_debug(hw, ICE_DBG_PKG, "Pkg[%d]: %d.%d.%d.%d,%s,%s\n",
			  i, pkg_info->pkg_info[i].ver.major,
			  pkg_info->pkg_info[i].ver.minor,
			  pkg_info->pkg_info[i].ver.update,
			  pkg_info->pkg_info[i].ver.draft,
			  pkg_info->pkg_info[i].name, flags);
	}

init_pkg_free_alloc:
	ice_free(hw, pkg_info);

	return state;
}

/**
 * ice_label_enum_handler
 * @sect_type: section type
 * @section: pointer to section
 * @index: index of the label entry to be returned
 * @offset: pointer to receive absolute offset, always zero for label sections
 *
 * This is a callback function that can be passed to ice_pkg_enum_entry.
 * Handles enumeration of individual label entries.
 */
static void *
ice_label_enum_handler(u32 __ALWAYS_UNUSED sect_type, void *section, u32 index,
		       u32 *offset)
{
	struct ice_label_section *labels;

	if (!section)
		return NULL;

	if (index > ICE_MAX_LABELS_IN_BUF)
		return NULL;

	if (offset)
		*offset = 0;

	labels = (struct ice_label_section *)section;
	if (index >= LE16_TO_CPU(labels->count))
		return NULL;

	return labels->label + index;
}

/**
 * ice_enum_labels
 * @ice_seg: pointer to the ice segment (NULL on subsequent calls)
 * @type: the section type that will contain the label (0 on subsequent calls)
 * @state: ice_pkg_enum structure that will hold the state of the enumeration
 * @value: pointer to a value that will return the label's value if found
 *
 * Enumerates a list of labels in the package. The caller will call
 * ice_enum_labels(ice_seg, type, ...) to start the enumeration, then call
 * ice_enum_labels(NULL, 0, ...) to continue. When the function returns a NULL
 * the end of the list has been reached.
 */
static char *
ice_enum_labels(struct ice_seg *ice_seg, u32 type, struct ice_pkg_enum *state,
		u16 *value)
{
	struct ice_label *label;

	/* Check for valid label section on first call */
	if (type && !(type >= ICE_SID_LBL_FIRST && type <= ICE_SID_LBL_LAST))
		return NULL;

	label = (struct ice_label *)ice_pkg_enum_entry(ice_seg, state, type,
						       NULL,
						       ice_label_enum_handler);
	if (!label)
		return NULL;

	*value = LE16_TO_CPU(label->value);
	return label->name;
}

/**
 * ice_find_label_value
 * @ice_seg: pointer to the ice segment (non-NULL)
 * @name: name of the label to search for
 * @type: the section type that will contain the label
 * @value: pointer to a value that will return the label's value if found
 *
 * Finds a label's value given the label name and the section type to search.
 * The ice_seg parameter must not be NULL since the first call to
 * ice_enum_labels requires a pointer to an actual ice_seg structure.
 */
enum ice_status
ice_find_label_value(struct ice_seg *ice_seg, char const *name, u32 type,
		     u16 *value)
{
	struct ice_pkg_enum state;
	char *label_name;
	u16 val;

	ice_memset(&state, 0, sizeof(state), ICE_NONDMA_MEM);

	if (!ice_seg)
		return ICE_ERR_PARAM;

	do {
		label_name = ice_enum_labels(ice_seg, type, &state, &val);
		if (label_name && !strcmp(label_name, name)) {
			*value = val;
			return ICE_SUCCESS;
		}

		ice_seg = NULL;
	} while (label_name);

	return ICE_ERR_CFG;
}

/**
 * ice_verify_pkg - verify package
 * @pkg: pointer to the package buffer
 * @len: size of the package buffer
 *
 * Verifies various attributes of the package file, including length, format
 * version, and the requirement of at least one segment.
 */
enum ice_ddp_state ice_verify_pkg(struct ice_pkg_hdr *pkg, u32 len)
{
	u32 seg_count;
	u32 i;

	if (len < ice_struct_size(pkg, seg_offset, 1))
		return ICE_DDP_PKG_INVALID_FILE;

	if (pkg->pkg_format_ver.major != ICE_PKG_FMT_VER_MAJ ||
	    pkg->pkg_format_ver.minor != ICE_PKG_FMT_VER_MNR ||
	    pkg->pkg_format_ver.update != ICE_PKG_FMT_VER_UPD ||
	    pkg->pkg_format_ver.draft != ICE_PKG_FMT_VER_DFT)
		return ICE_DDP_PKG_INVALID_FILE;

	/* pkg must have at least one segment */
	seg_count = LE32_TO_CPU(pkg->seg_count);
	if (seg_count < 1)
		return ICE_DDP_PKG_INVALID_FILE;

	/* make sure segment array fits in package length */
	if (len < ice_struct_size(pkg, seg_offset, seg_count))
		return ICE_DDP_PKG_INVALID_FILE;

	/* all segments must fit within length */
	for (i = 0; i < seg_count; i++) {
		u32 off = LE32_TO_CPU(pkg->seg_offset[i]);
		struct ice_generic_seg_hdr *seg;

		/* segment header must fit */
		if (len < off + sizeof(*seg))
			return ICE_DDP_PKG_INVALID_FILE;

		seg = (struct ice_generic_seg_hdr *)((u8 *)pkg + off);

		/* segment body must fit */
		if (len < off + LE32_TO_CPU(seg->seg_size))
			return ICE_DDP_PKG_INVALID_FILE;
	}

	return ICE_DDP_PKG_SUCCESS;
}

/**
 * ice_free_seg - free package segment pointer
 * @hw: pointer to the hardware structure
 *
 * Frees the package segment pointer in the proper manner, depending on if the
 * segment was allocated or just the passed in pointer was stored.
 */
void ice_free_seg(struct ice_hw *hw)
{
	if (hw->pkg_copy) {
		ice_free(hw, hw->pkg_copy);
		hw->pkg_copy = NULL;
		hw->pkg_size = 0;
	}
	hw->seg = NULL;
}

/**
 * ice_chk_pkg_version - check package version for compatibility with driver
 * @pkg_ver: pointer to a version structure to check
 *
 * Check to make sure that the package about to be downloaded is compatible with
 * the driver. To be compatible, the major and minor components of the package
 * version must match our ICE_PKG_SUPP_VER_MAJ and ICE_PKG_SUPP_VER_MNR
 * definitions.
 */
static enum ice_ddp_state ice_chk_pkg_version(struct ice_pkg_ver *pkg_ver)
{
	if (pkg_ver->major > ICE_PKG_SUPP_VER_MAJ ||
	    (pkg_ver->major == ICE_PKG_SUPP_VER_MAJ &&
	     pkg_ver->minor > ICE_PKG_SUPP_VER_MNR))
		return ICE_DDP_PKG_FILE_VERSION_TOO_HIGH;
	else if (pkg_ver->major < ICE_PKG_SUPP_VER_MAJ ||
		 (pkg_ver->major == ICE_PKG_SUPP_VER_MAJ &&
		  pkg_ver->minor < ICE_PKG_SUPP_VER_MNR))
		return ICE_DDP_PKG_FILE_VERSION_TOO_LOW;

	return ICE_DDP_PKG_SUCCESS;
}

/**
 * ice_chk_pkg_compat
 * @hw: pointer to the hardware structure
 * @ospkg: pointer to the package hdr
 * @seg: pointer to the package segment hdr
 *
 * This function checks the package version compatibility with driver and NVM
 */
static enum ice_ddp_state
ice_chk_pkg_compat(struct ice_hw *hw, struct ice_pkg_hdr *ospkg,
		   struct ice_seg **seg)
{
	struct ice_aqc_get_pkg_info_resp *pkg;
	enum ice_ddp_state state;
	u16 size;
	u32 i;

	/* Check package version compatibility */
	state = ice_chk_pkg_version(&hw->pkg_ver);
	if (state) {
		ice_debug(hw, ICE_DBG_INIT, "Package version check failed.\n");
		return state;
	}

	/* find ICE segment in given package */
	*seg = (struct ice_seg *)ice_find_seg_in_pkg(hw, hw->pkg_seg_id,
						     ospkg);
	if (!*seg) {
		ice_debug(hw, ICE_DBG_INIT, "no ice segment in package.\n");
		return ICE_DDP_PKG_INVALID_FILE;
	}

	/* Check if FW is compatible with the OS package */
	size = ice_struct_size(pkg, pkg_info, ICE_PKG_CNT);
	pkg = (struct ice_aqc_get_pkg_info_resp *)ice_malloc(hw, size);
	if (!pkg)
		return ICE_DDP_PKG_ERR;

	if (ice_aq_get_pkg_info_list(hw, pkg, size, NULL)) {
		state = ICE_DDP_PKG_ERR;
		goto fw_ddp_compat_free_alloc;
	}

	for (i = 0; i < LE32_TO_CPU(pkg->count); i++) {
		/* loop till we find the NVM package */
		if (!pkg->pkg_info[i].is_in_nvm)
			continue;
		if ((*seg)->hdr.seg_format_ver.major !=
			pkg->pkg_info[i].ver.major ||
		    (*seg)->hdr.seg_format_ver.minor >
			pkg->pkg_info[i].ver.minor) {
			state = ICE_DDP_PKG_FW_MISMATCH;
			ice_debug(hw, ICE_DBG_INIT, "OS package is not compatible with NVM.\n");
		}
		/* done processing NVM package so break */
		break;
	}
fw_ddp_compat_free_alloc:
	ice_free(hw, pkg);
	return state;
}

/**
 * ice_sw_fv_handler
 * @sect_type: section type
 * @section: pointer to section
 * @index: index of the field vector entry to be returned
 * @offset: ptr to variable that receives the offset in the field vector table
 *
 * This is a callback function that can be passed to ice_pkg_enum_entry.
 * This function treats the given section as of type ice_sw_fv_section and
 * enumerates offset field. "offset" is an index into the field vector table.
 */
static void *
ice_sw_fv_handler(u32 sect_type, void *section, u32 index, u32 *offset)
{
	struct ice_sw_fv_section *fv_section =
		(struct ice_sw_fv_section *)section;

	if (!section || sect_type != ICE_SID_FLD_VEC_SW)
		return NULL;
	if (index >= LE16_TO_CPU(fv_section->count))
		return NULL;
	if (offset)
		/* "index" passed in to this function is relative to a given
		 * 4k block. To get to the true index into the field vector
		 * table need to add the relative index to the base_offset
		 * field of this section
		 */
		*offset = LE16_TO_CPU(fv_section->base_offset) + index;
	return fv_section->fv + index;
}

/**
 * ice_get_prof_index_max - get the max profile index for used profile
 * @hw: pointer to the HW struct
 *
 * Calling this function will get the max profile index for used profile
 * and store the index number in struct ice_switch_info *switch_info
 * in hw for following use.
 */
static int ice_get_prof_index_max(struct ice_hw *hw)
{
	u16 prof_index = 0, j, max_prof_index = 0;
	struct ice_pkg_enum state;
	struct ice_seg *ice_seg;
	bool flag = false;
	struct ice_fv *fv;
	u32 offset;

	ice_memset(&state, 0, sizeof(state), ICE_NONDMA_MEM);

	if (!hw->seg)
		return ICE_ERR_PARAM;

	ice_seg = hw->seg;

	do {
		fv = (struct ice_fv *)
			ice_pkg_enum_entry(ice_seg, &state, ICE_SID_FLD_VEC_SW,
					   &offset, ice_sw_fv_handler);
		if (!fv)
			break;
		ice_seg = NULL;

		/* in the profile that not be used, the prot_id is set to 0xff
		 * and the off is set to 0x1ff for all the field vectors.
		 */
		for (j = 0; j < hw->blk[ICE_BLK_SW].es.fvw; j++)
			if (fv->ew[j].prot_id != ICE_PROT_INVALID ||
			    fv->ew[j].off != ICE_FV_OFFSET_INVAL)
				flag = true;
		if (flag && prof_index > max_prof_index)
			max_prof_index = prof_index;

		prof_index++;
		flag = false;
	} while (fv);

	hw->switch_info->max_used_prof_index = max_prof_index;

	return ICE_SUCCESS;
}

/**
 * ice_get_ddp_pkg_state - get DDP pkg state after download
 * @hw: pointer to the HW struct
 * @already_loaded: indicates if pkg was already loaded onto the device
 *
 */
static enum ice_ddp_state
ice_get_ddp_pkg_state(struct ice_hw *hw, bool already_loaded)
{
	if (hw->pkg_ver.major == hw->active_pkg_ver.major &&
	    hw->pkg_ver.minor == hw->active_pkg_ver.minor &&
	    hw->pkg_ver.update == hw->active_pkg_ver.update &&
	    hw->pkg_ver.draft == hw->active_pkg_ver.draft &&
	    !memcmp(hw->pkg_name, hw->active_pkg_name, sizeof(hw->pkg_name))) {
		if (already_loaded)
			return ICE_DDP_PKG_SAME_VERSION_ALREADY_LOADED;
		else
			return ICE_DDP_PKG_SUCCESS;
	} else if (hw->active_pkg_ver.major != ICE_PKG_SUPP_VER_MAJ ||
		   hw->active_pkg_ver.minor != ICE_PKG_SUPP_VER_MNR) {
		return ICE_DDP_PKG_ALREADY_LOADED_NOT_SUPPORTED;
	} else if (hw->active_pkg_ver.major == ICE_PKG_SUPP_VER_MAJ &&
		   hw->active_pkg_ver.minor == ICE_PKG_SUPP_VER_MNR) {
		return ICE_DDP_PKG_COMPATIBLE_ALREADY_LOADED;
	} else {
		return ICE_DDP_PKG_ERR;
	}
}

/**
 * ice_init_pkg_regs - initialize additional package registers
 * @hw: pointer to the hardware structure
 */
static void ice_init_pkg_regs(struct ice_hw *hw)
{
#define ICE_SW_BLK_INP_MASK_L 0xFFFFFFFF
#define ICE_SW_BLK_INP_MASK_H 0x0000FFFF
#define ICE_SW_BLK_IDX	0

	/* setup Switch block input mask, which is 48-bits in two parts */
	wr32(hw, GL_PREEXT_L2_PMASK0(ICE_SW_BLK_IDX), ICE_SW_BLK_INP_MASK_L);
	wr32(hw, GL_PREEXT_L2_PMASK1(ICE_SW_BLK_IDX), ICE_SW_BLK_INP_MASK_H);
}

/**
 * ice_init_pkg - initialize/download package
 * @hw: pointer to the hardware structure
 * @buf: pointer to the package buffer
 * @len: size of the package buffer
 *
 * This function initializes a package. The package contains HW tables
 * required to do packet processing. First, the function extracts package
 * information such as version. Then it finds the ice configuration segment
 * within the package; this function then saves a copy of the segment pointer
 * within the supplied package buffer. Next, the function will cache any hints
 * from the package, followed by downloading the package itself. Note, that if
 * a previous PF driver has already downloaded the package successfully, then
 * the current driver will not have to download the package again.
 *
 * The local package contents will be used to query default behavior and to
 * update specific sections of the HW's version of the package (e.g. to update
 * the parse graph to understand new protocols).
 *
 * This function stores a pointer to the package buffer memory, and it is
 * expected that the supplied buffer will not be freed immediately. If the
 * package buffer needs to be freed, such as when read from a file, use
 * ice_copy_and_init_pkg() instead of directly calling ice_init_pkg() in this
 * case.
 */
enum ice_ddp_state ice_init_pkg(struct ice_hw *hw, u8 *buf, u32 len)
{
	bool already_loaded = false;
	enum ice_ddp_state state;
	struct ice_pkg_hdr *pkg;
	struct ice_seg *seg;

	if (!buf || !len)
		return ICE_DDP_PKG_ERR;

	pkg = (struct ice_pkg_hdr *)buf;
	state = ice_verify_pkg(pkg, len);
	if (state) {
		ice_debug(hw, ICE_DBG_INIT, "failed to verify pkg (err: %d)\n",
			  state);
		return state;
	}

	/* initialize package info */
	state = ice_init_pkg_info(hw, pkg);
	if (state)
		return state;

	/* For packages with signing segments, must be a matching segment */
	if (hw->pkg_has_signing_seg)
		if (!ice_match_signing_seg(pkg, hw->pkg_seg_id,
					   hw->pkg_sign_type))
			return ICE_DDP_PKG_ERR;

	/* before downloading the package, check package version for
	 * compatibility with driver
	 */
	state = ice_chk_pkg_compat(hw, pkg, &seg);
	if (state)
		return state;

	/* initialize package hints and then download package */
	ice_init_pkg_hints(hw, seg);
	state = ice_download_pkg(hw, pkg, seg);

	if (state == ICE_DDP_PKG_ALREADY_LOADED) {
		ice_debug(hw, ICE_DBG_INIT, "package previously loaded - no work.\n");
		already_loaded = true;
	}

	/* Get information on the package currently loaded in HW, then make sure
	 * the driver is compatible with this version.
	 */
	if (!state || state == ICE_DDP_PKG_ALREADY_LOADED) {
		state = ice_get_pkg_info(hw);
		if (!state)
			state = ice_get_ddp_pkg_state(hw, already_loaded);
	}

	if (ice_is_init_pkg_successful(state)) {
		hw->seg = seg;
		/* on successful package download update other required
		 * registers to support the package and fill HW tables
		 * with package content.
		 */
		ice_init_pkg_regs(hw);
		ice_fill_blk_tbls(hw);
		ice_get_prof_index_max(hw);
	} else {
		ice_debug(hw, ICE_DBG_INIT, "package load failed, %d\n",
			  state);
	}

	return state;
}

/**
 * ice_copy_and_init_pkg - initialize/download a copy of the package
 * @hw: pointer to the hardware structure
 * @buf: pointer to the package buffer
 * @len: size of the package buffer
 *
 * This function copies the package buffer, and then calls ice_init_pkg() to
 * initialize the copied package contents.
 *
 * The copying is necessary if the package buffer supplied is constant, or if
 * the memory may disappear shortly after calling this function.
 *
 * If the package buffer resides in the data segment and can be modified, the
 * caller is free to use ice_init_pkg() instead of ice_copy_and_init_pkg().
 *
 * However, if the package buffer needs to be copied first, such as when being
 * read from a file, the caller should use ice_copy_and_init_pkg().
 *
 * This function will first copy the package buffer, before calling
 * ice_init_pkg(). The caller is free to immediately destroy the original
 * package buffer, as the new copy will be managed by this function and
 * related routines.
 */
enum ice_ddp_state
ice_copy_and_init_pkg(struct ice_hw *hw, const u8 *buf, u32 len)
{
	enum ice_ddp_state state;
	u8 *buf_copy;

	if (!buf || !len)
		return ICE_DDP_PKG_ERR;

	buf_copy = (u8 *)ice_memdup(hw, buf, len, ICE_NONDMA_TO_NONDMA);

	state = ice_init_pkg(hw, buf_copy, len);
	if (!ice_is_init_pkg_successful(state)) {
		/* Free the copy, since we failed to initialize the package */
		ice_free(hw, buf_copy);
	} else {
		/* Track the copied pkg so we can free it later */
		hw->pkg_copy = buf_copy;
		hw->pkg_size = len;
	}

	return state;
}

/**
 * ice_is_init_pkg_successful - check if DDP init was successful
 * @state: state of the DDP pkg after download
 */
bool ice_is_init_pkg_successful(enum ice_ddp_state state)
{
	switch (state) {
	case ICE_DDP_PKG_SUCCESS:
	case ICE_DDP_PKG_SAME_VERSION_ALREADY_LOADED:
	case ICE_DDP_PKG_COMPATIBLE_ALREADY_LOADED:
		return true;
	default:
		return false;
	}
}

/**
 * ice_pkg_buf_alloc
 * @hw: pointer to the HW structure
 *
 * Allocates a package buffer and returns a pointer to the buffer header.
 * Note: all package contents must be in Little Endian form.
 */
struct ice_buf_build *ice_pkg_buf_alloc(struct ice_hw *hw)
{
	struct ice_buf_build *bld;
	struct ice_buf_hdr *buf;

	bld = (struct ice_buf_build *)ice_malloc(hw, sizeof(*bld));
	if (!bld)
		return NULL;

	buf = (struct ice_buf_hdr *)bld;
	buf->data_end = CPU_TO_LE16(offsetof(struct ice_buf_hdr,
					     section_entry));
	return bld;
}

static bool ice_is_gtp_u_profile(u32 prof_idx)
{
	return (prof_idx >= ICE_PROFID_IPV6_GTPU_TEID &&
		prof_idx <= ICE_PROFID_IPV6_GTPU_IPV6_TCP) ||
	       prof_idx == ICE_PROFID_IPV4_GTPU_TEID;
}

static bool ice_is_gtp_c_profile(u32 prof_idx)
{
	switch (prof_idx) {
	case ICE_PROFID_IPV4_GTPC_TEID:
	case ICE_PROFID_IPV4_GTPC_NO_TEID:
	case ICE_PROFID_IPV6_GTPC_TEID:
	case ICE_PROFID_IPV6_GTPC_NO_TEID:
		return true;
	default:
		return false;
	}
}

/**
 * ice_get_sw_prof_type - determine switch profile type
 * @hw: pointer to the HW structure
 * @fv: pointer to the switch field vector
 * @prof_idx: profile index to check
 */
static enum ice_prof_type
ice_get_sw_prof_type(struct ice_hw *hw, struct ice_fv *fv, u32 prof_idx)
{
	bool valid_prof = false;
	u16 i;

	if (ice_is_gtp_c_profile(prof_idx))
		return ICE_PROF_TUN_GTPC;

	if (ice_is_gtp_u_profile(prof_idx))
		return ICE_PROF_TUN_GTPU;

	for (i = 0; i < hw->blk[ICE_BLK_SW].es.fvw; i++) {
		if (fv->ew[i].off != ICE_NAN_OFFSET)
			valid_prof = true;

		/* UDP tunnel will have UDP_OF protocol ID and VNI offset */
		if (fv->ew[i].prot_id == (u8)ICE_PROT_UDP_OF &&
		    fv->ew[i].off == ICE_VNI_OFFSET)
			return ICE_PROF_TUN_UDP;

		/* GRE tunnel will have GRE protocol */
		if (fv->ew[i].prot_id == (u8)ICE_PROT_GRE_OF)
			return ICE_PROF_TUN_GRE;
	}

	return valid_prof ? ICE_PROF_NON_TUN : ICE_PROF_INVALID;
}

/**
 * ice_get_sw_fv_bitmap - Get switch field vector bitmap based on profile type
 * @hw: pointer to hardware structure
 * @req_profs: type of profiles requested
 * @bm: pointer to memory for returning the bitmap of field vectors
 */
void
ice_get_sw_fv_bitmap(struct ice_hw *hw, enum ice_prof_type req_profs,
		     ice_bitmap_t *bm)
{
	struct ice_pkg_enum state;
	struct ice_seg *ice_seg;
	struct ice_fv *fv;

	ice_memset(&state, 0, sizeof(state), ICE_NONDMA_MEM);
	ice_zero_bitmap(bm, ICE_MAX_NUM_PROFILES);
	ice_seg = hw->seg;
	do {
		enum ice_prof_type prof_type;
		u32 offset;

		fv = (struct ice_fv *)
			ice_pkg_enum_entry(ice_seg, &state, ICE_SID_FLD_VEC_SW,
					   &offset, ice_sw_fv_handler);
		ice_seg = NULL;

		if (fv) {
			/* Determine field vector type */
			prof_type = ice_get_sw_prof_type(hw, fv, offset);

			if (req_profs & prof_type)
				ice_set_bit((u16)offset, bm);
		}
	} while (fv);
}

/**
 * ice_get_sw_fv_list
 * @hw: pointer to the HW structure
 * @lkups: lookup elements or match criteria for the advanced recipe, one
 *	   structure per protocol header
 * @bm: bitmap of field vectors to consider
 * @fv_list: Head of a list
 *
 * Finds all the field vector entries from switch block that contain
 * a given protocol ID and offset and returns a list of structures of type
 * "ice_sw_fv_list_entry". Every structure in the list has a field vector
 * definition and profile ID information
 * NOTE: The caller of the function is responsible for freeing the memory
 * allocated for every list entry.
 */
enum ice_status
ice_get_sw_fv_list(struct ice_hw *hw, struct ice_prot_lkup_ext *lkups,
		   ice_bitmap_t *bm, struct LIST_HEAD_TYPE *fv_list)
{
	struct ice_sw_fv_list_entry *fvl;
	struct ice_sw_fv_list_entry *tmp;
	struct ice_pkg_enum state;
	struct ice_seg *ice_seg;
	struct ice_fv *fv;
	u32 offset;

	ice_memset(&state, 0, sizeof(state), ICE_NONDMA_MEM);

	if (!lkups->n_val_words || !hw->seg)
		return ICE_ERR_PARAM;

	ice_seg = hw->seg;
	do {
		u16 i;

		fv = (struct ice_fv *)
			ice_pkg_enum_entry(ice_seg, &state, ICE_SID_FLD_VEC_SW,
					   &offset, ice_sw_fv_handler);
		if (!fv)
			break;
		ice_seg = NULL;

		/* If field vector is not in the bitmap list, then skip this
		 * profile.
		 */
		if (!ice_is_bit_set(bm, (u16)offset))
			continue;

		for (i = 0; i < lkups->n_val_words; i++) {
			int j;

			for (j = 0; j < hw->blk[ICE_BLK_SW].es.fvw; j++)
				if (fv->ew[j].prot_id ==
				    lkups->fv_words[i].prot_id &&
				    fv->ew[j].off == lkups->fv_words[i].off)
					break;
			if (j >= hw->blk[ICE_BLK_SW].es.fvw)
				break;
			if (i + 1 == lkups->n_val_words) {
				fvl = (struct ice_sw_fv_list_entry *)
					ice_malloc(hw, sizeof(*fvl));
				if (!fvl)
					goto err;
				fvl->fv_ptr = fv;
				fvl->profile_id = offset;
				LIST_ADD(&fvl->list_entry, fv_list);
				break;
			}
		}
	} while (fv);
	if (LIST_EMPTY(fv_list)) {
		ice_warn(hw, "Required profiles not found in currently loaded DDP package");
		return ICE_ERR_CFG;
	}
	return ICE_SUCCESS;

err:
	LIST_FOR_EACH_ENTRY_SAFE(fvl, tmp, fv_list, ice_sw_fv_list_entry,
				 list_entry) {
		LIST_DEL(&fvl->list_entry);
		ice_free(hw, fvl);
	}

	return ICE_ERR_NO_MEMORY;
}

/**
 * ice_init_prof_result_bm - Initialize the profile result index bitmap
 * @hw: pointer to hardware structure
 */
void ice_init_prof_result_bm(struct ice_hw *hw)
{
	struct ice_pkg_enum state;
	struct ice_seg *ice_seg;
	struct ice_fv *fv;

	ice_memset(&state, 0, sizeof(state), ICE_NONDMA_MEM);

	if (!hw->seg)
		return;

	ice_seg = hw->seg;
	do {
		u32 off;
		u16 i;

		fv = (struct ice_fv *)
			ice_pkg_enum_entry(ice_seg, &state, ICE_SID_FLD_VEC_SW,
					   &off, ice_sw_fv_handler);
		ice_seg = NULL;
		if (!fv)
			break;

		ice_zero_bitmap(hw->switch_info->prof_res_bm[off],
				ICE_MAX_FV_WORDS);

		/* Determine empty field vector indices, these can be
		 * used for recipe results. Skip index 0, since it is
		 * always used for Switch ID.
		 */
		for (i = 1; i < ICE_MAX_FV_WORDS; i++)
			if (fv->ew[i].prot_id == ICE_PROT_INVALID &&
			    fv->ew[i].off == ICE_FV_OFFSET_INVAL)
				ice_set_bit(i,
					    hw->switch_info->prof_res_bm[off]);
	} while (fv);
}

/**
 * ice_pkg_buf_free
 * @hw: pointer to the HW structure
 * @bld: pointer to pkg build (allocated by ice_pkg_buf_alloc())
 *
 * Frees a package buffer
 */
void ice_pkg_buf_free(struct ice_hw *hw, struct ice_buf_build *bld)
{
	ice_free(hw, bld);
}

/**
 * ice_pkg_buf_reserve_section
 * @bld: pointer to pkg build (allocated by ice_pkg_buf_alloc())
 * @count: the number of sections to reserve
 *
 * Reserves one or more section table entries in a package buffer. This routine
 * can be called multiple times as long as they are made before calling
 * ice_pkg_buf_alloc_section(). Once ice_pkg_buf_alloc_section()
 * is called once, the number of sections that can be allocated will not be able
 * to be increased; not using all reserved sections is fine, but this will
 * result in some wasted space in the buffer.
 * Note: all package contents must be in Little Endian form.
 */
enum ice_status
ice_pkg_buf_reserve_section(struct ice_buf_build *bld, u16 count)
{
	struct ice_buf_hdr *buf;
	u16 section_count;
	u16 data_end;

	if (!bld)
		return ICE_ERR_PARAM;

	buf = (struct ice_buf_hdr *)&bld->buf;

	/* already an active section, can't increase table size */
	section_count = LE16_TO_CPU(buf->section_count);
	if (section_count > 0)
		return ICE_ERR_CFG;

	if (bld->reserved_section_table_entries + count > ICE_MAX_S_COUNT)
		return ICE_ERR_CFG;
	bld->reserved_section_table_entries += count;

	data_end = LE16_TO_CPU(buf->data_end) +
		FLEX_ARRAY_SIZE(buf, section_entry, count);
	buf->data_end = CPU_TO_LE16(data_end);

	return ICE_SUCCESS;
}

/**
 * ice_pkg_buf_alloc_section
 * @bld: pointer to pkg build (allocated by ice_pkg_buf_alloc())
 * @type: the section type value
 * @size: the size of the section to reserve (in bytes)
 *
 * Reserves memory in the buffer for a section's content and updates the
 * buffers' status accordingly. This routine returns a pointer to the first
 * byte of the section start within the buffer, which is used to fill in the
 * section contents.
 * Note: all package contents must be in Little Endian form.
 */
void *
ice_pkg_buf_alloc_section(struct ice_buf_build *bld, u32 type, u16 size)
{
	struct ice_buf_hdr *buf;
	u16 sect_count;
	u16 data_end;

	if (!bld || !type || !size)
		return NULL;

	buf = (struct ice_buf_hdr *)&bld->buf;

	/* check for enough space left in buffer */
	data_end = LE16_TO_CPU(buf->data_end);

	/* section start must align on 4 byte boundary */
	data_end = ICE_ALIGN(data_end, 4);

	if ((data_end + size) > ICE_MAX_S_DATA_END)
		return NULL;

	/* check for more available section table entries */
	sect_count = LE16_TO_CPU(buf->section_count);
	if (sect_count < bld->reserved_section_table_entries) {
		void *section_ptr = ((u8 *)buf) + data_end;

		buf->section_entry[sect_count].offset = CPU_TO_LE16(data_end);
		buf->section_entry[sect_count].size = CPU_TO_LE16(size);
		buf->section_entry[sect_count].type = CPU_TO_LE32(type);

		data_end += size;
		buf->data_end = CPU_TO_LE16(data_end);

		buf->section_count = CPU_TO_LE16(sect_count + 1);
		return section_ptr;
	}

	/* no free section table entries */
	return NULL;
}

/**
 * ice_pkg_buf_alloc_single_section
 * @hw: pointer to the HW structure
 * @type: the section type value
 * @size: the size of the section to reserve (in bytes)
 * @section: returns pointer to the section
 *
 * Allocates a package buffer with a single section.
 * Note: all package contents must be in Little Endian form.
 */
struct ice_buf_build *
ice_pkg_buf_alloc_single_section(struct ice_hw *hw, u32 type, u16 size,
				 void **section)
{
	struct ice_buf_build *buf;

	if (!section)
		return NULL;

	buf = ice_pkg_buf_alloc(hw);
	if (!buf)
		return NULL;

	if (ice_pkg_buf_reserve_section(buf, 1))
		goto ice_pkg_buf_alloc_single_section_err;

	*section = ice_pkg_buf_alloc_section(buf, type, size);
	if (!*section)
		goto ice_pkg_buf_alloc_single_section_err;

	return buf;

ice_pkg_buf_alloc_single_section_err:
	ice_pkg_buf_free(hw, buf);
	return NULL;
}

/**
 * ice_pkg_buf_unreserve_section
 * @bld: pointer to pkg build (allocated by ice_pkg_buf_alloc())
 * @count: the number of sections to unreserve
 *
 * Unreserves one or more section table entries in a package buffer, releasing
 * space that can be used for section data. This routine can be called
 * multiple times as long as they are made before calling
 * ice_pkg_buf_alloc_section(). Once ice_pkg_buf_alloc_section()
 * is called once, the number of sections that can be allocated will not be able
 * to be increased; not using all reserved sections is fine, but this will
 * result in some wasted space in the buffer.
 * Note: all package contents must be in Little Endian form.
 */
enum ice_status
ice_pkg_buf_unreserve_section(struct ice_buf_build *bld, u16 count)
{
	struct ice_buf_hdr *buf;
	u16 section_count;
	u16 data_end;

	if (!bld)
		return ICE_ERR_PARAM;

	buf = (struct ice_buf_hdr *)&bld->buf;

	/* already an active section, can't decrease table size */
	section_count = LE16_TO_CPU(buf->section_count);
	if (section_count > 0)
		return ICE_ERR_CFG;

	if (count > bld->reserved_section_table_entries)
		return ICE_ERR_CFG;
	bld->reserved_section_table_entries -= count;

	data_end = LE16_TO_CPU(buf->data_end) -
		FLEX_ARRAY_SIZE(buf, section_entry, count);
	buf->data_end = CPU_TO_LE16(data_end);

	return ICE_SUCCESS;
}

/**
 * ice_pkg_buf_get_free_space
 * @bld: pointer to pkg build (allocated by ice_pkg_buf_alloc())
 *
 * Returns the number of free bytes remaining in the buffer.
 * Note: all package contents must be in Little Endian form.
 */
u16 ice_pkg_buf_get_free_space(struct ice_buf_build *bld)
{
	struct ice_buf_hdr *buf;

	if (!bld)
		return 0;

	buf = (struct ice_buf_hdr *)&bld->buf;
	return ICE_MAX_S_DATA_END - LE16_TO_CPU(buf->data_end);
}

/**
 * ice_pkg_buf_get_active_sections
 * @bld: pointer to pkg build (allocated by ice_pkg_buf_alloc())
 *
 * Returns the number of active sections. Before using the package buffer
 * in an update package command, the caller should make sure that there is at
 * least one active section - otherwise, the buffer is not legal and should
 * not be used.
 * Note: all package contents must be in Little Endian form.
 */
u16 ice_pkg_buf_get_active_sections(struct ice_buf_build *bld)
{
	struct ice_buf_hdr *buf;

	if (!bld)
		return 0;

	buf = (struct ice_buf_hdr *)&bld->buf;
	return LE16_TO_CPU(buf->section_count);
}

/**
 * ice_pkg_buf
 * @bld: pointer to pkg build (allocated by ice_pkg_buf_alloc())
 *
 * Return a pointer to the buffer's header
 */
struct ice_buf *ice_pkg_buf(struct ice_buf_build *bld)
{
	if (bld)
		return &bld->buf;

	return NULL;
}

/**
 * ice_find_buf_table
 * @ice_seg: pointer to the ice segment
 *
 * Returns the address of the buffer table within the ice segment.
 */
struct ice_buf_table *ice_find_buf_table(struct ice_seg *ice_seg)
{
	struct ice_nvm_table *nvms;

	nvms = (struct ice_nvm_table *)
		(ice_seg->device_table +
		 LE32_TO_CPU(ice_seg->device_table_count));

	return (_FORCE_ struct ice_buf_table *)
		(nvms->vers + LE32_TO_CPU(nvms->table_count));
}

/**
 * ice_pkg_val_buf
 * @buf: pointer to the ice buffer
 *
 * This helper function validates a buffer's header.
 */
static struct ice_buf_hdr *ice_pkg_val_buf(struct ice_buf *buf)
{
	struct ice_buf_hdr *hdr;
	u16 section_count;
	u16 data_end;

	hdr = (struct ice_buf_hdr *)buf->buf;
	/* verify data */
	section_count = LE16_TO_CPU(hdr->section_count);
	if (section_count < ICE_MIN_S_COUNT || section_count > ICE_MAX_S_COUNT)
		return NULL;

	data_end = LE16_TO_CPU(hdr->data_end);
	if (data_end < ICE_MIN_S_DATA_END || data_end > ICE_MAX_S_DATA_END)
		return NULL;

	return hdr;
}

/**
 * ice_pkg_enum_buf
 * @ice_seg: pointer to the ice segment (or NULL on subsequent calls)
 * @state: pointer to the enum state
 *
 * This function will enumerate all the buffers in the ice segment. The first
 * call is made with the ice_seg parameter non-NULL; on subsequent calls,
 * ice_seg is set to NULL which continues the enumeration. When the function
 * returns a NULL pointer, then the end of the buffers has been reached, or an
 * unexpected value has been detected (for example an invalid section count or
 * an invalid buffer end value).
 */
struct ice_buf_hdr *
ice_pkg_enum_buf(struct ice_seg *ice_seg, struct ice_pkg_enum *state)
{
	if (ice_seg) {
		state->buf_table = ice_find_buf_table(ice_seg);
		if (!state->buf_table)
			return NULL;

		state->buf_idx = 0;
		return ice_pkg_val_buf(state->buf_table->buf_array);
	}

	if (++state->buf_idx < LE32_TO_CPU(state->buf_table->buf_count))
		return ice_pkg_val_buf(state->buf_table->buf_array +
				       state->buf_idx);
	else
		return NULL;
}

/**
 * ice_pkg_advance_sect
 * @ice_seg: pointer to the ice segment (or NULL on subsequent calls)
 * @state: pointer to the enum state
 *
 * This helper function will advance the section within the ice segment,
 * also advancing the buffer if needed.
 */
bool
ice_pkg_advance_sect(struct ice_seg *ice_seg, struct ice_pkg_enum *state)
{
	if (!ice_seg && !state->buf)
		return false;

	if (!ice_seg && state->buf)
		if (++state->sect_idx < LE16_TO_CPU(state->buf->section_count))
			return true;

	state->buf = ice_pkg_enum_buf(ice_seg, state);
	if (!state->buf)
		return false;

	/* start of new buffer, reset section index */
	state->sect_idx = 0;
	return true;
}

/**
 * ice_pkg_enum_section
 * @ice_seg: pointer to the ice segment (or NULL on subsequent calls)
 * @state: pointer to the enum state
 * @sect_type: section type to enumerate
 *
 * This function will enumerate all the sections of a particular type in the
 * ice segment. The first call is made with the ice_seg parameter non-NULL;
 * on subsequent calls, ice_seg is set to NULL which continues the enumeration.
 * When the function returns a NULL pointer, then the end of the matching
 * sections has been reached.
 */
void *
ice_pkg_enum_section(struct ice_seg *ice_seg, struct ice_pkg_enum *state,
		     u32 sect_type)
{
	u16 offset, size;

	if (ice_seg)
		state->type = sect_type;

	if (!ice_pkg_advance_sect(ice_seg, state))
		return NULL;

	/* scan for next matching section */
	while (state->buf->section_entry[state->sect_idx].type !=
	       CPU_TO_LE32(state->type))
		if (!ice_pkg_advance_sect(NULL, state))
			return NULL;

	/* validate section */
	offset = LE16_TO_CPU(state->buf->section_entry[state->sect_idx].offset);
	if (offset < ICE_MIN_S_OFF || offset > ICE_MAX_S_OFF)
		return NULL;

	size = LE16_TO_CPU(state->buf->section_entry[state->sect_idx].size);
	if (size < ICE_MIN_S_SZ || size > ICE_MAX_S_SZ)
		return NULL;

	/* make sure the section fits in the buffer */
	if (offset + size > ICE_PKG_BUF_SIZE)
		return NULL;

	state->sect_type =
		LE32_TO_CPU(state->buf->section_entry[state->sect_idx].type);

	/* calc pointer to this section */
	state->sect = ((u8 *)state->buf) +
		LE16_TO_CPU(state->buf->section_entry[state->sect_idx].offset);

	return state->sect;
}

/**
 * ice_pkg_enum_entry
 * @ice_seg: pointer to the ice segment (or NULL on subsequent calls)
 * @state: pointer to the enum state
 * @sect_type: section type to enumerate
 * @offset: pointer to variable that receives the offset in the table (optional)
 * @handler: function that handles access to the entries into the section type
 *
 * This function will enumerate all the entries in particular section type in
 * the ice segment. The first call is made with the ice_seg parameter non-NULL;
 * on subsequent calls, ice_seg is set to NULL which continues the enumeration.
 * When the function returns a NULL pointer, then the end of the entries has
 * been reached.
 *
 * Since each section may have a different header and entry size, the handler
 * function is needed to determine the number and location entries in each
 * section.
 *
 * The offset parameter is optional, but should be used for sections that
 * contain an offset for each section table. For such cases, the section handler
 * function must return the appropriate offset + index to give the absolution
 * offset for each entry. For example, if the base for a section's header
 * indicates a base offset of 10, and the index for the entry is 2, then
 * section handler function should set the offset to 10 + 2 = 12.
 */
void *
ice_pkg_enum_entry(struct ice_seg *ice_seg, struct ice_pkg_enum *state,
		   u32 sect_type, u32 *offset,
		   void *(*handler)(u32 sect_type, void *section,
				    u32 index, u32 *offset))
{
	void *entry;

	if (ice_seg) {
		if (!handler)
			return NULL;

		if (!ice_pkg_enum_section(ice_seg, state, sect_type))
			return NULL;

		state->entry_idx = 0;
		state->handler = handler;
	} else {
		state->entry_idx++;
	}

	if (!state->handler)
		return NULL;

	/* get entry */
	entry = state->handler(state->sect_type, state->sect, state->entry_idx,
			       offset);
	if (!entry) {
		/* end of a section, look for another section of this type */
		if (!ice_pkg_enum_section(NULL, state, 0))
			return NULL;

		state->entry_idx = 0;
		entry = state->handler(state->sect_type, state->sect,
				       state->entry_idx, offset);
	}

	return entry;
}

/**
 * ice_boost_tcam_handler
 * @sect_type: section type
 * @section: pointer to section
 * @index: index of the boost TCAM entry to be returned
 * @offset: pointer to receive absolute offset, always 0 for boost TCAM sections
 *
 * This is a callback function that can be passed to ice_pkg_enum_entry.
 * Handles enumeration of individual boost TCAM entries.
 */
static void *
ice_boost_tcam_handler(u32 sect_type, void *section, u32 index, u32 *offset)
{
	struct ice_boost_tcam_section *boost;

	if (!section)
		return NULL;

	if (sect_type != ICE_SID_RXPARSER_BOOST_TCAM)
		return NULL;

	if (index > ICE_MAX_BST_TCAMS_IN_BUF)
		return NULL;

	if (offset)
		*offset = 0;

	boost = (struct ice_boost_tcam_section *)section;
	if (index >= LE16_TO_CPU(boost->count))
		return NULL;

	return boost->tcam + index;
}

/**
 * ice_find_boost_entry
 * @ice_seg: pointer to the ice segment (non-NULL)
 * @addr: Boost TCAM address of entry to search for
 * @entry: returns pointer to the entry
 *
 * Finds a particular Boost TCAM entry and returns a pointer to that entry
 * if it is found. The ice_seg parameter must not be NULL since the first call
 * to ice_pkg_enum_entry requires a pointer to an actual ice_segment structure.
 */
static enum ice_status
ice_find_boost_entry(struct ice_seg *ice_seg, u16 addr,
		     struct ice_boost_tcam_entry **entry)
{
	struct ice_boost_tcam_entry *tcam;
	struct ice_pkg_enum state;

	ice_memset(&state, 0, sizeof(state), ICE_NONDMA_MEM);

	if (!ice_seg)
		return ICE_ERR_PARAM;

	do {
		tcam = (struct ice_boost_tcam_entry *)
		       ice_pkg_enum_entry(ice_seg, &state,
					  ICE_SID_RXPARSER_BOOST_TCAM, NULL,
					  ice_boost_tcam_handler);
		if (tcam && LE16_TO_CPU(tcam->addr) == addr) {
			*entry = tcam;
			return ICE_SUCCESS;
		}

		ice_seg = NULL;
	} while (tcam);

	*entry = NULL;
	return ICE_ERR_CFG;
}

/**
 * ice_init_pkg_hints
 * @hw: pointer to the HW structure
 * @ice_seg: pointer to the segment of the package scan (non-NULL)
 *
 * This function will scan the package and save off relevant information
 * (hints or metadata) for driver use. The ice_seg parameter must not be NULL
 * since the first call to ice_enum_labels requires a pointer to an actual
 * ice_seg structure.
 */
void ice_init_pkg_hints(struct ice_hw *hw, struct ice_seg *ice_seg)
{
	struct ice_pkg_enum state;
	char *label_name;
	u16 val;
	int i;

	ice_memset(&hw->tnl, 0, sizeof(hw->tnl), ICE_NONDMA_MEM);
	ice_memset(&state, 0, sizeof(state), ICE_NONDMA_MEM);

	if (!ice_seg)
		return;

	label_name = ice_enum_labels(ice_seg, ICE_SID_LBL_RXPARSER_TMEM, &state,
				     &val);

	while (label_name) {
/* TODO: Replace !strnsmp() with wrappers like match_some_pre() */
		if (!strncmp(label_name, ICE_TNL_PRE, strlen(ICE_TNL_PRE)))
			/* check for a tunnel entry */
			ice_add_tunnel_hint(hw, label_name, val);

		label_name = ice_enum_labels(NULL, 0, &state, &val);
	}

	/* Cache the appropriate boost TCAM entry pointers for tunnels */
	for (i = 0; i < hw->tnl.count; i++) {
		ice_find_boost_entry(ice_seg, hw->tnl.tbl[i].boost_addr,
				     &hw->tnl.tbl[i].boost_entry);
		if (hw->tnl.tbl[i].boost_entry)
			hw->tnl.tbl[i].valid = true;
	}
}

/**
 * ice_acquire_global_cfg_lock
 * @hw: pointer to the HW structure
 * @access: access type (read or write)
 *
 * This function will request ownership of the global config lock for reading
 * or writing of the package. When attempting to obtain write access, the
 * caller must check for the following two return values:
 *
 * ICE_SUCCESS        - Means the caller has acquired the global config lock
 *                      and can perform writing of the package.
 * ICE_ERR_AQ_NO_WORK - Indicates another driver has already written the
 *                      package or has found that no update was necessary; in
 *                      this case, the caller can just skip performing any
 *                      update of the package.
 */
enum ice_status
ice_acquire_global_cfg_lock(struct ice_hw *hw,
			    enum ice_aq_res_access_type access)
{
	enum ice_status status;

	status = ice_acquire_res(hw, ICE_GLOBAL_CFG_LOCK_RES_ID, access,
				 ICE_GLOBAL_CFG_LOCK_TIMEOUT);

	if (status == ICE_ERR_AQ_NO_WORK)
		ice_debug(hw, ICE_DBG_PKG, "Global config lock: No work to do\n");

	return status;
}

/**
 * ice_release_global_cfg_lock
 * @hw: pointer to the HW structure
 *
 * This function will release the global config lock.
 */
void ice_release_global_cfg_lock(struct ice_hw *hw)
{
	ice_release_res(hw, ICE_GLOBAL_CFG_LOCK_RES_ID);
}

/**
 * ice_acquire_change_lock
 * @hw: pointer to the HW structure
 * @access: access type (read or write)
 *
 * This function will request ownership of the change lock.
 */
enum ice_status
ice_acquire_change_lock(struct ice_hw *hw, enum ice_aq_res_access_type access)
{
	return ice_acquire_res(hw, ICE_CHANGE_LOCK_RES_ID, access,
			       ICE_CHANGE_LOCK_TIMEOUT);
}

/**
 * ice_release_change_lock
 * @hw: pointer to the HW structure
 *
 * This function will release the change lock using the proper Admin Command.
 */
void ice_release_change_lock(struct ice_hw *hw)
{
	ice_release_res(hw, ICE_CHANGE_LOCK_RES_ID);
}

/**
 * ice_get_set_tx_topo - get or set tx topology
 * @hw: pointer to the HW struct
 * @buf: pointer to tx topology buffer
 * @buf_size: buffer size
 * @cd: pointer to command details structure or NULL
 * @flags: pointer to descriptor flags
 * @set: 0-get, 1-set topology
 *
 * The function will get or set tx topology
 */
static enum ice_status
ice_get_set_tx_topo(struct ice_hw *hw, u8 *buf, u16 buf_size,
		    struct ice_sq_cd *cd, u8 *flags, bool set)
{
	struct ice_aqc_get_set_tx_topo *cmd;
	struct ice_aq_desc desc;
	enum ice_status status;

	cmd = &desc.params.get_set_tx_topo;
	if (set) {
		ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_set_tx_topo);
		cmd->set_flags = ICE_AQC_TX_TOPO_FLAGS_ISSUED;
		/* requested to update a new topology, not a default topolgy */
		if (buf)
			cmd->set_flags |= ICE_AQC_TX_TOPO_FLAGS_SRC_RAM |
					  ICE_AQC_TX_TOPO_FLAGS_LOAD_NEW;
	} else {
		ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_get_tx_topo);
		cmd->get_flags = ICE_AQC_TX_TOPO_GET_RAM;
	}
	desc.flags |= CPU_TO_LE16(ICE_AQ_FLAG_RD);
	status = ice_aq_send_cmd(hw, &desc, buf, buf_size, cd);
	if (status)
		return status;
	/* read the return flag values (first byte) for get operation */
	if (!set && flags)
		*flags = desc.params.get_set_tx_topo.set_flags;

	return ICE_SUCCESS;
}

/**
 * ice_cfg_tx_topo - Initialize new tx topology if available
 * @hw: pointer to the HW struct
 * @buf: pointer to Tx topology buffer
 * @len: buffer size
 *
 * The function will apply the new Tx topology from the package buffer
 * if available.
 */
enum ice_status ice_cfg_tx_topo(struct ice_hw *hw, u8 *buf, u32 len)
{
	u8 *current_topo, *new_topo = NULL;
	struct ice_run_time_cfg_seg *seg;
	struct ice_buf_hdr *section;
	struct ice_pkg_hdr *pkg_hdr;
	enum ice_ddp_state state;
	u16 i, size = 0, offset;
	enum ice_status status;
	u32 reg = 0;
	u8 flags;

	if (!buf || !len)
		return ICE_ERR_PARAM;

	/* Does FW support new Tx topology mode ? */
	if (!hw->func_caps.common_cap.tx_sched_topo_comp_mode_en) {
		ice_debug(hw, ICE_DBG_INIT, "FW doesn't support compatibility mode\n");
		return ICE_ERR_NOT_SUPPORTED;
	}

	current_topo = (u8 *)ice_malloc(hw, ICE_AQ_MAX_BUF_LEN);
	if (!current_topo)
		return ICE_ERR_NO_MEMORY;

	/* get the current Tx topology */
	status = ice_get_set_tx_topo(hw, current_topo, ICE_AQ_MAX_BUF_LEN, NULL,
				     &flags, false);
	ice_free(hw, current_topo);

	if (status) {
		ice_debug(hw, ICE_DBG_INIT, "Get current topology is failed\n");
		return status;
	}

	/* Is default topology already applied ? */
	if (!(flags & ICE_AQC_TX_TOPO_FLAGS_LOAD_NEW) &&
	    hw->num_tx_sched_layers == 9) {
		ice_debug(hw, ICE_DBG_INIT, "Loaded default topology\n");
		/* Already default topology is loaded */
		return ICE_ERR_ALREADY_EXISTS;
	}

	/* Is new topology already applied ? */
	if ((flags & ICE_AQC_TX_TOPO_FLAGS_LOAD_NEW) &&
	    hw->num_tx_sched_layers == 5) {
		ice_debug(hw, ICE_DBG_INIT, "Loaded new topology\n");
		/* Already new topology is loaded */
		return ICE_ERR_ALREADY_EXISTS;
	}

	/* Is set topology issued already ? */
	if (flags & ICE_AQC_TX_TOPO_FLAGS_ISSUED) {
		ice_debug(hw, ICE_DBG_INIT, "Update tx topology was done by another PF\n");
		/* add a small delay before exiting */
		for (i = 0; i < 20; i++)
			ice_msec_delay(100, true);
		return ICE_ERR_ALREADY_EXISTS;
	}

	/* Change the topology from new to default (5 to 9) */
	if (!(flags & ICE_AQC_TX_TOPO_FLAGS_LOAD_NEW) &&
	    hw->num_tx_sched_layers == 5) {
		ice_debug(hw, ICE_DBG_INIT, "Change topology from 5 to 9 layers\n");
		goto update_topo;
	}

	pkg_hdr = (struct ice_pkg_hdr *)buf;
	state = ice_verify_pkg(pkg_hdr, len);
	if (state) {
		ice_debug(hw, ICE_DBG_INIT, "failed to verify pkg (err: %d)\n",
			  state);
		return ICE_ERR_CFG;
	}

	/* find run time configuration segment */
	seg = (struct ice_run_time_cfg_seg *)
		ice_find_seg_in_pkg(hw, SEGMENT_TYPE_ICE_RUN_TIME_CFG, pkg_hdr);
	if (!seg) {
		ice_debug(hw, ICE_DBG_INIT, "5 layer topology segment is missing\n");
		return ICE_ERR_CFG;
	}

	if (LE32_TO_CPU(seg->buf_table.buf_count) < ICE_MIN_S_COUNT) {
		ice_debug(hw, ICE_DBG_INIT, "5 layer topology segment count(%d) is wrong\n",
			  seg->buf_table.buf_count);
		return ICE_ERR_CFG;
	}

	section = ice_pkg_val_buf(seg->buf_table.buf_array);

	if (!section || LE32_TO_CPU(section->section_entry[0].type) !=
		ICE_SID_TX_5_LAYER_TOPO) {
		ice_debug(hw, ICE_DBG_INIT, "5 layer topology section type is wrong\n");
		return ICE_ERR_CFG;
	}

	size = LE16_TO_CPU(section->section_entry[0].size);
	offset = LE16_TO_CPU(section->section_entry[0].offset);
	if (size < ICE_MIN_S_SZ || size > ICE_MAX_S_SZ) {
		ice_debug(hw, ICE_DBG_INIT, "5 layer topology section size is wrong\n");
		return ICE_ERR_CFG;
	}

	/* make sure the section fits in the buffer */
	if (offset + size > ICE_PKG_BUF_SIZE) {
		ice_debug(hw, ICE_DBG_INIT, "5 layer topology buffer > 4K\n");
		return ICE_ERR_CFG;
	}

	/* Get the new topology buffer */
	new_topo = ((u8 *)section) + offset;

update_topo:
	/* acquire global lock to make sure that set topology issued
	 * by one PF
	 */
	status = ice_acquire_res(hw, ICE_GLOBAL_CFG_LOCK_RES_ID, ICE_RES_WRITE,
				 ICE_GLOBAL_CFG_LOCK_TIMEOUT);
	if (status) {
		ice_debug(hw, ICE_DBG_INIT, "Failed to acquire global lock\n");
		return status;
	}

	/* check reset was triggered already or not */
	reg = rd32(hw, GLGEN_RSTAT);
	if (reg & GLGEN_RSTAT_DEVSTATE_M) {
		/* Reset is in progress, re-init the hw again */
		ice_debug(hw, ICE_DBG_INIT, "Reset is in progress. layer topology might be applied already\n");
		ice_check_reset(hw);
		return ICE_SUCCESS;
	}

	/* set new topology */
	status = ice_get_set_tx_topo(hw, new_topo, size, NULL, NULL, true);
	if (status) {
		ice_debug(hw, ICE_DBG_INIT, "Set tx topology is failed\n");
		return status;
	}

	/* new topology is updated, delay 1 second before issuing the CORRER */
	for (i = 0; i < 10; i++)
		ice_msec_delay(100, true);
	ice_reset(hw, ICE_RESET_CORER);
	/* CORER will clear the global lock, so no explicit call
	 * required for release
	 */
	return ICE_SUCCESS;
}
