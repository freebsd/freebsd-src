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
/*$FreeBSD$*/

#ifndef _ICE_DDP_COMMON_H_
#define _ICE_DDP_COMMON_H_

#include "ice_osdep.h"
#include "ice_adminq_cmd.h"
#include "ice_controlq.h"
#include "ice_status.h"
#include "ice_flex_type.h"
#include "ice_protocol_type.h"

/* Package minimal version supported */
#define ICE_PKG_SUPP_VER_MAJ	1
#define ICE_PKG_SUPP_VER_MNR	3

/* Package format version */
#define ICE_PKG_FMT_VER_MAJ	1
#define ICE_PKG_FMT_VER_MNR	0
#define ICE_PKG_FMT_VER_UPD	0
#define ICE_PKG_FMT_VER_DFT	0

#define ICE_PKG_CNT 4

enum ice_ddp_state {
	/* Indicates that this call to ice_init_pkg
	 * successfully loaded the requested DDP package
	 */
	ICE_DDP_PKG_SUCCESS				= 0,

	/* Generic error for already loaded errors, it is mapped later to
	 * the more specific one (one of the next 3)
	 */
	ICE_DDP_PKG_ALREADY_LOADED			= -1,

	/* Indicates that a DDP package of the same version has already been
	 * loaded onto the device by a previous call or by another PF
	 */
	ICE_DDP_PKG_SAME_VERSION_ALREADY_LOADED		= -2,

	/* The device has a DDP package that is not supported by the driver */
	ICE_DDP_PKG_ALREADY_LOADED_NOT_SUPPORTED	= -3,

	/* The device has a compatible package
	 * (but different from the request) already loaded
	 */
	ICE_DDP_PKG_COMPATIBLE_ALREADY_LOADED		= -4,

	/* The firmware loaded on the device is not compatible with
	 * the DDP package loaded
	 */
	ICE_DDP_PKG_FW_MISMATCH				= -5,

	/* The DDP package file is invalid */
	ICE_DDP_PKG_INVALID_FILE			= -6,

	/* The version of the DDP package provided is higher than
	 * the driver supports
	 */
	ICE_DDP_PKG_FILE_VERSION_TOO_HIGH		= -7,

	/* The version of the DDP package provided is lower than the
	 * driver supports
	 */
	ICE_DDP_PKG_FILE_VERSION_TOO_LOW		= -8,

	/* Missing security manifest in DDP pkg */
	ICE_DDP_PKG_NO_SEC_MANIFEST			= -9,

	/* The RSA signature of the DDP package file provided is invalid */
	ICE_DDP_PKG_FILE_SIGNATURE_INVALID		= -10,

	/* The DDP package file security revision is too low and not
	 * supported by firmware
	 */
	ICE_DDP_PKG_SECURE_VERSION_NBR_TOO_LOW		= -11,

	/* Manifest hash mismatch */
	ICE_DDP_PKG_MANIFEST_INVALID			= -12,

	/* Buffer hash mismatches manifest */
	ICE_DDP_PKG_BUFFER_INVALID			= -13,

	/* Other errors */
	ICE_DDP_PKG_ERR					= -14,
};

/* Package and segment headers and tables */
struct ice_pkg_hdr {
	struct ice_pkg_ver pkg_format_ver;
	__le32 seg_count;
	__le32 seg_offset[STRUCT_HACK_VAR_LEN];
};

/* Package signing algorithm types */
#define SEGMENT_SIGN_TYPE_INVALID	0x00000000
#define SEGMENT_SIGN_TYPE_RSA2K		0x00000001
#define SEGMENT_SIGN_TYPE_RSA3K		0x00000002
#define SEGMENT_SIGN_TYPE_RSA3K_SBB	0x00000003 /* Secure Boot Block */

/* generic segment */
struct ice_generic_seg_hdr {
#define	SEGMENT_TYPE_INVALID	0x00000000
#define SEGMENT_TYPE_METADATA	0x00000001
#define SEGMENT_TYPE_ICE_E810	0x00000010
#define SEGMENT_TYPE_SIGNING	0x00001001
#define SEGMENT_TYPE_ICE_RUN_TIME_CFG 0x00000020
	__le32 seg_type;
	struct ice_pkg_ver seg_format_ver;
	__le32 seg_size;
	char seg_id[ICE_PKG_NAME_SIZE];
};

/* ice specific segment */

union ice_device_id {
	struct {
		__le16 device_id;
		__le16 vendor_id;
	} dev_vend_id;
	__le32 id;
};

struct ice_device_id_entry {
	union ice_device_id device;
	union ice_device_id sub_device;
};

struct ice_seg {
	struct ice_generic_seg_hdr hdr;
	__le32 device_table_count;
	struct ice_device_id_entry device_table[STRUCT_HACK_VAR_LEN];
};

struct ice_nvm_table {
	__le32 table_count;
	__le32 vers[STRUCT_HACK_VAR_LEN];
};

struct ice_buf {
#define ICE_PKG_BUF_SIZE	4096
	u8 buf[ICE_PKG_BUF_SIZE];
};

struct ice_buf_table {
	__le32 buf_count;
	struct ice_buf buf_array[STRUCT_HACK_VAR_LEN];
};

struct ice_run_time_cfg_seg {
	struct ice_generic_seg_hdr hdr;
	u8 rsvd[8];
	struct ice_buf_table buf_table;
};

/* global metadata specific segment */
struct ice_global_metadata_seg {
	struct ice_generic_seg_hdr hdr;
	struct ice_pkg_ver pkg_ver;
	__le32 rsvd;
	char pkg_name[ICE_PKG_NAME_SIZE];
};

#define ICE_MIN_S_OFF		12
#define ICE_MAX_S_OFF		4095
#define ICE_MIN_S_SZ		1
#define ICE_MAX_S_SZ		4084

struct ice_sign_seg {
	struct ice_generic_seg_hdr hdr;
	__le32 seg_id;
	__le32 sign_type;
	__le32 signed_seg_idx;
	__le32 signed_buf_start;
	__le32 signed_buf_count;
#define ICE_SIGN_SEG_RESERVED_COUNT	44
	u8 reserved[ICE_SIGN_SEG_RESERVED_COUNT];
	struct ice_buf_table buf_tbl;
};

/* section information */
struct ice_section_entry {
	__le32 type;
	__le16 offset;
	__le16 size;
};

#define ICE_MIN_S_COUNT		1
#define ICE_MAX_S_COUNT		511
#define ICE_MIN_S_DATA_END	12
#define ICE_MAX_S_DATA_END	4096

#define ICE_METADATA_BUF	0x80000000

struct ice_buf_hdr {
	__le16 section_count;
	__le16 data_end;
	struct ice_section_entry section_entry[STRUCT_HACK_VAR_LEN];
};

#define ICE_MAX_ENTRIES_IN_BUF(hd_sz, ent_sz) ((ICE_PKG_BUF_SIZE - \
	ice_struct_size((struct ice_buf_hdr *)0, section_entry, 1) - (hd_sz)) /\
	(ent_sz))

/* ice package section IDs */
#define ICE_SID_METADATA		1
#define ICE_SID_XLT0_SW			10
#define ICE_SID_XLT_KEY_BUILDER_SW	11
#define ICE_SID_XLT1_SW			12
#define ICE_SID_XLT2_SW			13
#define ICE_SID_PROFID_TCAM_SW		14
#define ICE_SID_PROFID_REDIR_SW		15
#define ICE_SID_FLD_VEC_SW		16
#define ICE_SID_CDID_KEY_BUILDER_SW	17
#define ICE_SID_CDID_REDIR_SW		18

#define ICE_SID_XLT0_ACL		20
#define ICE_SID_XLT_KEY_BUILDER_ACL	21
#define ICE_SID_XLT1_ACL		22
#define ICE_SID_XLT2_ACL		23
#define ICE_SID_PROFID_TCAM_ACL		24
#define ICE_SID_PROFID_REDIR_ACL	25
#define ICE_SID_FLD_VEC_ACL		26
#define ICE_SID_CDID_KEY_BUILDER_ACL	27
#define ICE_SID_CDID_REDIR_ACL		28

#define ICE_SID_XLT0_FD			30
#define ICE_SID_XLT_KEY_BUILDER_FD	31
#define ICE_SID_XLT1_FD			32
#define ICE_SID_XLT2_FD			33
#define ICE_SID_PROFID_TCAM_FD		34
#define ICE_SID_PROFID_REDIR_FD		35
#define ICE_SID_FLD_VEC_FD		36
#define ICE_SID_CDID_KEY_BUILDER_FD	37
#define ICE_SID_CDID_REDIR_FD		38

#define ICE_SID_XLT0_RSS		40
#define ICE_SID_XLT_KEY_BUILDER_RSS	41
#define ICE_SID_XLT1_RSS		42
#define ICE_SID_XLT2_RSS		43
#define ICE_SID_PROFID_TCAM_RSS		44
#define ICE_SID_PROFID_REDIR_RSS	45
#define ICE_SID_FLD_VEC_RSS		46
#define ICE_SID_CDID_KEY_BUILDER_RSS	47
#define ICE_SID_CDID_REDIR_RSS		48

#define ICE_SID_RXPARSER_CAM		50
#define ICE_SID_RXPARSER_NOMATCH_CAM	51
#define ICE_SID_RXPARSER_IMEM		52
#define ICE_SID_RXPARSER_XLT0_BUILDER	53
#define ICE_SID_RXPARSER_NODE_PTYPE	54
#define ICE_SID_RXPARSER_MARKER_PTYPE	55
#define ICE_SID_RXPARSER_BOOST_TCAM	56
#define ICE_SID_RXPARSER_PROTO_GRP	57
#define ICE_SID_RXPARSER_METADATA_INIT	58
#define ICE_SID_RXPARSER_XLT0		59

#define ICE_SID_TXPARSER_CAM		60
#define ICE_SID_TXPARSER_NOMATCH_CAM	61
#define ICE_SID_TXPARSER_IMEM		62
#define ICE_SID_TXPARSER_XLT0_BUILDER	63
#define ICE_SID_TXPARSER_NODE_PTYPE	64
#define ICE_SID_TXPARSER_MARKER_PTYPE	65
#define ICE_SID_TXPARSER_BOOST_TCAM	66
#define ICE_SID_TXPARSER_PROTO_GRP	67
#define ICE_SID_TXPARSER_METADATA_INIT	68
#define ICE_SID_TXPARSER_XLT0		69

#define ICE_SID_RXPARSER_INIT_REDIR	70
#define ICE_SID_TXPARSER_INIT_REDIR	71
#define ICE_SID_RXPARSER_MARKER_GRP	72
#define ICE_SID_TXPARSER_MARKER_GRP	73
#define ICE_SID_RXPARSER_LAST_PROTO	74
#define ICE_SID_TXPARSER_LAST_PROTO	75
#define ICE_SID_RXPARSER_PG_SPILL	76
#define ICE_SID_TXPARSER_PG_SPILL	77
#define ICE_SID_RXPARSER_NOMATCH_SPILL	78
#define ICE_SID_TXPARSER_NOMATCH_SPILL	79

#define ICE_SID_XLT0_PE			80
#define ICE_SID_XLT_KEY_BUILDER_PE	81
#define ICE_SID_XLT1_PE			82
#define ICE_SID_XLT2_PE			83
#define ICE_SID_PROFID_TCAM_PE		84
#define ICE_SID_PROFID_REDIR_PE		85
#define ICE_SID_FLD_VEC_PE		86
#define ICE_SID_CDID_KEY_BUILDER_PE	87
#define ICE_SID_CDID_REDIR_PE		88

#define ICE_SID_RXPARSER_FLAG_REDIR	97

/* Label Metadata section IDs */
#define ICE_SID_LBL_FIRST		0x80000010
#define ICE_SID_LBL_RXPARSER_IMEM	0x80000010
#define ICE_SID_LBL_TXPARSER_IMEM	0x80000011
#define ICE_SID_LBL_RESERVED_12		0x80000012
#define ICE_SID_LBL_RESERVED_13		0x80000013
#define ICE_SID_LBL_RXPARSER_MARKER	0x80000014
#define ICE_SID_LBL_TXPARSER_MARKER	0x80000015
#define ICE_SID_LBL_PTYPE		0x80000016
#define ICE_SID_LBL_PROTOCOL_ID		0x80000017
#define ICE_SID_LBL_RXPARSER_TMEM	0x80000018
#define ICE_SID_LBL_TXPARSER_TMEM	0x80000019
#define ICE_SID_LBL_RXPARSER_PG		0x8000001A
#define ICE_SID_LBL_TXPARSER_PG		0x8000001B
#define ICE_SID_LBL_RXPARSER_M_TCAM	0x8000001C
#define ICE_SID_LBL_TXPARSER_M_TCAM	0x8000001D
#define ICE_SID_LBL_SW_PROFID_TCAM	0x8000001E
#define ICE_SID_LBL_ACL_PROFID_TCAM	0x8000001F
#define ICE_SID_LBL_PE_PROFID_TCAM	0x80000020
#define ICE_SID_LBL_RSS_PROFID_TCAM	0x80000021
#define ICE_SID_LBL_FD_PROFID_TCAM	0x80000022
#define ICE_SID_LBL_FLAG		0x80000023
#define ICE_SID_LBL_REG			0x80000024
#define ICE_SID_LBL_SW_PTG		0x80000025
#define ICE_SID_LBL_ACL_PTG		0x80000026
#define ICE_SID_LBL_PE_PTG		0x80000027
#define ICE_SID_LBL_RSS_PTG		0x80000028
#define ICE_SID_LBL_FD_PTG		0x80000029
#define ICE_SID_LBL_SW_VSIG		0x8000002A
#define ICE_SID_LBL_ACL_VSIG		0x8000002B
#define ICE_SID_LBL_PE_VSIG		0x8000002C
#define ICE_SID_LBL_RSS_VSIG		0x8000002D
#define ICE_SID_LBL_FD_VSIG		0x8000002E
#define ICE_SID_LBL_PTYPE_META		0x8000002F
#define ICE_SID_LBL_SW_PROFID		0x80000030
#define ICE_SID_LBL_ACL_PROFID		0x80000031
#define ICE_SID_LBL_PE_PROFID		0x80000032
#define ICE_SID_LBL_RSS_PROFID		0x80000033
#define ICE_SID_LBL_FD_PROFID		0x80000034
#define ICE_SID_LBL_RXPARSER_MARKER_GRP	0x80000035
#define ICE_SID_LBL_TXPARSER_MARKER_GRP	0x80000036
#define ICE_SID_LBL_RXPARSER_PROTO	0x80000037
#define ICE_SID_LBL_TXPARSER_PROTO	0x80000038
/* The following define MUST be updated to reflect the last label section ID */
#define ICE_SID_LBL_LAST		0x80000038

/* Label ICE runtime configuration section IDs */
#define ICE_SID_TX_5_LAYER_TOPO		0x10

enum ice_block {
	ICE_BLK_SW = 0,
	ICE_BLK_ACL,
	ICE_BLK_FD,
	ICE_BLK_RSS,
	ICE_BLK_PE,
	ICE_BLK_COUNT
};

enum ice_sect {
	ICE_XLT0 = 0,
	ICE_XLT_KB,
	ICE_XLT1,
	ICE_XLT2,
	ICE_PROF_TCAM,
	ICE_PROF_REDIR,
	ICE_VEC_TBL,
	ICE_CDID_KB,
	ICE_CDID_REDIR,
	ICE_SECT_COUNT
};

/* package buffer building */

struct ice_buf_build {
	struct ice_buf buf;
	u16 reserved_section_table_entries;
};

struct ice_pkg_enum {
	struct ice_buf_table *buf_table;
	u32 buf_idx;

	u32 type;
	struct ice_buf_hdr *buf;
	u32 sect_idx;
	void *sect;
	u32 sect_type;

	u32 entry_idx;
	void *(*handler)(u32 sect_type, void *section, u32 index, u32 *offset);
};

struct ice_hw;

enum ice_status
ice_acquire_change_lock(struct ice_hw *hw, enum ice_aq_res_access_type access);
void ice_release_change_lock(struct ice_hw *hw);

struct ice_buf_build *ice_pkg_buf_alloc(struct ice_hw *hw);
void *
ice_pkg_buf_alloc_section(struct ice_buf_build *bld, u32 type, u16 size);
enum ice_status
ice_pkg_buf_reserve_section(struct ice_buf_build *bld, u16 count);
enum ice_status
ice_get_sw_fv_list(struct ice_hw *hw, struct ice_prot_lkup_ext *lkups,
		   ice_bitmap_t *bm, struct LIST_HEAD_TYPE *fv_list);
enum ice_status
ice_pkg_buf_unreserve_section(struct ice_buf_build *bld, u16 count);
u16 ice_pkg_buf_get_free_space(struct ice_buf_build *bld);
u16 ice_pkg_buf_get_active_sections(struct ice_buf_build *bld);

enum ice_status
ice_update_pkg(struct ice_hw *hw, struct ice_buf *bufs, u32 count);
enum ice_status
ice_update_pkg_no_lock(struct ice_hw *hw, struct ice_buf *bufs, u32 count);
void ice_release_global_cfg_lock(struct ice_hw *hw);
struct ice_generic_seg_hdr *
ice_find_seg_in_pkg(struct ice_hw *hw, u32 seg_type,
		    struct ice_pkg_hdr *pkg_hdr);
enum ice_ddp_state
ice_verify_pkg(struct ice_pkg_hdr *pkg, u32 len);
enum ice_ddp_state
ice_get_pkg_info(struct ice_hw *hw);
void ice_init_pkg_hints(struct ice_hw *hw, struct ice_seg *ice_seg);
struct ice_buf_table *ice_find_buf_table(struct ice_seg *ice_seg);
enum ice_status
ice_acquire_global_cfg_lock(struct ice_hw *hw,
			    enum ice_aq_res_access_type access);

struct ice_buf_table *ice_find_buf_table(struct ice_seg *ice_seg);
struct ice_buf_hdr *
ice_pkg_enum_buf(struct ice_seg *ice_seg, struct ice_pkg_enum *state);
bool
ice_pkg_advance_sect(struct ice_seg *ice_seg, struct ice_pkg_enum *state);
void *
ice_pkg_enum_entry(struct ice_seg *ice_seg, struct ice_pkg_enum *state,
		   u32 sect_type, u32 *offset,
		   void *(*handler)(u32 sect_type, void *section,
				    u32 index, u32 *offset));
void *
ice_pkg_enum_section(struct ice_seg *ice_seg, struct ice_pkg_enum *state,
		     u32 sect_type);
enum ice_ddp_state ice_init_pkg(struct ice_hw *hw, u8 *buff, u32 len);
enum ice_ddp_state
ice_copy_and_init_pkg(struct ice_hw *hw, const u8 *buf, u32 len);
bool ice_is_init_pkg_successful(enum ice_ddp_state state);
void ice_free_seg(struct ice_hw *hw);

struct ice_buf_build *
ice_pkg_buf_alloc_single_section(struct ice_hw *hw, u32 type, u16 size,
				 void **section);
struct ice_buf *ice_pkg_buf(struct ice_buf_build *bld);
void ice_pkg_buf_free(struct ice_hw *hw, struct ice_buf_build *bld);

enum ice_status ice_cfg_tx_topo(struct ice_hw *hw, u8 *buf, u32 len);

#endif /* _ICE_DDP_COMMON_H_ */
