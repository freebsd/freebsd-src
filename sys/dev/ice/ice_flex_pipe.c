/* SPDX-License-Identifier: BSD-3-Clause */
/*  Copyright (c) 2024, Intel Corporation
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

#include "ice_common.h"
#include "ice_ddp_common.h"
#include "ice_flex_pipe.h"
#include "ice_protocol_type.h"
#include "ice_flow.h"

static const struct ice_tunnel_type_scan tnls[] = {
	{ TNL_VXLAN,		"TNL_VXLAN_PF" },
	{ TNL_GENEVE,		"TNL_GENEVE_PF" },
	{ TNL_LAST,		"" }
};

static const u32 ice_sect_lkup[ICE_BLK_COUNT][ICE_SECT_COUNT] = {
	/* SWITCH */
	{
		ICE_SID_XLT0_SW,
		ICE_SID_XLT_KEY_BUILDER_SW,
		ICE_SID_XLT1_SW,
		ICE_SID_XLT2_SW,
		ICE_SID_PROFID_TCAM_SW,
		ICE_SID_PROFID_REDIR_SW,
		ICE_SID_FLD_VEC_SW,
		ICE_SID_CDID_KEY_BUILDER_SW,
		ICE_SID_CDID_REDIR_SW
	},

	/* ACL */
	{
		ICE_SID_XLT0_ACL,
		ICE_SID_XLT_KEY_BUILDER_ACL,
		ICE_SID_XLT1_ACL,
		ICE_SID_XLT2_ACL,
		ICE_SID_PROFID_TCAM_ACL,
		ICE_SID_PROFID_REDIR_ACL,
		ICE_SID_FLD_VEC_ACL,
		ICE_SID_CDID_KEY_BUILDER_ACL,
		ICE_SID_CDID_REDIR_ACL
	},

	/* FD */
	{
		ICE_SID_XLT0_FD,
		ICE_SID_XLT_KEY_BUILDER_FD,
		ICE_SID_XLT1_FD,
		ICE_SID_XLT2_FD,
		ICE_SID_PROFID_TCAM_FD,
		ICE_SID_PROFID_REDIR_FD,
		ICE_SID_FLD_VEC_FD,
		ICE_SID_CDID_KEY_BUILDER_FD,
		ICE_SID_CDID_REDIR_FD
	},

	/* RSS */
	{
		ICE_SID_XLT0_RSS,
		ICE_SID_XLT_KEY_BUILDER_RSS,
		ICE_SID_XLT1_RSS,
		ICE_SID_XLT2_RSS,
		ICE_SID_PROFID_TCAM_RSS,
		ICE_SID_PROFID_REDIR_RSS,
		ICE_SID_FLD_VEC_RSS,
		ICE_SID_CDID_KEY_BUILDER_RSS,
		ICE_SID_CDID_REDIR_RSS
	},

	/* PE */
	{
		ICE_SID_XLT0_PE,
		ICE_SID_XLT_KEY_BUILDER_PE,
		ICE_SID_XLT1_PE,
		ICE_SID_XLT2_PE,
		ICE_SID_PROFID_TCAM_PE,
		ICE_SID_PROFID_REDIR_PE,
		ICE_SID_FLD_VEC_PE,
		ICE_SID_CDID_KEY_BUILDER_PE,
		ICE_SID_CDID_REDIR_PE
	}
};

/**
 * ice_sect_id - returns section ID
 * @blk: block type
 * @sect: section type
 *
 * This helper function returns the proper section ID given a block type and a
 * section type.
 */
static u32 ice_sect_id(enum ice_block blk, enum ice_sect sect)
{
	return ice_sect_lkup[blk][sect];
}

/**
 * ice_add_tunnel_hint
 * @hw: pointer to the HW structure
 * @label_name: label text
 * @val: value of the tunnel port boost entry
 */
void ice_add_tunnel_hint(struct ice_hw *hw, char *label_name, u16 val)
{
	if (hw->tnl.count < ICE_TUNNEL_MAX_ENTRIES) {
		u16 i;

		for (i = 0; tnls[i].type != TNL_LAST; i++) {
			size_t len = strlen(tnls[i].label_prefix);

			/* Look for matching label start, before continuing */
			if (strncmp(label_name, tnls[i].label_prefix, len))
				continue;

			/* Make sure this label matches our PF. Note that the PF
			 * character ('0' - '7') will be located where our
			 * prefix string's null terminator is located.
			 */
			if ((label_name[len] - '0') == hw->pf_id) {
				hw->tnl.tbl[hw->tnl.count].type = tnls[i].type;
				hw->tnl.tbl[hw->tnl.count].valid = false;
				hw->tnl.tbl[hw->tnl.count].in_use = false;
				hw->tnl.tbl[hw->tnl.count].marked = false;
				hw->tnl.tbl[hw->tnl.count].boost_addr = val;
				hw->tnl.tbl[hw->tnl.count].port = 0;
				hw->tnl.count++;
				break;
			}
		}
	}
}

/* Key creation */

#define ICE_DC_KEY	0x1	/* don't care */
#define ICE_DC_KEYINV	0x1
#define ICE_NM_KEY	0x0	/* never match */
#define ICE_NM_KEYINV	0x0
#define ICE_0_KEY	0x1	/* match 0 */
#define ICE_0_KEYINV	0x0
#define ICE_1_KEY	0x0	/* match 1 */
#define ICE_1_KEYINV	0x1

/**
 * ice_gen_key_word - generate 16-bits of a key/mask word
 * @val: the value
 * @valid: valid bits mask (change only the valid bits)
 * @dont_care: don't care mask
 * @nvr_mtch: never match mask
 * @key: pointer to an array of where the resulting key portion
 * @key_inv: pointer to an array of where the resulting key invert portion
 *
 * This function generates 16-bits from a 8-bit value, an 8-bit don't care mask
 * and an 8-bit never match mask. The 16-bits of output are divided into 8 bits
 * of key and 8 bits of key invert.
 *
 *     '0' =    b01, always match a 0 bit
 *     '1' =    b10, always match a 1 bit
 *     '?' =    b11, don't care bit (always matches)
 *     '~' =    b00, never match bit
 *
 * Input:
 *          val:         b0  1  0  1  0  1
 *          dont_care:   b0  0  1  1  0  0
 *          never_mtch:  b0  0  0  0  1  1
 *          ------------------------------
 * Result:  key:        b01 10 11 11 00 00
 */
static enum ice_status
ice_gen_key_word(u8 val, u8 valid, u8 dont_care, u8 nvr_mtch, u8 *key,
		 u8 *key_inv)
{
	u8 in_key = *key, in_key_inv = *key_inv;
	u8 i;

	/* 'dont_care' and 'nvr_mtch' masks cannot overlap */
	if ((dont_care ^ nvr_mtch) != (dont_care | nvr_mtch))
		return ICE_ERR_CFG;

	*key = 0;
	*key_inv = 0;

	/* encode the 8 bits into 8-bit key and 8-bit key invert */
	for (i = 0; i < 8; i++) {
		*key >>= 1;
		*key_inv >>= 1;

		if (!(valid & 0x1)) { /* change only valid bits */
			*key |= (in_key & 0x1) << 7;
			*key_inv |= (in_key_inv & 0x1) << 7;
		} else if (dont_care & 0x1) { /* don't care bit */
			*key |= ICE_DC_KEY << 7;
			*key_inv |= ICE_DC_KEYINV << 7;
		} else if (nvr_mtch & 0x1) { /* never match bit */
			*key |= ICE_NM_KEY << 7;
			*key_inv |= ICE_NM_KEYINV << 7;
		} else if (val & 0x01) { /* exact 1 match */
			*key |= ICE_1_KEY << 7;
			*key_inv |= ICE_1_KEYINV << 7;
		} else { /* exact 0 match */
			*key |= ICE_0_KEY << 7;
			*key_inv |= ICE_0_KEYINV << 7;
		}

		dont_care >>= 1;
		nvr_mtch >>= 1;
		valid >>= 1;
		val >>= 1;
		in_key >>= 1;
		in_key_inv >>= 1;
	}

	return ICE_SUCCESS;
}

/**
 * ice_bits_max_set - determine if the number of bits set is within a maximum
 * @mask: pointer to the byte array which is the mask
 * @size: the number of bytes in the mask
 * @max: the max number of set bits
 *
 * This function determines if there are at most 'max' number of bits set in an
 * array. Returns true if the number for bits set is <= max or will return false
 * otherwise.
 */
static bool ice_bits_max_set(const u8 *mask, u16 size, u16 max)
{
	u16 count = 0;
	u16 i;

	/* check each byte */
	for (i = 0; i < size; i++) {
		/* if 0, go to next byte */
		if (!mask[i])
			continue;

		/* We know there is at least one set bit in this byte because of
		 * the above check; if we already have found 'max' number of
		 * bits set, then we can return failure now.
		 */
		if (count == max)
			return false;

		/* count the bits in this byte, checking threshold */
		count += ice_hweight8(mask[i]);
		if (count > max)
			return false;
	}

	return true;
}

/**
 * ice_set_key - generate a variable sized key with multiples of 16-bits
 * @key: pointer to where the key will be stored
 * @size: the size of the complete key in bytes (must be even)
 * @val: array of 8-bit values that makes up the value portion of the key
 * @upd: array of 8-bit masks that determine what key portion to update
 * @dc: array of 8-bit masks that make up the don't care mask
 * @nm: array of 8-bit masks that make up the never match mask
 * @off: the offset of the first byte in the key to update
 * @len: the number of bytes in the key update
 *
 * This function generates a key from a value, a don't care mask and a never
 * match mask.
 * upd, dc, and nm are optional parameters, and can be NULL:
 *	upd == NULL --> upd mask is all 1's (update all bits)
 *	dc == NULL --> dc mask is all 0's (no don't care bits)
 *	nm == NULL --> nm mask is all 0's (no never match bits)
 */
static enum ice_status
ice_set_key(u8 *key, u16 size, u8 *val, u8 *upd, u8 *dc, u8 *nm, u16 off,
	    u16 len)
{
	u16 half_size;
	u16 i;

	/* size must be a multiple of 2 bytes. */
	if (size % 2)
		return ICE_ERR_CFG;
	half_size = size / 2;

	if (off + len > half_size)
		return ICE_ERR_CFG;

	/* Make sure at most one bit is set in the never match mask. Having more
	 * than one never match mask bit set will cause HW to consume excessive
	 * power otherwise; this is a power management efficiency check.
	 */
#define ICE_NVR_MTCH_BITS_MAX	1
	if (nm && !ice_bits_max_set(nm, len, ICE_NVR_MTCH_BITS_MAX))
		return ICE_ERR_CFG;

	for (i = 0; i < len; i++)
		if (ice_gen_key_word(val[i], upd ? upd[i] : 0xff,
				     dc ? dc[i] : 0, nm ? nm[i] : 0,
				     key + off + i, key + half_size + off + i))
			return ICE_ERR_CFG;

	return ICE_SUCCESS;
}

/**
 * ice_tunnel_port_in_use_hlpr - helper function to determine tunnel usage
 * @hw: pointer to the HW structure
 * @port: port to search for
 * @index: optionally returns index
 *
 * Returns whether a port is already in use as a tunnel, and optionally its
 * index
 */
static bool ice_tunnel_port_in_use_hlpr(struct ice_hw *hw, u16 port, u16 *index)
{
	u16 i;

	for (i = 0; i < hw->tnl.count && i < ICE_TUNNEL_MAX_ENTRIES; i++)
		if (hw->tnl.tbl[i].in_use && hw->tnl.tbl[i].port == port) {
			if (index)
				*index = i;
			return true;
		}

	return false;
}

/**
 * ice_tunnel_port_in_use
 * @hw: pointer to the HW structure
 * @port: port to search for
 * @index: optionally returns index
 *
 * Returns whether a port is already in use as a tunnel, and optionally its
 * index
 */
bool ice_tunnel_port_in_use(struct ice_hw *hw, u16 port, u16 *index)
{
	bool res;

	ice_acquire_lock(&hw->tnl_lock);
	res = ice_tunnel_port_in_use_hlpr(hw, port, index);
	ice_release_lock(&hw->tnl_lock);

	return res;
}

/**
 * ice_tunnel_get_type
 * @hw: pointer to the HW structure
 * @port: port to search for
 * @type: returns tunnel index
 *
 * For a given port number, will return the type of tunnel.
 */
bool
ice_tunnel_get_type(struct ice_hw *hw, u16 port, enum ice_tunnel_type *type)
{
	bool res = false;
	u16 i;

	ice_acquire_lock(&hw->tnl_lock);

	for (i = 0; i < hw->tnl.count && i < ICE_TUNNEL_MAX_ENTRIES; i++)
		if (hw->tnl.tbl[i].in_use && hw->tnl.tbl[i].port == port) {
			*type = hw->tnl.tbl[i].type;
			res = true;
			break;
		}

	ice_release_lock(&hw->tnl_lock);

	return res;
}

/**
 * ice_find_free_tunnel_entry
 * @hw: pointer to the HW structure
 * @type: tunnel type
 * @index: optionally returns index
 *
 * Returns whether there is a free tunnel entry, and optionally its index
 */
static bool
ice_find_free_tunnel_entry(struct ice_hw *hw, enum ice_tunnel_type type,
			   u16 *index)
{
	u16 i;

	for (i = 0; i < hw->tnl.count && i < ICE_TUNNEL_MAX_ENTRIES; i++)
		if (hw->tnl.tbl[i].valid && !hw->tnl.tbl[i].in_use &&
		    hw->tnl.tbl[i].type == type) {
			if (index)
				*index = i;
			return true;
		}

	return false;
}

/**
 * ice_get_open_tunnel_port - retrieve an open tunnel port
 * @hw: pointer to the HW structure
 * @type: tunnel type (TNL_ALL will return any open port)
 * @port: returns open port
 */
bool
ice_get_open_tunnel_port(struct ice_hw *hw, enum ice_tunnel_type type,
			 u16 *port)
{
	bool res = false;
	u16 i;

	ice_acquire_lock(&hw->tnl_lock);

	for (i = 0; i < hw->tnl.count && i < ICE_TUNNEL_MAX_ENTRIES; i++)
		if (hw->tnl.tbl[i].valid && hw->tnl.tbl[i].in_use &&
		    (type == TNL_ALL || hw->tnl.tbl[i].type == type)) {
			*port = hw->tnl.tbl[i].port;
			res = true;
			break;
		}

	ice_release_lock(&hw->tnl_lock);

	return res;
}

/**
 * ice_create_tunnel
 * @hw: pointer to the HW structure
 * @type: type of tunnel
 * @port: port of tunnel to create
 *
 * Create a tunnel by updating the parse graph in the parser. We do that by
 * creating a package buffer with the tunnel info and issuing an update package
 * command.
 */
enum ice_status
ice_create_tunnel(struct ice_hw *hw, enum ice_tunnel_type type, u16 port)
{
	struct ice_boost_tcam_section *sect_rx, *sect_tx;
	enum ice_status status = ICE_ERR_MAX_LIMIT;
	struct ice_buf_build *bld;
	u16 index;

	ice_acquire_lock(&hw->tnl_lock);

	if (ice_tunnel_port_in_use_hlpr(hw, port, &index)) {
		hw->tnl.tbl[index].ref++;
		status = ICE_SUCCESS;
		goto ice_create_tunnel_end;
	}

	if (!ice_find_free_tunnel_entry(hw, type, &index)) {
		status = ICE_ERR_OUT_OF_RANGE;
		goto ice_create_tunnel_end;
	}

	bld = ice_pkg_buf_alloc(hw);
	if (!bld) {
		status = ICE_ERR_NO_MEMORY;
		goto ice_create_tunnel_end;
	}

	/* allocate 2 sections, one for Rx parser, one for Tx parser */
	if (ice_pkg_buf_reserve_section(bld, 2))
		goto ice_create_tunnel_err;

	sect_rx = (struct ice_boost_tcam_section *)
		ice_pkg_buf_alloc_section(bld, ICE_SID_RXPARSER_BOOST_TCAM,
					  ice_struct_size(sect_rx, tcam, 1));
	if (!sect_rx)
		goto ice_create_tunnel_err;
	sect_rx->count = CPU_TO_LE16(1);

	sect_tx = (struct ice_boost_tcam_section *)
		ice_pkg_buf_alloc_section(bld, ICE_SID_TXPARSER_BOOST_TCAM,
					  ice_struct_size(sect_tx, tcam, 1));
	if (!sect_tx)
		goto ice_create_tunnel_err;
	sect_tx->count = CPU_TO_LE16(1);

	/* copy original boost entry to update package buffer */
	ice_memcpy(sect_rx->tcam, hw->tnl.tbl[index].boost_entry,
		   sizeof(*sect_rx->tcam), ICE_NONDMA_TO_NONDMA);

	/* over-write the never-match dest port key bits with the encoded port
	 * bits
	 */
	ice_set_key((u8 *)&sect_rx->tcam[0].key, sizeof(sect_rx->tcam[0].key),
		    (u8 *)&port, NULL, NULL, NULL,
		    (u16)offsetof(struct ice_boost_key_value, hv_dst_port_key),
		    sizeof(sect_rx->tcam[0].key.key.hv_dst_port_key));

	/* exact copy of entry to Tx section entry */
	ice_memcpy(sect_tx->tcam, sect_rx->tcam, sizeof(*sect_tx->tcam),
		   ICE_NONDMA_TO_NONDMA);

	status = ice_update_pkg(hw, ice_pkg_buf(bld), 1);
	if (!status) {
		hw->tnl.tbl[index].port = port;
		hw->tnl.tbl[index].in_use = true;
		hw->tnl.tbl[index].ref = 1;
	}

ice_create_tunnel_err:
	ice_pkg_buf_free(hw, bld);

ice_create_tunnel_end:
	ice_release_lock(&hw->tnl_lock);

	return status;
}

/**
 * ice_destroy_tunnel
 * @hw: pointer to the HW structure
 * @port: port of tunnel to destroy (ignored if the all parameter is true)
 * @all: flag that states to destroy all tunnels
 *
 * Destroys a tunnel or all tunnels by creating an update package buffer
 * targeting the specific updates requested and then performing an update
 * package.
 */
enum ice_status ice_destroy_tunnel(struct ice_hw *hw, u16 port, bool all)
{
	struct ice_boost_tcam_section *sect_rx, *sect_tx;
	enum ice_status status = ICE_ERR_MAX_LIMIT;
	struct ice_buf_build *bld;
	u16 count = 0;
	u16 index;
	u16 size;
	u16 i, j;

	ice_acquire_lock(&hw->tnl_lock);

	if (!all && ice_tunnel_port_in_use_hlpr(hw, port, &index))
		if (hw->tnl.tbl[index].ref > 1) {
			hw->tnl.tbl[index].ref--;
			status = ICE_SUCCESS;
			goto ice_destroy_tunnel_end;
		}

	/* determine count */
	for (i = 0; i < hw->tnl.count && i < ICE_TUNNEL_MAX_ENTRIES; i++)
		if (hw->tnl.tbl[i].valid && hw->tnl.tbl[i].in_use &&
		    (all || hw->tnl.tbl[i].port == port))
			count++;

	if (!count) {
		status = ICE_ERR_PARAM;
		goto ice_destroy_tunnel_end;
	}

	/* size of section - there is at least one entry */
	size = ice_struct_size(sect_rx, tcam, count);

	bld = ice_pkg_buf_alloc(hw);
	if (!bld) {
		status = ICE_ERR_NO_MEMORY;
		goto ice_destroy_tunnel_end;
	}

	/* allocate 2 sections, one for Rx parser, one for Tx parser */
	if (ice_pkg_buf_reserve_section(bld, 2))
		goto ice_destroy_tunnel_err;

	sect_rx = (struct ice_boost_tcam_section *)
		ice_pkg_buf_alloc_section(bld, ICE_SID_RXPARSER_BOOST_TCAM,
					  size);
	if (!sect_rx)
		goto ice_destroy_tunnel_err;
	sect_rx->count = CPU_TO_LE16(count);

	sect_tx = (struct ice_boost_tcam_section *)
		ice_pkg_buf_alloc_section(bld, ICE_SID_TXPARSER_BOOST_TCAM,
					  size);
	if (!sect_tx)
		goto ice_destroy_tunnel_err;
	sect_tx->count = CPU_TO_LE16(count);

	/* copy original boost entry to update package buffer, one copy to Rx
	 * section, another copy to the Tx section
	 */
	for (i = 0, j = 0; i < hw->tnl.count && i < ICE_TUNNEL_MAX_ENTRIES; i++)
		if (hw->tnl.tbl[i].valid && hw->tnl.tbl[i].in_use &&
		    (all || hw->tnl.tbl[i].port == port)) {
			ice_memcpy(sect_rx->tcam + j,
				   hw->tnl.tbl[i].boost_entry,
				   sizeof(*sect_rx->tcam),
				   ICE_NONDMA_TO_NONDMA);
			ice_memcpy(sect_tx->tcam + j,
				   hw->tnl.tbl[i].boost_entry,
				   sizeof(*sect_tx->tcam),
				   ICE_NONDMA_TO_NONDMA);
			hw->tnl.tbl[i].marked = true;
			j++;
		}

	status = ice_update_pkg(hw, ice_pkg_buf(bld), 1);
	if (!status)
		for (i = 0; i < hw->tnl.count &&
		     i < ICE_TUNNEL_MAX_ENTRIES; i++)
			if (hw->tnl.tbl[i].marked) {
				hw->tnl.tbl[i].ref = 0;
				hw->tnl.tbl[i].port = 0;
				hw->tnl.tbl[i].in_use = false;
				hw->tnl.tbl[i].marked = false;
			}

ice_destroy_tunnel_err:
	ice_pkg_buf_free(hw, bld);

ice_destroy_tunnel_end:
	ice_release_lock(&hw->tnl_lock);

	return status;
}

/**
 * ice_replay_tunnels
 * @hw: pointer to the HW structure
 *
 * Replays all tunnels
 */
enum ice_status ice_replay_tunnels(struct ice_hw *hw)
{
	enum ice_status status = ICE_SUCCESS;
	u16 i;

	ice_debug(hw, ICE_DBG_TRACE, "%s\n", __func__);

	for (i = 0; i < hw->tnl.count && i < ICE_TUNNEL_MAX_ENTRIES; i++) {
		enum ice_tunnel_type type = hw->tnl.tbl[i].type;
		u16 refs = hw->tnl.tbl[i].ref;
		u16 port = hw->tnl.tbl[i].port;

		if (!hw->tnl.tbl[i].in_use)
			continue;

		/* Replay tunnels one at a time by destroying them, then
		 * recreating them
		 */
		hw->tnl.tbl[i].ref = 1; /* make sure to destroy in one call */
		status = ice_destroy_tunnel(hw, port, false);
		if (status) {
			ice_debug(hw, ICE_DBG_PKG, "ERR: 0x%x - destroy tunnel port 0x%x\n",
				  status, port);
			break;
		}

		status = ice_create_tunnel(hw, type, port);
		if (status) {
			ice_debug(hw, ICE_DBG_PKG, "ERR: 0x%x - create tunnel port 0x%x\n",
				  status, port);
			break;
		}

		/* reset to original ref count */
		hw->tnl.tbl[i].ref = refs;
	}

	return status;
}

/**
 * ice_find_prot_off - find prot ID and offset pair, based on prof and FV index
 * @hw: pointer to the hardware structure
 * @blk: hardware block
 * @prof: profile ID
 * @fv_idx: field vector word index
 * @prot: variable to receive the protocol ID
 * @off: variable to receive the protocol offset
 */
enum ice_status
ice_find_prot_off(struct ice_hw *hw, enum ice_block blk, u8 prof, u16 fv_idx,
		  u8 *prot, u16 *off)
{
	struct ice_fv_word *fv_ext;

	if (prof >= hw->blk[blk].es.count)
		return ICE_ERR_PARAM;

	if (fv_idx >= hw->blk[blk].es.fvw)
		return ICE_ERR_PARAM;

	fv_ext = hw->blk[blk].es.t + (prof * hw->blk[blk].es.fvw);

	*prot = fv_ext[fv_idx].prot_id;
	*off = fv_ext[fv_idx].off;

	return ICE_SUCCESS;
}

/* PTG Management */

/**
 * ice_ptg_update_xlt1 - Updates packet type groups in HW via XLT1 table
 * @hw: pointer to the hardware structure
 * @blk: HW block
 *
 * This function will update the XLT1 hardware table to reflect the new
 * packet type group configuration.
 */
enum ice_status ice_ptg_update_xlt1(struct ice_hw *hw, enum ice_block blk)
{
	struct ice_xlt1_section *sect;
	struct ice_buf_build *bld;
	enum ice_status status;
	u16 index;

	bld = ice_pkg_buf_alloc_single_section(hw, ice_sect_id(blk, ICE_XLT1),
					       ice_struct_size(sect, value,
							       ICE_XLT1_CNT),
					       (void **)&sect);
	if (!bld)
		return ICE_ERR_NO_MEMORY;

	sect->count = CPU_TO_LE16(ICE_XLT1_CNT);
	sect->offset = CPU_TO_LE16(0);
	for (index = 0; index < ICE_XLT1_CNT; index++)
		sect->value[index] = hw->blk[blk].xlt1.ptypes[index].ptg;

	status = ice_update_pkg(hw, ice_pkg_buf(bld), 1);

	ice_pkg_buf_free(hw, bld);

	return status;
}

/**
 * ice_ptg_find_ptype - Search for packet type group using packet type (ptype)
 * @hw: pointer to the hardware structure
 * @blk: HW block
 * @ptype: the ptype to search for
 * @ptg: pointer to variable that receives the PTG
 *
 * This function will search the PTGs for a particular ptype, returning the
 * PTG ID that contains it through the PTG parameter, with the value of
 * ICE_DEFAULT_PTG (0) meaning it is part the default PTG.
 */
static enum ice_status
ice_ptg_find_ptype(struct ice_hw *hw, enum ice_block blk, u16 ptype, u8 *ptg)
{
	if (ptype >= ICE_XLT1_CNT || !ptg)
		return ICE_ERR_PARAM;

	*ptg = hw->blk[blk].xlt1.ptypes[ptype].ptg;
	return ICE_SUCCESS;
}

/**
 * ice_ptg_alloc_val - Allocates a new packet type group ID by value
 * @hw: pointer to the hardware structure
 * @blk: HW block
 * @ptg: the PTG to allocate
 *
 * This function allocates a given packet type group ID specified by the PTG
 * parameter.
 */
static void ice_ptg_alloc_val(struct ice_hw *hw, enum ice_block blk, u8 ptg)
{
	hw->blk[blk].xlt1.ptg_tbl[ptg].in_use = true;
}

/**
 * ice_ptg_free - Frees a packet type group
 * @hw: pointer to the hardware structure
 * @blk: HW block
 * @ptg: the PTG ID to free
 *
 * This function frees a packet type group, and returns all the current ptypes
 * within it to the default PTG.
 */
void ice_ptg_free(struct ice_hw *hw, enum ice_block blk, u8 ptg)
{
	struct ice_ptg_ptype *p, *temp;

	hw->blk[blk].xlt1.ptg_tbl[ptg].in_use = false;
	p = hw->blk[blk].xlt1.ptg_tbl[ptg].first_ptype;
	while (p) {
		p->ptg = ICE_DEFAULT_PTG;
		temp = p->next_ptype;
		p->next_ptype = NULL;
		p = temp;
	}

	hw->blk[blk].xlt1.ptg_tbl[ptg].first_ptype = NULL;
}

/**
 * ice_ptg_remove_ptype - Removes ptype from a particular packet type group
 * @hw: pointer to the hardware structure
 * @blk: HW block
 * @ptype: the ptype to remove
 * @ptg: the PTG to remove the ptype from
 *
 * This function will remove the ptype from the specific PTG, and move it to
 * the default PTG (ICE_DEFAULT_PTG).
 */
static enum ice_status
ice_ptg_remove_ptype(struct ice_hw *hw, enum ice_block blk, u16 ptype, u8 ptg)
{
	struct ice_ptg_ptype **ch;
	struct ice_ptg_ptype *p;

	if (ptype > ICE_XLT1_CNT - 1)
		return ICE_ERR_PARAM;

	if (!hw->blk[blk].xlt1.ptg_tbl[ptg].in_use)
		return ICE_ERR_DOES_NOT_EXIST;

	/* Should not happen if .in_use is set, bad config */
	if (!hw->blk[blk].xlt1.ptg_tbl[ptg].first_ptype)
		return ICE_ERR_CFG;

	/* find the ptype within this PTG, and bypass the link over it */
	p = hw->blk[blk].xlt1.ptg_tbl[ptg].first_ptype;
	ch = &hw->blk[blk].xlt1.ptg_tbl[ptg].first_ptype;
	while (p) {
		if (ptype == (p - hw->blk[blk].xlt1.ptypes)) {
			*ch = p->next_ptype;
			break;
		}

		ch = &p->next_ptype;
		p = p->next_ptype;
	}

	hw->blk[blk].xlt1.ptypes[ptype].ptg = ICE_DEFAULT_PTG;
	hw->blk[blk].xlt1.ptypes[ptype].next_ptype = NULL;

	return ICE_SUCCESS;
}

/**
 * ice_ptg_add_mv_ptype - Adds/moves ptype to a particular packet type group
 * @hw: pointer to the hardware structure
 * @blk: HW block
 * @ptype: the ptype to add or move
 * @ptg: the PTG to add or move the ptype to
 *
 * This function will either add or move a ptype to a particular PTG depending
 * on if the ptype is already part of another group. Note that using a
 * a destination PTG ID of ICE_DEFAULT_PTG (0) will move the ptype to the
 * default PTG.
 */
static enum ice_status
ice_ptg_add_mv_ptype(struct ice_hw *hw, enum ice_block blk, u16 ptype, u8 ptg)
{
	enum ice_status status;
	u8 original_ptg;

	if (ptype > ICE_XLT1_CNT - 1)
		return ICE_ERR_PARAM;

	if (!hw->blk[blk].xlt1.ptg_tbl[ptg].in_use && ptg != ICE_DEFAULT_PTG)
		return ICE_ERR_DOES_NOT_EXIST;

	status = ice_ptg_find_ptype(hw, blk, ptype, &original_ptg);
	if (status)
		return status;

	/* Is ptype already in the correct PTG? */
	if (original_ptg == ptg)
		return ICE_SUCCESS;

	/* Remove from original PTG and move back to the default PTG */
	if (original_ptg != ICE_DEFAULT_PTG)
		ice_ptg_remove_ptype(hw, blk, ptype, original_ptg);

	/* Moving to default PTG? Then we're done with this request */
	if (ptg == ICE_DEFAULT_PTG)
		return ICE_SUCCESS;

	/* Add ptype to PTG at beginning of list */
	hw->blk[blk].xlt1.ptypes[ptype].next_ptype =
		hw->blk[blk].xlt1.ptg_tbl[ptg].first_ptype;
	hw->blk[blk].xlt1.ptg_tbl[ptg].first_ptype =
		&hw->blk[blk].xlt1.ptypes[ptype];

	hw->blk[blk].xlt1.ptypes[ptype].ptg = ptg;
	hw->blk[blk].xlt1.t[ptype] = ptg;

	return ICE_SUCCESS;
}

/* Block / table size info */
struct ice_blk_size_details {
	u16 xlt1;			/* # XLT1 entries */
	u16 xlt2;			/* # XLT2 entries */
	u16 prof_tcam;			/* # profile ID TCAM entries */
	u16 prof_id;			/* # profile IDs */
	u8 prof_cdid_bits;		/* # CDID one-hot bits used in key */
	u16 prof_redir;			/* # profile redirection entries */
	u16 es;				/* # extraction sequence entries */
	u16 fvw;			/* # field vector words */
	u8 overwrite;			/* overwrite existing entries allowed */
	u8 reverse;			/* reverse FV order */
};

static const struct ice_blk_size_details blk_sizes[ICE_BLK_COUNT] = {
	/**
	 * Table Definitions
	 * XLT1 - Number of entries in XLT1 table
	 * XLT2 - Number of entries in XLT2 table
	 * TCAM - Number of entries Profile ID TCAM table
	 * CDID - Control Domain ID of the hardware block
	 * PRED - Number of entries in the Profile Redirection Table
	 * FV   - Number of entries in the Field Vector
	 * FVW  - Width (in WORDs) of the Field Vector
	 * OVR  - Overwrite existing table entries
	 * REV  - Reverse FV
	 */
	/*          XLT1        , XLT2        ,TCAM, PID,CDID,PRED,   FV, FVW */
	/*          Overwrite   , Reverse FV */
	/* SW  */ { ICE_XLT1_CNT, ICE_XLT2_CNT, 512, 256,   0,  256, 256,  48,
		    false, false },
	/* ACL */ { ICE_XLT1_CNT, ICE_XLT2_CNT, 512, 128,   0,  128, 128,  32,
		    false, false },
	/* FD  */ { ICE_XLT1_CNT, ICE_XLT2_CNT, 512, 128,   0,  128, 128,  24,
		    false, true  },
	/* RSS */ { ICE_XLT1_CNT, ICE_XLT2_CNT, 512, 128,   0,  128, 128,  24,
		    true,  true  },
	/* PE  */ { ICE_XLT1_CNT, ICE_XLT2_CNT,  64,  32,   0,   32,  32,  24,
		    false, false },
};

enum ice_sid_all {
	ICE_SID_XLT1_OFF = 0,
	ICE_SID_XLT2_OFF,
	ICE_SID_PR_OFF,
	ICE_SID_PR_REDIR_OFF,
	ICE_SID_ES_OFF,
	ICE_SID_OFF_COUNT,
};

/* Characteristic handling */

/**
 * ice_match_prop_lst - determine if properties of two lists match
 * @list1: first properties list
 * @list2: second properties list
 *
 * Count, cookies and the order must match in order to be considered equivalent.
 */
static bool
ice_match_prop_lst(struct LIST_HEAD_TYPE *list1, struct LIST_HEAD_TYPE *list2)
{
	struct ice_vsig_prof *tmp1;
	struct ice_vsig_prof *tmp2;
	u16 chk_count = 0;
	u16 count = 0;

	/* compare counts */
	LIST_FOR_EACH_ENTRY(tmp1, list1, ice_vsig_prof, list)
		count++;
	LIST_FOR_EACH_ENTRY(tmp2, list2, ice_vsig_prof, list)
		chk_count++;
	if (!count || count != chk_count)
		return false;

	tmp1 = LIST_FIRST_ENTRY(list1, struct ice_vsig_prof, list);
	tmp2 = LIST_FIRST_ENTRY(list2, struct ice_vsig_prof, list);

	/* profile cookies must compare, and in the exact same order to take
	 * into account priority
	 */
	while (count--) {
		if (tmp2->profile_cookie != tmp1->profile_cookie)
			return false;

		tmp1 = LIST_NEXT_ENTRY(tmp1, struct ice_vsig_prof, list);
		tmp2 = LIST_NEXT_ENTRY(tmp2, struct ice_vsig_prof, list);
	}

	return true;
}

/* VSIG Management */

/**
 * ice_vsig_update_xlt2_sect - update one section of XLT2 table
 * @hw: pointer to the hardware structure
 * @blk: HW block
 * @vsi: HW VSI number to program
 * @vsig: VSIG for the VSI
 *
 * This function will update the XLT2 hardware table with the input VSI
 * group configuration.
 */
static enum ice_status
ice_vsig_update_xlt2_sect(struct ice_hw *hw, enum ice_block blk, u16 vsi,
			  u16 vsig)
{
	struct ice_xlt2_section *sect;
	struct ice_buf_build *bld;
	enum ice_status status;

	bld = ice_pkg_buf_alloc_single_section(hw, ice_sect_id(blk, ICE_XLT2),
					       ice_struct_size(sect, value, 1),
					       (void **)&sect);
	if (!bld)
		return ICE_ERR_NO_MEMORY;

	sect->count = CPU_TO_LE16(1);
	sect->offset = CPU_TO_LE16(vsi);
	sect->value[0] = CPU_TO_LE16(vsig);

	status = ice_update_pkg(hw, ice_pkg_buf(bld), 1);

	ice_pkg_buf_free(hw, bld);

	return status;
}

/**
 * ice_vsig_update_xlt2 - update XLT2 table with VSIG configuration
 * @hw: pointer to the hardware structure
 * @blk: HW block
 *
 * This function will update the XLT2 hardware table with the input VSI
 * group configuration of used vsis.
 */
enum ice_status ice_vsig_update_xlt2(struct ice_hw *hw, enum ice_block blk)
{
	u16 vsi;

	for (vsi = 0; vsi < ICE_MAX_VSI; vsi++) {
		/* update only vsis that have been changed */
		if (hw->blk[blk].xlt2.vsis[vsi].changed) {
			enum ice_status status;
			u16 vsig;

			vsig = hw->blk[blk].xlt2.vsis[vsi].vsig;
			status = ice_vsig_update_xlt2_sect(hw, blk, vsi, vsig);
			if (status)
				return status;

			hw->blk[blk].xlt2.vsis[vsi].changed = 0;
		}
	}

	return ICE_SUCCESS;
}

/**
 * ice_vsig_find_vsi - find a VSIG that contains a specified VSI
 * @hw: pointer to the hardware structure
 * @blk: HW block
 * @vsi: VSI of interest
 * @vsig: pointer to receive the VSI group
 *
 * This function will lookup the VSI entry in the XLT2 list and return
 * the VSI group its associated with.
 */
enum ice_status
ice_vsig_find_vsi(struct ice_hw *hw, enum ice_block blk, u16 vsi, u16 *vsig)
{
	if (!vsig || vsi >= ICE_MAX_VSI)
		return ICE_ERR_PARAM;

	/* As long as there's a default or valid VSIG associated with the input
	 * VSI, the functions returns a success. Any handling of VSIG will be
	 * done by the following add, update or remove functions.
	 */
	*vsig = hw->blk[blk].xlt2.vsis[vsi].vsig;

	return ICE_SUCCESS;
}

/**
 * ice_vsig_alloc_val - allocate a new VSIG by value
 * @hw: pointer to the hardware structure
 * @blk: HW block
 * @vsig: the VSIG to allocate
 *
 * This function will allocate a given VSIG specified by the VSIG parameter.
 */
static u16 ice_vsig_alloc_val(struct ice_hw *hw, enum ice_block blk, u16 vsig)
{
	u16 idx = vsig & ICE_VSIG_IDX_M;

	if (!hw->blk[blk].xlt2.vsig_tbl[idx].in_use) {
		INIT_LIST_HEAD(&hw->blk[blk].xlt2.vsig_tbl[idx].prop_lst);
		hw->blk[blk].xlt2.vsig_tbl[idx].in_use = true;
	}

	return ICE_VSIG_VALUE(idx, hw->pf_id);
}

/**
 * ice_vsig_alloc - Finds a free entry and allocates a new VSIG
 * @hw: pointer to the hardware structure
 * @blk: HW block
 *
 * This function will iterate through the VSIG list and mark the first
 * unused entry for the new VSIG entry as used and return that value.
 */
static u16 ice_vsig_alloc(struct ice_hw *hw, enum ice_block blk)
{
	u16 i;

	for (i = 1; i < ICE_MAX_VSIGS; i++)
		if (!hw->blk[blk].xlt2.vsig_tbl[i].in_use)
			return ice_vsig_alloc_val(hw, blk, i);

	return ICE_DEFAULT_VSIG;
}

/**
 * ice_find_dup_props_vsig - find VSI group with a specified set of properties
 * @hw: pointer to the hardware structure
 * @blk: HW block
 * @chs: characteristic list
 * @vsig: returns the VSIG with the matching profiles, if found
 *
 * Each VSIG is associated with a characteristic set; i.e. all VSIs under
 * a group have the same characteristic set. To check if there exists a VSIG
 * which has the same characteristics as the input characteristics; this
 * function will iterate through the XLT2 list and return the VSIG that has a
 * matching configuration. In order to make sure that priorities are accounted
 * for, the list must match exactly, including the order in which the
 * characteristics are listed.
 */
static enum ice_status
ice_find_dup_props_vsig(struct ice_hw *hw, enum ice_block blk,
			struct LIST_HEAD_TYPE *chs, u16 *vsig)
{
	struct ice_xlt2 *xlt2 = &hw->blk[blk].xlt2;
	u16 i;

	for (i = 0; i < xlt2->count; i++)
		if (xlt2->vsig_tbl[i].in_use &&
		    ice_match_prop_lst(chs, &xlt2->vsig_tbl[i].prop_lst)) {
			*vsig = ICE_VSIG_VALUE(i, hw->pf_id);
			return ICE_SUCCESS;
		}

	return ICE_ERR_DOES_NOT_EXIST;
}

/**
 * ice_vsig_free - free VSI group
 * @hw: pointer to the hardware structure
 * @blk: HW block
 * @vsig: VSIG to remove
 *
 * The function will remove all VSIs associated with the input VSIG and move
 * them to the DEFAULT_VSIG and mark the VSIG available.
 */
static enum ice_status
ice_vsig_free(struct ice_hw *hw, enum ice_block blk, u16 vsig)
{
	struct ice_vsig_prof *dtmp, *del;
	struct ice_vsig_vsi *vsi_cur;
	u16 idx;

	idx = vsig & ICE_VSIG_IDX_M;
	if (idx >= ICE_MAX_VSIGS)
		return ICE_ERR_PARAM;

	if (!hw->blk[blk].xlt2.vsig_tbl[idx].in_use)
		return ICE_ERR_DOES_NOT_EXIST;

	hw->blk[blk].xlt2.vsig_tbl[idx].in_use = false;

	vsi_cur = hw->blk[blk].xlt2.vsig_tbl[idx].first_vsi;
	/* If the VSIG has at least 1 VSI then iterate through the
	 * list and remove the VSIs before deleting the group.
	 */
	if (vsi_cur) {
		/* remove all vsis associated with this VSIG XLT2 entry */
		do {
			struct ice_vsig_vsi *tmp = vsi_cur->next_vsi;

			vsi_cur->vsig = ICE_DEFAULT_VSIG;
			vsi_cur->changed = 1;
			vsi_cur->next_vsi = NULL;
			vsi_cur = tmp;
		} while (vsi_cur);

		/* NULL terminate head of VSI list */
		hw->blk[blk].xlt2.vsig_tbl[idx].first_vsi = NULL;
	}

	/* free characteristic list */
	LIST_FOR_EACH_ENTRY_SAFE(del, dtmp,
				 &hw->blk[blk].xlt2.vsig_tbl[idx].prop_lst,
				 ice_vsig_prof, list) {
		LIST_DEL(&del->list);
		ice_free(hw, del);
	}

	/* if VSIG characteristic list was cleared for reset
	 * re-initialize the list head
	 */
	INIT_LIST_HEAD(&hw->blk[blk].xlt2.vsig_tbl[idx].prop_lst);

	return ICE_SUCCESS;
}

/**
 * ice_vsig_remove_vsi - remove VSI from VSIG
 * @hw: pointer to the hardware structure
 * @blk: HW block
 * @vsi: VSI to remove
 * @vsig: VSI group to remove from
 *
 * The function will remove the input VSI from its VSI group and move it
 * to the DEFAULT_VSIG.
 */
static enum ice_status
ice_vsig_remove_vsi(struct ice_hw *hw, enum ice_block blk, u16 vsi, u16 vsig)
{
	struct ice_vsig_vsi **vsi_head, *vsi_cur, *vsi_tgt;
	u16 idx;

	idx = vsig & ICE_VSIG_IDX_M;

	if (vsi >= ICE_MAX_VSI || idx >= ICE_MAX_VSIGS)
		return ICE_ERR_PARAM;

	if (!hw->blk[blk].xlt2.vsig_tbl[idx].in_use)
		return ICE_ERR_DOES_NOT_EXIST;

	/* entry already in default VSIG, don't have to remove */
	if (idx == ICE_DEFAULT_VSIG)
		return ICE_SUCCESS;

	vsi_head = &hw->blk[blk].xlt2.vsig_tbl[idx].first_vsi;
	if (!(*vsi_head))
		return ICE_ERR_CFG;

	vsi_tgt = &hw->blk[blk].xlt2.vsis[vsi];
	vsi_cur = (*vsi_head);

	/* iterate the VSI list, skip over the entry to be removed */
	while (vsi_cur) {
		if (vsi_tgt == vsi_cur) {
			(*vsi_head) = vsi_cur->next_vsi;
			break;
		}
		vsi_head = &vsi_cur->next_vsi;
		vsi_cur = vsi_cur->next_vsi;
	}

	/* verify if VSI was removed from group list */
	if (!vsi_cur)
		return ICE_ERR_DOES_NOT_EXIST;

	vsi_cur->vsig = ICE_DEFAULT_VSIG;
	vsi_cur->changed = 1;
	vsi_cur->next_vsi = NULL;

	return ICE_SUCCESS;
}

/**
 * ice_vsig_add_mv_vsi - add or move a VSI to a VSI group
 * @hw: pointer to the hardware structure
 * @blk: HW block
 * @vsi: VSI to move
 * @vsig: destination VSI group
 *
 * This function will move or add the input VSI to the target VSIG.
 * The function will find the original VSIG the VSI belongs to and
 * move the entry to the DEFAULT_VSIG, update the original VSIG and
 * then move entry to the new VSIG.
 */
static enum ice_status
ice_vsig_add_mv_vsi(struct ice_hw *hw, enum ice_block blk, u16 vsi, u16 vsig)
{
	struct ice_vsig_vsi *tmp;
	enum ice_status status;
	u16 orig_vsig, idx;

	idx = vsig & ICE_VSIG_IDX_M;

	if (vsi >= ICE_MAX_VSI || idx >= ICE_MAX_VSIGS)
		return ICE_ERR_PARAM;

	/* if VSIG not in use and VSIG is not default type this VSIG
	 * doesn't exist.
	 */
	if (!hw->blk[blk].xlt2.vsig_tbl[idx].in_use &&
	    vsig != ICE_DEFAULT_VSIG)
		return ICE_ERR_DOES_NOT_EXIST;

	status = ice_vsig_find_vsi(hw, blk, vsi, &orig_vsig);
	if (status)
		return status;

	/* no update required if vsigs match */
	if (orig_vsig == vsig)
		return ICE_SUCCESS;

	if (orig_vsig != ICE_DEFAULT_VSIG) {
		/* remove entry from orig_vsig and add to default VSIG */
		status = ice_vsig_remove_vsi(hw, blk, vsi, orig_vsig);
		if (status)
			return status;
	}

	if (idx == ICE_DEFAULT_VSIG)
		return ICE_SUCCESS;

	/* Create VSI entry and add VSIG and prop_mask values */
	hw->blk[blk].xlt2.vsis[vsi].vsig = vsig;
	hw->blk[blk].xlt2.vsis[vsi].changed = 1;

	/* Add new entry to the head of the VSIG list */
	tmp = hw->blk[blk].xlt2.vsig_tbl[idx].first_vsi;
	hw->blk[blk].xlt2.vsig_tbl[idx].first_vsi =
		&hw->blk[blk].xlt2.vsis[vsi];
	hw->blk[blk].xlt2.vsis[vsi].next_vsi = tmp;
	hw->blk[blk].xlt2.t[vsi] = vsig;

	return ICE_SUCCESS;
}

/**
 * ice_find_prof_id - find profile ID for a given field vector
 * @hw: pointer to the hardware structure
 * @blk: HW block
 * @fv: field vector to search for
 * @prof_id: receives the profile ID
 */
static enum ice_status
ice_find_prof_id(struct ice_hw *hw, enum ice_block blk,
		 struct ice_fv_word *fv, u8 *prof_id)
{
	struct ice_es *es = &hw->blk[blk].es;
	u16 off;
	u8 i;

	for (i = 0; i < (u8)es->count; i++) {
		off = i * es->fvw;

		if (memcmp(&es->t[off], fv, es->fvw * sizeof(*fv)))
			continue;

		*prof_id = i;
		return ICE_SUCCESS;
	}

	return ICE_ERR_DOES_NOT_EXIST;
}

/**
 * ice_prof_id_rsrc_type - get profile ID resource type for a block type
 * @blk: the block type
 * @rsrc_type: pointer to variable to receive the resource type
 */
static bool ice_prof_id_rsrc_type(enum ice_block blk, u16 *rsrc_type)
{
	switch (blk) {
	case ICE_BLK_RSS:
		*rsrc_type = ICE_AQC_RES_TYPE_HASH_PROF_BLDR_PROFID;
		break;
	case ICE_BLK_PE:
		*rsrc_type = ICE_AQC_RES_TYPE_QHASH_PROF_BLDR_PROFID;
		break;
	default:
		return false;
	}
	return true;
}

/**
 * ice_tcam_ent_rsrc_type - get TCAM entry resource type for a block type
 * @blk: the block type
 * @rsrc_type: pointer to variable to receive the resource type
 */
static bool ice_tcam_ent_rsrc_type(enum ice_block blk, u16 *rsrc_type)
{
	switch (blk) {
	case ICE_BLK_RSS:
		*rsrc_type = ICE_AQC_RES_TYPE_HASH_PROF_BLDR_TCAM;
		break;
	case ICE_BLK_PE:
		*rsrc_type = ICE_AQC_RES_TYPE_QHASH_PROF_BLDR_TCAM;
		break;
	default:
		return false;
	}
	return true;
}

/**
 * ice_alloc_tcam_ent - allocate hardware TCAM entry
 * @hw: pointer to the HW struct
 * @blk: the block to allocate the TCAM for
 * @btm: true to allocate from bottom of table, false to allocate from top
 * @tcam_idx: pointer to variable to receive the TCAM entry
 *
 * This function allocates a new entry in a Profile ID TCAM for a specific
 * block.
 */
static enum ice_status
ice_alloc_tcam_ent(struct ice_hw *hw, enum ice_block blk, bool btm,
		   u16 *tcam_idx)
{
	u16 res_type;

	if (!ice_tcam_ent_rsrc_type(blk, &res_type))
		return ICE_ERR_PARAM;

	return ice_alloc_hw_res(hw, res_type, 1, btm, tcam_idx);
}

/**
 * ice_free_tcam_ent - free hardware TCAM entry
 * @hw: pointer to the HW struct
 * @blk: the block from which to free the TCAM entry
 * @tcam_idx: the TCAM entry to free
 *
 * This function frees an entry in a Profile ID TCAM for a specific block.
 */
static enum ice_status
ice_free_tcam_ent(struct ice_hw *hw, enum ice_block blk, u16 tcam_idx)
{
	u16 res_type;

	if (!ice_tcam_ent_rsrc_type(blk, &res_type))
		return ICE_ERR_PARAM;

	return ice_free_hw_res(hw, res_type, 1, &tcam_idx);
}

/**
 * ice_alloc_prof_id - allocate profile ID
 * @hw: pointer to the HW struct
 * @blk: the block to allocate the profile ID for
 * @prof_id: pointer to variable to receive the profile ID
 *
 * This function allocates a new profile ID, which also corresponds to a Field
 * Vector (Extraction Sequence) entry.
 */
static enum ice_status
ice_alloc_prof_id(struct ice_hw *hw, enum ice_block blk, u8 *prof_id)
{
	enum ice_status status;
	u16 res_type;
	u16 get_prof;

	if (!ice_prof_id_rsrc_type(blk, &res_type))
		return ICE_ERR_PARAM;

	status = ice_alloc_hw_res(hw, res_type, 1, false, &get_prof);
	if (!status)
		*prof_id = (u8)get_prof;

	return status;
}

/**
 * ice_free_prof_id - free profile ID
 * @hw: pointer to the HW struct
 * @blk: the block from which to free the profile ID
 * @prof_id: the profile ID to free
 *
 * This function frees a profile ID, which also corresponds to a Field Vector.
 */
static enum ice_status
ice_free_prof_id(struct ice_hw *hw, enum ice_block blk, u8 prof_id)
{
	u16 tmp_prof_id = (u16)prof_id;
	u16 res_type;

	if (!ice_prof_id_rsrc_type(blk, &res_type))
		return ICE_ERR_PARAM;

	return ice_free_hw_res(hw, res_type, 1, &tmp_prof_id);
}

/**
 * ice_prof_inc_ref - increment reference count for profile
 * @hw: pointer to the HW struct
 * @blk: the block from which to free the profile ID
 * @prof_id: the profile ID for which to increment the reference count
 */
static enum ice_status
ice_prof_inc_ref(struct ice_hw *hw, enum ice_block blk, u8 prof_id)
{
	if (prof_id > hw->blk[blk].es.count)
		return ICE_ERR_PARAM;

	hw->blk[blk].es.ref_count[prof_id]++;

	return ICE_SUCCESS;
}

/**
 * ice_write_es - write an extraction sequence to hardware
 * @hw: pointer to the HW struct
 * @blk: the block in which to write the extraction sequence
 * @prof_id: the profile ID to write
 * @fv: pointer to the extraction sequence to write - NULL to clear extraction
 */
static void
ice_write_es(struct ice_hw *hw, enum ice_block blk, u8 prof_id,
	     struct ice_fv_word *fv)
{
	u16 off;

	off = prof_id * hw->blk[blk].es.fvw;
	if (!fv) {
		ice_memset(&hw->blk[blk].es.t[off], 0, hw->blk[blk].es.fvw *
			   sizeof(*fv), ICE_NONDMA_MEM);
		hw->blk[blk].es.written[prof_id] = false;
	} else {
		ice_memcpy(&hw->blk[blk].es.t[off], fv, hw->blk[blk].es.fvw *
			   sizeof(*fv), ICE_NONDMA_TO_NONDMA);
	}
}

/**
 * ice_prof_dec_ref - decrement reference count for profile
 * @hw: pointer to the HW struct
 * @blk: the block from which to free the profile ID
 * @prof_id: the profile ID for which to decrement the reference count
 */
static enum ice_status
ice_prof_dec_ref(struct ice_hw *hw, enum ice_block blk, u8 prof_id)
{
	if (prof_id > hw->blk[blk].es.count)
		return ICE_ERR_PARAM;

	if (hw->blk[blk].es.ref_count[prof_id] > 0) {
		if (!--hw->blk[blk].es.ref_count[prof_id]) {
			ice_write_es(hw, blk, prof_id, NULL);
			return ice_free_prof_id(hw, blk, prof_id);
		}
	}

	return ICE_SUCCESS;
}

/* Block / table section IDs */
static const u32 ice_blk_sids[ICE_BLK_COUNT][ICE_SID_OFF_COUNT] = {
	/* SWITCH */
	{	ICE_SID_XLT1_SW,
		ICE_SID_XLT2_SW,
		ICE_SID_PROFID_TCAM_SW,
		ICE_SID_PROFID_REDIR_SW,
		ICE_SID_FLD_VEC_SW
	},

	/* ACL */
	{	ICE_SID_XLT1_ACL,
		ICE_SID_XLT2_ACL,
		ICE_SID_PROFID_TCAM_ACL,
		ICE_SID_PROFID_REDIR_ACL,
		ICE_SID_FLD_VEC_ACL
	},

	/* FD */
	{	ICE_SID_XLT1_FD,
		ICE_SID_XLT2_FD,
		ICE_SID_PROFID_TCAM_FD,
		ICE_SID_PROFID_REDIR_FD,
		ICE_SID_FLD_VEC_FD
	},

	/* RSS */
	{	ICE_SID_XLT1_RSS,
		ICE_SID_XLT2_RSS,
		ICE_SID_PROFID_TCAM_RSS,
		ICE_SID_PROFID_REDIR_RSS,
		ICE_SID_FLD_VEC_RSS
	},

	/* PE */
	{	ICE_SID_XLT1_PE,
		ICE_SID_XLT2_PE,
		ICE_SID_PROFID_TCAM_PE,
		ICE_SID_PROFID_REDIR_PE,
		ICE_SID_FLD_VEC_PE
	}
};

/**
 * ice_init_sw_xlt1_db - init software XLT1 database from HW tables
 * @hw: pointer to the hardware structure
 * @blk: the HW block to initialize
 */
static void ice_init_sw_xlt1_db(struct ice_hw *hw, enum ice_block blk)
{
	u16 pt;

	for (pt = 0; pt < hw->blk[blk].xlt1.count; pt++) {
		u8 ptg;

		ptg = hw->blk[blk].xlt1.t[pt];
		if (ptg != ICE_DEFAULT_PTG) {
			ice_ptg_alloc_val(hw, blk, ptg);
			ice_ptg_add_mv_ptype(hw, blk, pt, ptg);
		}
	}
}

/**
 * ice_init_sw_xlt2_db - init software XLT2 database from HW tables
 * @hw: pointer to the hardware structure
 * @blk: the HW block to initialize
 */
static void ice_init_sw_xlt2_db(struct ice_hw *hw, enum ice_block blk)
{
	u16 vsi;

	for (vsi = 0; vsi < hw->blk[blk].xlt2.count; vsi++) {
		u16 vsig;

		vsig = hw->blk[blk].xlt2.t[vsi];
		if (vsig) {
			ice_vsig_alloc_val(hw, blk, vsig);
			ice_vsig_add_mv_vsi(hw, blk, vsi, vsig);
			/* no changes at this time, since this has been
			 * initialized from the original package
			 */
			hw->blk[blk].xlt2.vsis[vsi].changed = 0;
		}
	}
}

/**
 * ice_init_sw_db - init software database from HW tables
 * @hw: pointer to the hardware structure
 */
static void ice_init_sw_db(struct ice_hw *hw)
{
	u16 i;

	for (i = 0; i < ICE_BLK_COUNT; i++) {
		ice_init_sw_xlt1_db(hw, (enum ice_block)i);
		ice_init_sw_xlt2_db(hw, (enum ice_block)i);
	}
}

/**
 * ice_fill_tbl - Reads content of a single table type into database
 * @hw: pointer to the hardware structure
 * @block_id: Block ID of the table to copy
 * @sid: Section ID of the table to copy
 *
 * Will attempt to read the entire content of a given table of a single block
 * into the driver database. We assume that the buffer will always
 * be as large or larger than the data contained in the package. If
 * this condition is not met, there is most likely an error in the package
 * contents.
 */
static void ice_fill_tbl(struct ice_hw *hw, enum ice_block block_id, u32 sid)
{
	u32 dst_len, sect_len, offset = 0;
	struct ice_prof_redir_section *pr;
	struct ice_prof_id_section *pid;
	struct ice_xlt1_section *xlt1;
	struct ice_xlt2_section *xlt2;
	struct ice_sw_fv_section *es;
	struct ice_pkg_enum state;
	u8 *src, *dst;
	void *sect;

	/* if the HW segment pointer is null then the first iteration of
	 * ice_pkg_enum_section() will fail. In this case the HW tables will
	 * not be filled and return success.
	 */
	if (!hw->seg) {
		ice_debug(hw, ICE_DBG_PKG, "hw->seg is NULL, tables are not filled\n");
		return;
	}

	ice_memset(&state, 0, sizeof(state), ICE_NONDMA_MEM);

	sect = ice_pkg_enum_section(hw->seg, &state, sid);

	while (sect) {
		switch (sid) {
		case ICE_SID_XLT1_SW:
		case ICE_SID_XLT1_FD:
		case ICE_SID_XLT1_RSS:
		case ICE_SID_XLT1_ACL:
		case ICE_SID_XLT1_PE:
			xlt1 = (struct ice_xlt1_section *)sect;
			src = xlt1->value;
			sect_len = LE16_TO_CPU(xlt1->count) *
				sizeof(*hw->blk[block_id].xlt1.t);
			dst = hw->blk[block_id].xlt1.t;
			dst_len = hw->blk[block_id].xlt1.count *
				sizeof(*hw->blk[block_id].xlt1.t);
			break;
		case ICE_SID_XLT2_SW:
		case ICE_SID_XLT2_FD:
		case ICE_SID_XLT2_RSS:
		case ICE_SID_XLT2_ACL:
		case ICE_SID_XLT2_PE:
			xlt2 = (struct ice_xlt2_section *)sect;
			src = (_FORCE_ u8 *)xlt2->value;
			sect_len = LE16_TO_CPU(xlt2->count) *
				sizeof(*hw->blk[block_id].xlt2.t);
			dst = (u8 *)hw->blk[block_id].xlt2.t;
			dst_len = hw->blk[block_id].xlt2.count *
				sizeof(*hw->blk[block_id].xlt2.t);
			break;
		case ICE_SID_PROFID_TCAM_SW:
		case ICE_SID_PROFID_TCAM_FD:
		case ICE_SID_PROFID_TCAM_RSS:
		case ICE_SID_PROFID_TCAM_ACL:
		case ICE_SID_PROFID_TCAM_PE:
			pid = (struct ice_prof_id_section *)sect;
			src = (u8 *)pid->entry;
			sect_len = LE16_TO_CPU(pid->count) *
				sizeof(*hw->blk[block_id].prof.t);
			dst = (u8 *)hw->blk[block_id].prof.t;
			dst_len = hw->blk[block_id].prof.count *
				sizeof(*hw->blk[block_id].prof.t);
			break;
		case ICE_SID_PROFID_REDIR_SW:
		case ICE_SID_PROFID_REDIR_FD:
		case ICE_SID_PROFID_REDIR_RSS:
		case ICE_SID_PROFID_REDIR_ACL:
		case ICE_SID_PROFID_REDIR_PE:
			pr = (struct ice_prof_redir_section *)sect;
			src = pr->redir_value;
			sect_len = LE16_TO_CPU(pr->count) *
				sizeof(*hw->blk[block_id].prof_redir.t);
			dst = hw->blk[block_id].prof_redir.t;
			dst_len = hw->blk[block_id].prof_redir.count *
				sizeof(*hw->blk[block_id].prof_redir.t);
			break;
		case ICE_SID_FLD_VEC_SW:
		case ICE_SID_FLD_VEC_FD:
		case ICE_SID_FLD_VEC_RSS:
		case ICE_SID_FLD_VEC_ACL:
		case ICE_SID_FLD_VEC_PE:
			es = (struct ice_sw_fv_section *)sect;
			src = (u8 *)es->fv;
			sect_len = (u32)(LE16_TO_CPU(es->count) *
					 hw->blk[block_id].es.fvw) *
				sizeof(*hw->blk[block_id].es.t);
			dst = (u8 *)hw->blk[block_id].es.t;
			dst_len = (u32)(hw->blk[block_id].es.count *
					hw->blk[block_id].es.fvw) *
				sizeof(*hw->blk[block_id].es.t);
			break;
		default:
			return;
		}

		/* if the section offset exceeds destination length, terminate
		 * table fill.
		 */
		if (offset > dst_len)
			return;

		/* if the sum of section size and offset exceed destination size
		 * then we are out of bounds of the HW table size for that PF.
		 * Changing section length to fill the remaining table space
		 * of that PF.
		 */
		if ((offset + sect_len) > dst_len)
			sect_len = dst_len - offset;

		ice_memcpy(dst + offset, src, sect_len, ICE_NONDMA_TO_NONDMA);
		offset += sect_len;
		sect = ice_pkg_enum_section(NULL, &state, sid);
	}
}

/**
 * ice_init_flow_profs - init flow profile locks and list heads
 * @hw: pointer to the hardware structure
 * @blk_idx: HW block index
 */
static
void ice_init_flow_profs(struct ice_hw *hw, u8 blk_idx)
{
	ice_init_lock(&hw->fl_profs_locks[blk_idx]);
	INIT_LIST_HEAD(&hw->fl_profs[blk_idx]);
}

/**
 * ice_init_hw_tbls - init hardware table memory
 * @hw: pointer to the hardware structure
 */
enum ice_status ice_init_hw_tbls(struct ice_hw *hw)
{
	u8 i;

	ice_init_lock(&hw->rss_locks);
	INIT_LIST_HEAD(&hw->rss_list_head);
	for (i = 0; i < ICE_BLK_COUNT; i++) {
		struct ice_prof_redir *prof_redir = &hw->blk[i].prof_redir;
		struct ice_prof_tcam *prof = &hw->blk[i].prof;
		struct ice_xlt1 *xlt1 = &hw->blk[i].xlt1;
		struct ice_xlt2 *xlt2 = &hw->blk[i].xlt2;
		struct ice_es *es = &hw->blk[i].es;
		u16 j;

		if (hw->blk[i].is_list_init)
			continue;

		ice_init_flow_profs(hw, i);
		ice_init_lock(&es->prof_map_lock);
		INIT_LIST_HEAD(&es->prof_map);
		hw->blk[i].is_list_init = true;

		hw->blk[i].overwrite = blk_sizes[i].overwrite;
		es->reverse = blk_sizes[i].reverse;

		xlt1->sid = ice_blk_sids[i][ICE_SID_XLT1_OFF];
		xlt1->count = blk_sizes[i].xlt1;

		xlt1->ptypes = (struct ice_ptg_ptype *)
			ice_calloc(hw, xlt1->count, sizeof(*xlt1->ptypes));

		if (!xlt1->ptypes)
			goto err;

		xlt1->ptg_tbl = (struct ice_ptg_entry *)
			ice_calloc(hw, ICE_MAX_PTGS, sizeof(*xlt1->ptg_tbl));

		if (!xlt1->ptg_tbl)
			goto err;

		xlt1->t = (u8 *)ice_calloc(hw, xlt1->count, sizeof(*xlt1->t));
		if (!xlt1->t)
			goto err;

		xlt2->sid = ice_blk_sids[i][ICE_SID_XLT2_OFF];
		xlt2->count = blk_sizes[i].xlt2;

		xlt2->vsis = (struct ice_vsig_vsi *)
			ice_calloc(hw, xlt2->count, sizeof(*xlt2->vsis));

		if (!xlt2->vsis)
			goto err;

		xlt2->vsig_tbl = (struct ice_vsig_entry *)
			ice_calloc(hw, xlt2->count, sizeof(*xlt2->vsig_tbl));
		if (!xlt2->vsig_tbl)
			goto err;

		for (j = 0; j < xlt2->count; j++)
			INIT_LIST_HEAD(&xlt2->vsig_tbl[j].prop_lst);

		xlt2->t = (u16 *)ice_calloc(hw, xlt2->count, sizeof(*xlt2->t));
		if (!xlt2->t)
			goto err;

		prof->sid = ice_blk_sids[i][ICE_SID_PR_OFF];
		prof->count = blk_sizes[i].prof_tcam;
		prof->max_prof_id = blk_sizes[i].prof_id;
		prof->cdid_bits = blk_sizes[i].prof_cdid_bits;
		prof->t = (struct ice_prof_tcam_entry *)
			ice_calloc(hw, prof->count, sizeof(*prof->t));

		if (!prof->t)
			goto err;

		prof_redir->sid = ice_blk_sids[i][ICE_SID_PR_REDIR_OFF];
		prof_redir->count = blk_sizes[i].prof_redir;
		prof_redir->t = (u8 *)ice_calloc(hw, prof_redir->count,
						 sizeof(*prof_redir->t));

		if (!prof_redir->t)
			goto err;

		es->sid = ice_blk_sids[i][ICE_SID_ES_OFF];
		es->count = blk_sizes[i].es;
		es->fvw = blk_sizes[i].fvw;
		es->t = (struct ice_fv_word *)
			ice_calloc(hw, (u32)(es->count * es->fvw),
				   sizeof(*es->t));
		if (!es->t)
			goto err;

		es->ref_count = (u16 *)
			ice_calloc(hw, es->count, sizeof(*es->ref_count));

		if (!es->ref_count)
			goto err;

		es->written = (u8 *)
			ice_calloc(hw, es->count, sizeof(*es->written));

		if (!es->written)
			goto err;

	}
	return ICE_SUCCESS;

err:
	ice_free_hw_tbls(hw);
	return ICE_ERR_NO_MEMORY;
}

/**
 * ice_fill_blk_tbls - Read package context for tables
 * @hw: pointer to the hardware structure
 *
 * Reads the current package contents and populates the driver
 * database with the data iteratively for all advanced feature
 * blocks. Assume that the HW tables have been allocated.
 */
void ice_fill_blk_tbls(struct ice_hw *hw)
{
	u8 i;

	for (i = 0; i < ICE_BLK_COUNT; i++) {
		enum ice_block blk_id = (enum ice_block)i;

		ice_fill_tbl(hw, blk_id, hw->blk[blk_id].xlt1.sid);
		ice_fill_tbl(hw, blk_id, hw->blk[blk_id].xlt2.sid);
		ice_fill_tbl(hw, blk_id, hw->blk[blk_id].prof.sid);
		ice_fill_tbl(hw, blk_id, hw->blk[blk_id].prof_redir.sid);
		ice_fill_tbl(hw, blk_id, hw->blk[blk_id].es.sid);
	}

	ice_init_sw_db(hw);
}

/**
 * ice_free_prof_map - free profile map
 * @hw: pointer to the hardware structure
 * @blk_idx: HW block index
 */
static void ice_free_prof_map(struct ice_hw *hw, u8 blk_idx)
{
	struct ice_es *es = &hw->blk[blk_idx].es;
	struct ice_prof_map *del, *tmp;

	ice_acquire_lock(&es->prof_map_lock);
	LIST_FOR_EACH_ENTRY_SAFE(del, tmp, &es->prof_map,
				 ice_prof_map, list) {
		LIST_DEL(&del->list);
		ice_free(hw, del);
	}
	INIT_LIST_HEAD(&es->prof_map);
	ice_release_lock(&es->prof_map_lock);
}

/**
 * ice_free_flow_profs - free flow profile entries
 * @hw: pointer to the hardware structure
 * @blk_idx: HW block index
 */
static void ice_free_flow_profs(struct ice_hw *hw, u8 blk_idx)
{
	struct ice_flow_prof *p, *tmp;

	ice_acquire_lock(&hw->fl_profs_locks[blk_idx]);
	LIST_FOR_EACH_ENTRY_SAFE(p, tmp, &hw->fl_profs[blk_idx],
				 ice_flow_prof, l_entry) {
		LIST_DEL(&p->l_entry);

		ice_free(hw, p);
	}
	ice_release_lock(&hw->fl_profs_locks[blk_idx]);

	/* if driver is in reset and tables are being cleared
	 * re-initialize the flow profile list heads
	 */
	INIT_LIST_HEAD(&hw->fl_profs[blk_idx]);
}

/**
 * ice_free_vsig_tbl - free complete VSIG table entries
 * @hw: pointer to the hardware structure
 * @blk: the HW block on which to free the VSIG table entries
 */
static void ice_free_vsig_tbl(struct ice_hw *hw, enum ice_block blk)
{
	u16 i;

	if (!hw->blk[blk].xlt2.vsig_tbl)
		return;

	for (i = 1; i < ICE_MAX_VSIGS; i++)
		if (hw->blk[blk].xlt2.vsig_tbl[i].in_use)
			ice_vsig_free(hw, blk, i);
}

/**
 * ice_free_hw_tbls - free hardware table memory
 * @hw: pointer to the hardware structure
 */
void ice_free_hw_tbls(struct ice_hw *hw)
{
	struct ice_rss_cfg *r, *rt;
	u8 i;

	for (i = 0; i < ICE_BLK_COUNT; i++) {
		if (hw->blk[i].is_list_init) {
			struct ice_es *es = &hw->blk[i].es;

			ice_free_prof_map(hw, i);
			ice_destroy_lock(&es->prof_map_lock);

			ice_free_flow_profs(hw, i);
			ice_destroy_lock(&hw->fl_profs_locks[i]);

			hw->blk[i].is_list_init = false;
		}
		ice_free_vsig_tbl(hw, (enum ice_block)i);
		ice_free(hw, hw->blk[i].xlt1.ptypes);
		ice_free(hw, hw->blk[i].xlt1.ptg_tbl);
		ice_free(hw, hw->blk[i].xlt1.t);
		ice_free(hw, hw->blk[i].xlt2.t);
		ice_free(hw, hw->blk[i].xlt2.vsig_tbl);
		ice_free(hw, hw->blk[i].xlt2.vsis);
		ice_free(hw, hw->blk[i].prof.t);
		ice_free(hw, hw->blk[i].prof_redir.t);
		ice_free(hw, hw->blk[i].es.t);
		ice_free(hw, hw->blk[i].es.ref_count);
		ice_free(hw, hw->blk[i].es.written);
	}

	LIST_FOR_EACH_ENTRY_SAFE(r, rt, &hw->rss_list_head,
				 ice_rss_cfg, l_entry) {
		LIST_DEL(&r->l_entry);
		ice_free(hw, r);
	}
	ice_destroy_lock(&hw->rss_locks);
	ice_memset(hw->blk, 0, sizeof(hw->blk), ICE_NONDMA_MEM);
}

/**
 * ice_clear_hw_tbls - clear HW tables and flow profiles
 * @hw: pointer to the hardware structure
 */
void ice_clear_hw_tbls(struct ice_hw *hw)
{
	u8 i;

	for (i = 0; i < ICE_BLK_COUNT; i++) {
		struct ice_prof_redir *prof_redir = &hw->blk[i].prof_redir;
		struct ice_prof_tcam *prof = &hw->blk[i].prof;
		struct ice_xlt1 *xlt1 = &hw->blk[i].xlt1;
		struct ice_xlt2 *xlt2 = &hw->blk[i].xlt2;
		struct ice_es *es = &hw->blk[i].es;

		if (hw->blk[i].is_list_init) {
			ice_free_prof_map(hw, i);
			ice_free_flow_profs(hw, i);
		}

		ice_free_vsig_tbl(hw, (enum ice_block)i);

		if (xlt1->ptypes)
			ice_memset(xlt1->ptypes, 0,
				   xlt1->count * sizeof(*xlt1->ptypes),
				   ICE_NONDMA_MEM);

		if (xlt1->ptg_tbl)
			ice_memset(xlt1->ptg_tbl, 0,
				   ICE_MAX_PTGS * sizeof(*xlt1->ptg_tbl),
				   ICE_NONDMA_MEM);

		if (xlt1->t)
			ice_memset(xlt1->t, 0, xlt1->count * sizeof(*xlt1->t),
				   ICE_NONDMA_MEM);

		if (xlt2->vsis)
			ice_memset(xlt2->vsis, 0,
				   xlt2->count * sizeof(*xlt2->vsis),
				   ICE_NONDMA_MEM);

		if (xlt2->vsig_tbl)
			ice_memset(xlt2->vsig_tbl, 0,
				   xlt2->count * sizeof(*xlt2->vsig_tbl),
				   ICE_NONDMA_MEM);

		if (xlt2->t)
			ice_memset(xlt2->t, 0, xlt2->count * sizeof(*xlt2->t),
				   ICE_NONDMA_MEM);

		if (prof->t)
			ice_memset(prof->t, 0, prof->count * sizeof(*prof->t),
				   ICE_NONDMA_MEM);

		if (prof_redir->t)
			ice_memset(prof_redir->t, 0,
				   prof_redir->count * sizeof(*prof_redir->t),
				   ICE_NONDMA_MEM);

		if (es->t)
			ice_memset(es->t, 0,
				   es->count * sizeof(*es->t) * es->fvw,
				   ICE_NONDMA_MEM);

		if (es->ref_count)
			ice_memset(es->ref_count, 0,
				   es->count * sizeof(*es->ref_count),
				   ICE_NONDMA_MEM);

		if (es->written)
			ice_memset(es->written, 0,
				   es->count * sizeof(*es->written),
				   ICE_NONDMA_MEM);

	}
}

/**
 * ice_prof_gen_key - generate profile ID key
 * @hw: pointer to the HW struct
 * @blk: the block in which to write profile ID to
 * @ptg: packet type group (PTG) portion of key
 * @vsig: VSIG portion of key
 * @cdid: CDID portion of key
 * @flags: flag portion of key
 * @vl_msk: valid mask
 * @dc_msk: don't care mask
 * @nm_msk: never match mask
 * @key: output of profile ID key
 */
static enum ice_status
ice_prof_gen_key(struct ice_hw *hw, enum ice_block blk, u8 ptg, u16 vsig,
		 u8 cdid, u16 flags, u8 vl_msk[ICE_TCAM_KEY_VAL_SZ],
		 u8 dc_msk[ICE_TCAM_KEY_VAL_SZ], u8 nm_msk[ICE_TCAM_KEY_VAL_SZ],
		 u8 key[ICE_TCAM_KEY_SZ])
{
	struct ice_prof_id_key inkey;

	inkey.xlt1 = ptg;
	inkey.xlt2_cdid = CPU_TO_LE16(vsig);
	inkey.flags = CPU_TO_LE16(flags);

	switch (hw->blk[blk].prof.cdid_bits) {
	case 0:
		break;
	case 2:
#define ICE_CD_2_M 0xC000U
#define ICE_CD_2_S 14
		inkey.xlt2_cdid &= ~CPU_TO_LE16(ICE_CD_2_M);
		inkey.xlt2_cdid |= CPU_TO_LE16(BIT(cdid) << ICE_CD_2_S);
		break;
	case 4:
#define ICE_CD_4_M 0xF000U
#define ICE_CD_4_S 12
		inkey.xlt2_cdid &= ~CPU_TO_LE16(ICE_CD_4_M);
		inkey.xlt2_cdid |= CPU_TO_LE16(BIT(cdid) << ICE_CD_4_S);
		break;
	case 8:
#define ICE_CD_8_M 0xFF00U
#define ICE_CD_8_S 16
		inkey.xlt2_cdid &= ~CPU_TO_LE16(ICE_CD_8_M);
		inkey.xlt2_cdid |= CPU_TO_LE16(BIT(cdid) << ICE_CD_8_S);
		break;
	default:
		ice_debug(hw, ICE_DBG_PKG, "Error in profile config\n");
		break;
	}

	return ice_set_key(key, ICE_TCAM_KEY_SZ, (u8 *)&inkey, vl_msk, dc_msk,
			   nm_msk, 0, ICE_TCAM_KEY_SZ / 2);
}

/**
 * ice_tcam_write_entry - write TCAM entry
 * @hw: pointer to the HW struct
 * @blk: the block in which to write profile ID to
 * @idx: the entry index to write to
 * @prof_id: profile ID
 * @ptg: packet type group (PTG) portion of key
 * @vsig: VSIG portion of key
 * @cdid: CDID portion of key
 * @flags: flag portion of key
 * @vl_msk: valid mask
 * @dc_msk: don't care mask
 * @nm_msk: never match mask
 */
static enum ice_status
ice_tcam_write_entry(struct ice_hw *hw, enum ice_block blk, u16 idx,
		     u8 prof_id, u8 ptg, u16 vsig, u8 cdid, u16 flags,
		     u8 vl_msk[ICE_TCAM_KEY_VAL_SZ],
		     u8 dc_msk[ICE_TCAM_KEY_VAL_SZ],
		     u8 nm_msk[ICE_TCAM_KEY_VAL_SZ])
{
	struct ice_prof_tcam_entry;
	enum ice_status status;

	status = ice_prof_gen_key(hw, blk, ptg, vsig, cdid, flags, vl_msk,
				  dc_msk, nm_msk, hw->blk[blk].prof.t[idx].key);
	if (!status) {
		hw->blk[blk].prof.t[idx].addr = CPU_TO_LE16(idx);
		hw->blk[blk].prof.t[idx].prof_id = prof_id;
	}

	return status;
}

/**
 * ice_vsig_get_ref - returns number of VSIs belong to a VSIG
 * @hw: pointer to the hardware structure
 * @blk: HW block
 * @vsig: VSIG to query
 * @refs: pointer to variable to receive the reference count
 */
static enum ice_status
ice_vsig_get_ref(struct ice_hw *hw, enum ice_block blk, u16 vsig, u16 *refs)
{
	u16 idx = vsig & ICE_VSIG_IDX_M;
	struct ice_vsig_vsi *ptr;

	*refs = 0;

	if (!hw->blk[blk].xlt2.vsig_tbl[idx].in_use)
		return ICE_ERR_DOES_NOT_EXIST;

	ptr = hw->blk[blk].xlt2.vsig_tbl[idx].first_vsi;
	while (ptr) {
		(*refs)++;
		ptr = ptr->next_vsi;
	}

	return ICE_SUCCESS;
}

/**
 * ice_has_prof_vsig - check to see if VSIG has a specific profile
 * @hw: pointer to the hardware structure
 * @blk: HW block
 * @vsig: VSIG to check against
 * @hdl: profile handle
 */
static bool
ice_has_prof_vsig(struct ice_hw *hw, enum ice_block blk, u16 vsig, u64 hdl)
{
	u16 idx = vsig & ICE_VSIG_IDX_M;
	struct ice_vsig_prof *ent;

	LIST_FOR_EACH_ENTRY(ent, &hw->blk[blk].xlt2.vsig_tbl[idx].prop_lst,
			    ice_vsig_prof, list)
		if (ent->profile_cookie == hdl)
			return true;

	ice_debug(hw, ICE_DBG_INIT, "Characteristic list for VSI group %d not found.\n",
		  vsig);
	return false;
}

/**
 * ice_prof_bld_es - build profile ID extraction sequence changes
 * @hw: pointer to the HW struct
 * @blk: hardware block
 * @bld: the update package buffer build to add to
 * @chgs: the list of changes to make in hardware
 */
static enum ice_status
ice_prof_bld_es(struct ice_hw *hw, enum ice_block blk,
		struct ice_buf_build *bld, struct LIST_HEAD_TYPE *chgs)
{
	u16 vec_size = hw->blk[blk].es.fvw * sizeof(struct ice_fv_word);
	struct ice_chs_chg *tmp;

	LIST_FOR_EACH_ENTRY(tmp, chgs, ice_chs_chg, list_entry)
		if (tmp->type == ICE_PTG_ES_ADD && tmp->add_prof) {
			u16 off = tmp->prof_id * hw->blk[blk].es.fvw;
			struct ice_pkg_es *p;
			u32 id;

			id = ice_sect_id(blk, ICE_VEC_TBL);
			p = (struct ice_pkg_es *)
				ice_pkg_buf_alloc_section(bld, id,
							  ice_struct_size(p, es,
									  1) +
							  vec_size -
							  sizeof(p->es[0]));

			if (!p)
				return ICE_ERR_MAX_LIMIT;

			p->count = CPU_TO_LE16(1);
			p->offset = CPU_TO_LE16(tmp->prof_id);

			ice_memcpy(p->es, &hw->blk[blk].es.t[off], vec_size,
				   ICE_NONDMA_TO_NONDMA);
		}

	return ICE_SUCCESS;
}

/**
 * ice_prof_bld_tcam - build profile ID TCAM changes
 * @hw: pointer to the HW struct
 * @blk: hardware block
 * @bld: the update package buffer build to add to
 * @chgs: the list of changes to make in hardware
 */
static enum ice_status
ice_prof_bld_tcam(struct ice_hw *hw, enum ice_block blk,
		  struct ice_buf_build *bld, struct LIST_HEAD_TYPE *chgs)
{
	struct ice_chs_chg *tmp;

	LIST_FOR_EACH_ENTRY(tmp, chgs, ice_chs_chg, list_entry)
		if (tmp->type == ICE_TCAM_ADD && tmp->add_tcam_idx) {
			struct ice_prof_id_section *p;
			u32 id;

			id = ice_sect_id(blk, ICE_PROF_TCAM);
			p = (struct ice_prof_id_section *)
				ice_pkg_buf_alloc_section(bld, id,
							  ice_struct_size(p,
									  entry,
									  1));

			if (!p)
				return ICE_ERR_MAX_LIMIT;

			p->count = CPU_TO_LE16(1);
			p->entry[0].addr = CPU_TO_LE16(tmp->tcam_idx);
			p->entry[0].prof_id = tmp->prof_id;

			ice_memcpy(p->entry[0].key,
				   &hw->blk[blk].prof.t[tmp->tcam_idx].key,
				   sizeof(hw->blk[blk].prof.t->key),
				   ICE_NONDMA_TO_NONDMA);
		}

	return ICE_SUCCESS;
}

/**
 * ice_prof_bld_xlt1 - build XLT1 changes
 * @blk: hardware block
 * @bld: the update package buffer build to add to
 * @chgs: the list of changes to make in hardware
 */
static enum ice_status
ice_prof_bld_xlt1(enum ice_block blk, struct ice_buf_build *bld,
		  struct LIST_HEAD_TYPE *chgs)
{
	struct ice_chs_chg *tmp;

	LIST_FOR_EACH_ENTRY(tmp, chgs, ice_chs_chg, list_entry)
		if (tmp->type == ICE_PTG_ES_ADD && tmp->add_ptg) {
			struct ice_xlt1_section *p;
			u32 id;

			id = ice_sect_id(blk, ICE_XLT1);
			p = (struct ice_xlt1_section *)
				ice_pkg_buf_alloc_section(bld, id,
							  ice_struct_size(p,
									  value,
									  1));

			if (!p)
				return ICE_ERR_MAX_LIMIT;

			p->count = CPU_TO_LE16(1);
			p->offset = CPU_TO_LE16(tmp->ptype);
			p->value[0] = tmp->ptg;
		}

	return ICE_SUCCESS;
}

/**
 * ice_prof_bld_xlt2 - build XLT2 changes
 * @blk: hardware block
 * @bld: the update package buffer build to add to
 * @chgs: the list of changes to make in hardware
 */
static enum ice_status
ice_prof_bld_xlt2(enum ice_block blk, struct ice_buf_build *bld,
		  struct LIST_HEAD_TYPE *chgs)
{
	struct ice_chs_chg *tmp;

	LIST_FOR_EACH_ENTRY(tmp, chgs, ice_chs_chg, list_entry) {
		struct ice_xlt2_section *p;
		u32 id;

		switch (tmp->type) {
		case ICE_VSIG_ADD:
		case ICE_VSI_MOVE:
		case ICE_VSIG_REM:
			id = ice_sect_id(blk, ICE_XLT2);
			p = (struct ice_xlt2_section *)
				ice_pkg_buf_alloc_section(bld, id,
							  ice_struct_size(p,
									  value,
									  1));

			if (!p)
				return ICE_ERR_MAX_LIMIT;

			p->count = CPU_TO_LE16(1);
			p->offset = CPU_TO_LE16(tmp->vsi);
			p->value[0] = CPU_TO_LE16(tmp->vsig);
			break;
		default:
			break;
		}
	}

	return ICE_SUCCESS;
}

/**
 * ice_upd_prof_hw - update hardware using the change list
 * @hw: pointer to the HW struct
 * @blk: hardware block
 * @chgs: the list of changes to make in hardware
 */
static enum ice_status
ice_upd_prof_hw(struct ice_hw *hw, enum ice_block blk,
		struct LIST_HEAD_TYPE *chgs)
{
	struct ice_buf_build *b;
	struct ice_chs_chg *tmp;
	enum ice_status status;
	u16 pkg_sects;
	u16 xlt1 = 0;
	u16 xlt2 = 0;
	u16 tcam = 0;
	u16 es = 0;
	u16 sects;

	/* count number of sections we need */
	LIST_FOR_EACH_ENTRY(tmp, chgs, ice_chs_chg, list_entry) {
		switch (tmp->type) {
		case ICE_PTG_ES_ADD:
			if (tmp->add_ptg)
				xlt1++;
			if (tmp->add_prof)
				es++;
			break;
		case ICE_TCAM_ADD:
			tcam++;
			break;
		case ICE_VSIG_ADD:
		case ICE_VSI_MOVE:
		case ICE_VSIG_REM:
			xlt2++;
			break;
		default:
			break;
		}
	}
	sects = xlt1 + xlt2 + tcam + es;

	if (!sects)
		return ICE_SUCCESS;

	/* Build update package buffer */
	b = ice_pkg_buf_alloc(hw);
	if (!b)
		return ICE_ERR_NO_MEMORY;

	status = ice_pkg_buf_reserve_section(b, sects);
	if (status)
		goto error_tmp;

	/* Preserve order of table update: ES, TCAM, PTG, VSIG */
	if (es) {
		status = ice_prof_bld_es(hw, blk, b, chgs);
		if (status)
			goto error_tmp;
	}

	if (tcam) {
		status = ice_prof_bld_tcam(hw, blk, b, chgs);
		if (status)
			goto error_tmp;
	}

	if (xlt1) {
		status = ice_prof_bld_xlt1(blk, b, chgs);
		if (status)
			goto error_tmp;
	}

	if (xlt2) {
		status = ice_prof_bld_xlt2(blk, b, chgs);
		if (status)
			goto error_tmp;
	}

	/* After package buffer build check if the section count in buffer is
	 * non-zero and matches the number of sections detected for package
	 * update.
	 */
	pkg_sects = ice_pkg_buf_get_active_sections(b);
	if (!pkg_sects || pkg_sects != sects) {
		status = ICE_ERR_INVAL_SIZE;
		goto error_tmp;
	}

	/* update package */
	status = ice_update_pkg(hw, ice_pkg_buf(b), 1);
	if (status == ICE_ERR_AQ_ERROR)
		ice_debug(hw, ICE_DBG_INIT, "Unable to update HW profile\n");

error_tmp:
	ice_pkg_buf_free(hw, b);
	return status;
}

/**
 * ice_add_prof - add profile
 * @hw: pointer to the HW struct
 * @blk: hardware block
 * @id: profile tracking ID
 * @ptypes: bitmap indicating ptypes (ICE_FLOW_PTYPE_MAX bits)
 * @es: extraction sequence (length of array is determined by the block)
 *
 * This function registers a profile, which matches a set of PTGs with a
 * particular extraction sequence. While the hardware profile is allocated
 * it will not be written until the first call to ice_add_flow that specifies
 * the ID value used here.
 */
enum ice_status
ice_add_prof(struct ice_hw *hw, enum ice_block blk, u64 id,
	     ice_bitmap_t *ptypes, struct ice_fv_word *es)
{
	ice_declare_bitmap(ptgs_used, ICE_XLT1_CNT);
	struct ice_prof_map *prof;
	enum ice_status status;
	u8 prof_id;
	u16 ptype;

	ice_zero_bitmap(ptgs_used, ICE_XLT1_CNT);

	ice_acquire_lock(&hw->blk[blk].es.prof_map_lock);

	/* search for existing profile */
	status = ice_find_prof_id(hw, blk, es, &prof_id);
	if (status) {
		/* allocate profile ID */
		status = ice_alloc_prof_id(hw, blk, &prof_id);
		if (status)
			goto err_ice_add_prof;

		/* and write new es */
		ice_write_es(hw, blk, prof_id, es);
	}

	ice_prof_inc_ref(hw, blk, prof_id);

	/* add profile info */

	prof = (struct ice_prof_map *)ice_malloc(hw, sizeof(*prof));
	if (!prof)
		goto err_ice_add_prof;

	prof->profile_cookie = id;
	prof->prof_id = prof_id;
	prof->ptg_cnt = 0;
	prof->context = 0;

	/* build list of ptgs */
	ice_for_each_set_bit(ptype, ptypes, ICE_FLOW_PTYPE_MAX) {
		u8 ptg;

		/* The package should place all ptypes in a non-zero
		 * PTG, so the following call should never fail.
		 */
		if (ice_ptg_find_ptype(hw, blk, ptype, &ptg))
			continue;

		/* If PTG is already added, skip and continue */
		if (ice_is_bit_set(ptgs_used, ptg))
			continue;

		ice_set_bit(ptg, ptgs_used);
		prof->ptg[prof->ptg_cnt] = ptg;

		if (++prof->ptg_cnt >= ICE_MAX_PTG_PER_PROFILE)
			break;
	}

	LIST_ADD(&prof->list, &hw->blk[blk].es.prof_map);
	status = ICE_SUCCESS;

err_ice_add_prof:
	ice_release_lock(&hw->blk[blk].es.prof_map_lock);
	return status;
}

/**
 * ice_search_prof_id - Search for a profile tracking ID
 * @hw: pointer to the HW struct
 * @blk: hardware block
 * @id: profile tracking ID
 *
 * This will search for a profile tracking ID which was previously added.
 * The profile map lock should be held before calling this function.
 */
struct ice_prof_map *
ice_search_prof_id(struct ice_hw *hw, enum ice_block blk, u64 id)
{
	struct ice_prof_map *entry = NULL;
	struct ice_prof_map *map;

	LIST_FOR_EACH_ENTRY(map, &hw->blk[blk].es.prof_map, ice_prof_map, list)
		if (map->profile_cookie == id) {
			entry = map;
			break;
		}

	return entry;
}

/**
 * ice_set_prof_context - Set context for a given profile
 * @hw: pointer to the HW struct
 * @blk: hardware block
 * @id: profile tracking ID
 * @cntxt: context
 */
enum ice_status
ice_set_prof_context(struct ice_hw *hw, enum ice_block blk, u64 id, u64 cntxt)
{
	enum ice_status status = ICE_ERR_DOES_NOT_EXIST;
	struct ice_prof_map *entry;

	ice_acquire_lock(&hw->blk[blk].es.prof_map_lock);
	entry = ice_search_prof_id(hw, blk, id);
	if (entry) {
		entry->context = cntxt;
		status = ICE_SUCCESS;
	}
	ice_release_lock(&hw->blk[blk].es.prof_map_lock);
	return status;
}

/**
 * ice_get_prof_context - Get context for a given profile
 * @hw: pointer to the HW struct
 * @blk: hardware block
 * @id: profile tracking ID
 * @cntxt: pointer to variable to receive the context
 */
enum ice_status
ice_get_prof_context(struct ice_hw *hw, enum ice_block blk, u64 id, u64 *cntxt)
{
	enum ice_status status = ICE_ERR_DOES_NOT_EXIST;
	struct ice_prof_map *entry;

	ice_acquire_lock(&hw->blk[blk].es.prof_map_lock);
	entry = ice_search_prof_id(hw, blk, id);
	if (entry) {
		*cntxt = entry->context;
		status = ICE_SUCCESS;
	}
	ice_release_lock(&hw->blk[blk].es.prof_map_lock);
	return status;
}

/**
 * ice_vsig_prof_id_count - count profiles in a VSIG
 * @hw: pointer to the HW struct
 * @blk: hardware block
 * @vsig: VSIG to remove the profile from
 */
static u16
ice_vsig_prof_id_count(struct ice_hw *hw, enum ice_block blk, u16 vsig)
{
	u16 idx = vsig & ICE_VSIG_IDX_M, count = 0;
	struct ice_vsig_prof *p;

	LIST_FOR_EACH_ENTRY(p, &hw->blk[blk].xlt2.vsig_tbl[idx].prop_lst,
			    ice_vsig_prof, list)
		count++;

	return count;
}

/**
 * ice_rel_tcam_idx - release a TCAM index
 * @hw: pointer to the HW struct
 * @blk: hardware block
 * @idx: the index to release
 */
static enum ice_status
ice_rel_tcam_idx(struct ice_hw *hw, enum ice_block blk, u16 idx)
{
	/* Masks to invoke a never match entry */
	u8 vl_msk[ICE_TCAM_KEY_VAL_SZ] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
	u8 dc_msk[ICE_TCAM_KEY_VAL_SZ] = { 0xFE, 0xFF, 0xFF, 0xFF, 0xFF };
	u8 nm_msk[ICE_TCAM_KEY_VAL_SZ] = { 0x01, 0x00, 0x00, 0x00, 0x00 };
	enum ice_status status;

	/* write the TCAM entry */
	status = ice_tcam_write_entry(hw, blk, idx, 0, 0, 0, 0, 0, vl_msk,
				      dc_msk, nm_msk);
	if (status)
		return status;

	/* release the TCAM entry */
	status = ice_free_tcam_ent(hw, blk, idx);

	return status;
}

/**
 * ice_rem_prof_id - remove one profile from a VSIG
 * @hw: pointer to the HW struct
 * @blk: hardware block
 * @prof: pointer to profile structure to remove
 */
static enum ice_status
ice_rem_prof_id(struct ice_hw *hw, enum ice_block blk,
		struct ice_vsig_prof *prof)
{
	enum ice_status status;
	u16 i;

	for (i = 0; i < prof->tcam_count; i++)
		if (prof->tcam[i].in_use) {
			prof->tcam[i].in_use = false;
			status = ice_rel_tcam_idx(hw, blk,
						  prof->tcam[i].tcam_idx);
			if (status)
				return ICE_ERR_HW_TABLE;
		}

	return ICE_SUCCESS;
}

/**
 * ice_rem_vsig - remove VSIG
 * @hw: pointer to the HW struct
 * @blk: hardware block
 * @vsig: the VSIG to remove
 * @chg: the change list
 */
static enum ice_status
ice_rem_vsig(struct ice_hw *hw, enum ice_block blk, u16 vsig,
	     struct LIST_HEAD_TYPE *chg)
{
	u16 idx = vsig & ICE_VSIG_IDX_M;
	struct ice_vsig_vsi *vsi_cur;
	struct ice_vsig_prof *d, *t;

	/* remove TCAM entries */
	LIST_FOR_EACH_ENTRY_SAFE(d, t,
				 &hw->blk[blk].xlt2.vsig_tbl[idx].prop_lst,
				 ice_vsig_prof, list) {
		enum ice_status status;

		status = ice_rem_prof_id(hw, blk, d);
		if (status)
			return status;

		LIST_DEL(&d->list);
		ice_free(hw, d);
	}

	/* Move all VSIS associated with this VSIG to the default VSIG */
	vsi_cur = hw->blk[blk].xlt2.vsig_tbl[idx].first_vsi;
	/* If the VSIG has at least 1 VSI then iterate through the list
	 * and remove the VSIs before deleting the group.
	 */
	if (vsi_cur)
		do {
			struct ice_vsig_vsi *tmp = vsi_cur->next_vsi;
			struct ice_chs_chg *p;

			p = (struct ice_chs_chg *)ice_malloc(hw, sizeof(*p));
			if (!p)
				return ICE_ERR_NO_MEMORY;

			p->type = ICE_VSIG_REM;
			p->orig_vsig = vsig;
			p->vsig = ICE_DEFAULT_VSIG;
			p->vsi = (u16)(vsi_cur - hw->blk[blk].xlt2.vsis);

			LIST_ADD(&p->list_entry, chg);

			vsi_cur = tmp;
		} while (vsi_cur);

	return ice_vsig_free(hw, blk, vsig);
}

/**
 * ice_rem_prof_id_vsig - remove a specific profile from a VSIG
 * @hw: pointer to the HW struct
 * @blk: hardware block
 * @vsig: VSIG to remove the profile from
 * @hdl: profile handle indicating which profile to remove
 * @chg: list to receive a record of changes
 */
static enum ice_status
ice_rem_prof_id_vsig(struct ice_hw *hw, enum ice_block blk, u16 vsig, u64 hdl,
		     struct LIST_HEAD_TYPE *chg)
{
	u16 idx = vsig & ICE_VSIG_IDX_M;
	struct ice_vsig_prof *p, *t;

	LIST_FOR_EACH_ENTRY_SAFE(p, t,
				 &hw->blk[blk].xlt2.vsig_tbl[idx].prop_lst,
				 ice_vsig_prof, list)
		if (p->profile_cookie == hdl) {
			enum ice_status status;

			if (ice_vsig_prof_id_count(hw, blk, vsig) == 1)
				/* this is the last profile, remove the VSIG */
				return ice_rem_vsig(hw, blk, vsig, chg);

			status = ice_rem_prof_id(hw, blk, p);
			if (!status) {
				LIST_DEL(&p->list);
				ice_free(hw, p);
			}
			return status;
		}

	return ICE_ERR_DOES_NOT_EXIST;
}

/**
 * ice_rem_flow_all - remove all flows with a particular profile
 * @hw: pointer to the HW struct
 * @blk: hardware block
 * @id: profile tracking ID
 */
static enum ice_status
ice_rem_flow_all(struct ice_hw *hw, enum ice_block blk, u64 id)
{
	struct ice_chs_chg *del, *tmp;
	enum ice_status status;
	struct LIST_HEAD_TYPE chg;
	u16 i;

	INIT_LIST_HEAD(&chg);

	for (i = 1; i < ICE_MAX_VSIGS; i++)
		if (hw->blk[blk].xlt2.vsig_tbl[i].in_use) {
			if (ice_has_prof_vsig(hw, blk, i, id)) {
				status = ice_rem_prof_id_vsig(hw, blk, i, id,
							      &chg);
				if (status)
					goto err_ice_rem_flow_all;
			}
		}

	status = ice_upd_prof_hw(hw, blk, &chg);

err_ice_rem_flow_all:
	LIST_FOR_EACH_ENTRY_SAFE(del, tmp, &chg, ice_chs_chg, list_entry) {
		LIST_DEL(&del->list_entry);
		ice_free(hw, del);
	}

	return status;
}

/**
 * ice_rem_prof - remove profile
 * @hw: pointer to the HW struct
 * @blk: hardware block
 * @id: profile tracking ID
 *
 * This will remove the profile specified by the ID parameter, which was
 * previously created through ice_add_prof. If any existing entries
 * are associated with this profile, they will be removed as well.
 */
enum ice_status ice_rem_prof(struct ice_hw *hw, enum ice_block blk, u64 id)
{
	struct ice_prof_map *pmap;
	enum ice_status status;

	ice_acquire_lock(&hw->blk[blk].es.prof_map_lock);

	pmap = ice_search_prof_id(hw, blk, id);
	if (!pmap) {
		status = ICE_ERR_DOES_NOT_EXIST;
		goto err_ice_rem_prof;
	}

	/* remove all flows with this profile */
	status = ice_rem_flow_all(hw, blk, pmap->profile_cookie);
	if (status)
		goto err_ice_rem_prof;

	/* dereference profile, and possibly remove */
	ice_prof_dec_ref(hw, blk, pmap->prof_id);

	LIST_DEL(&pmap->list);
	ice_free(hw, pmap);

err_ice_rem_prof:
	ice_release_lock(&hw->blk[blk].es.prof_map_lock);
	return status;
}

/**
 * ice_get_prof - get profile
 * @hw: pointer to the HW struct
 * @blk: hardware block
 * @hdl: profile handle
 * @chg: change list
 */
static enum ice_status
ice_get_prof(struct ice_hw *hw, enum ice_block blk, u64 hdl,
	     struct LIST_HEAD_TYPE *chg)
{
	enum ice_status status = ICE_SUCCESS;
	struct ice_prof_map *map;
	struct ice_chs_chg *p;
	u16 i;

	ice_acquire_lock(&hw->blk[blk].es.prof_map_lock);
	/* Get the details on the profile specified by the handle ID */
	map = ice_search_prof_id(hw, blk, hdl);
	if (!map) {
		status = ICE_ERR_DOES_NOT_EXIST;
		goto err_ice_get_prof;
	}

	for (i = 0; i < map->ptg_cnt; i++)
		if (!hw->blk[blk].es.written[map->prof_id]) {
			/* add ES to change list */
			p = (struct ice_chs_chg *)ice_malloc(hw, sizeof(*p));
			if (!p) {
				status = ICE_ERR_NO_MEMORY;
				goto err_ice_get_prof;
			}

			p->type = ICE_PTG_ES_ADD;
			p->ptype = 0;
			p->ptg = map->ptg[i];
			p->add_ptg = 0;

			p->add_prof = 1;
			p->prof_id = map->prof_id;

			hw->blk[blk].es.written[map->prof_id] = true;

			LIST_ADD(&p->list_entry, chg);
		}

err_ice_get_prof:
	ice_release_lock(&hw->blk[blk].es.prof_map_lock);
	/* let caller clean up the change list */
	return status;
}

/**
 * ice_get_profs_vsig - get a copy of the list of profiles from a VSIG
 * @hw: pointer to the HW struct
 * @blk: hardware block
 * @vsig: VSIG from which to copy the list
 * @lst: output list
 *
 * This routine makes a copy of the list of profiles in the specified VSIG.
 */
static enum ice_status
ice_get_profs_vsig(struct ice_hw *hw, enum ice_block blk, u16 vsig,
		   struct LIST_HEAD_TYPE *lst)
{
	struct ice_vsig_prof *ent1, *ent2;
	u16 idx = vsig & ICE_VSIG_IDX_M;

	LIST_FOR_EACH_ENTRY(ent1, &hw->blk[blk].xlt2.vsig_tbl[idx].prop_lst,
			    ice_vsig_prof, list) {
		struct ice_vsig_prof *p;

		/* copy to the input list */
		p = (struct ice_vsig_prof *)ice_memdup(hw, ent1, sizeof(*p),
						       ICE_NONDMA_TO_NONDMA);
		if (!p)
			goto err_ice_get_profs_vsig;

		LIST_ADD_TAIL(&p->list, lst);
	}

	return ICE_SUCCESS;

err_ice_get_profs_vsig:
	LIST_FOR_EACH_ENTRY_SAFE(ent1, ent2, lst, ice_vsig_prof, list) {
		LIST_DEL(&ent1->list);
		ice_free(hw, ent1);
	}

	return ICE_ERR_NO_MEMORY;
}

/**
 * ice_add_prof_to_lst - add profile entry to a list
 * @hw: pointer to the HW struct
 * @blk: hardware block
 * @lst: the list to be added to
 * @hdl: profile handle of entry to add
 */
static enum ice_status
ice_add_prof_to_lst(struct ice_hw *hw, enum ice_block blk,
		    struct LIST_HEAD_TYPE *lst, u64 hdl)
{
	enum ice_status status = ICE_SUCCESS;
	struct ice_prof_map *map;
	struct ice_vsig_prof *p;
	u16 i;

	ice_acquire_lock(&hw->blk[blk].es.prof_map_lock);
	map = ice_search_prof_id(hw, blk, hdl);
	if (!map) {
		status = ICE_ERR_DOES_NOT_EXIST;
		goto err_ice_add_prof_to_lst;
	}

	p = (struct ice_vsig_prof *)ice_malloc(hw, sizeof(*p));
	if (!p) {
		status = ICE_ERR_NO_MEMORY;
		goto err_ice_add_prof_to_lst;
	}

	p->profile_cookie = map->profile_cookie;
	p->prof_id = map->prof_id;
	p->tcam_count = map->ptg_cnt;

	for (i = 0; i < map->ptg_cnt; i++) {
		p->tcam[i].prof_id = map->prof_id;
		p->tcam[i].tcam_idx = ICE_INVALID_TCAM;
		p->tcam[i].ptg = map->ptg[i];
	}

	LIST_ADD(&p->list, lst);

err_ice_add_prof_to_lst:
	ice_release_lock(&hw->blk[blk].es.prof_map_lock);
	return status;
}

/**
 * ice_move_vsi - move VSI to another VSIG
 * @hw: pointer to the HW struct
 * @blk: hardware block
 * @vsi: the VSI to move
 * @vsig: the VSIG to move the VSI to
 * @chg: the change list
 */
static enum ice_status
ice_move_vsi(struct ice_hw *hw, enum ice_block blk, u16 vsi, u16 vsig,
	     struct LIST_HEAD_TYPE *chg)
{
	enum ice_status status;
	struct ice_chs_chg *p;
	u16 orig_vsig;

	p = (struct ice_chs_chg *)ice_malloc(hw, sizeof(*p));
	if (!p)
		return ICE_ERR_NO_MEMORY;

	status = ice_vsig_find_vsi(hw, blk, vsi, &orig_vsig);
	if (!status)
		status = ice_vsig_add_mv_vsi(hw, blk, vsi, vsig);

	if (status) {
		ice_free(hw, p);
		return status;
	}

	p->type = ICE_VSI_MOVE;
	p->vsi = vsi;
	p->orig_vsig = orig_vsig;
	p->vsig = vsig;

	LIST_ADD(&p->list_entry, chg);

	return ICE_SUCCESS;
}

/**
 * ice_rem_chg_tcam_ent - remove a specific TCAM entry from change list
 * @hw: pointer to the HW struct
 * @idx: the index of the TCAM entry to remove
 * @chg: the list of change structures to search
 */
static void
ice_rem_chg_tcam_ent(struct ice_hw *hw, u16 idx, struct LIST_HEAD_TYPE *chg)
{
	struct ice_chs_chg *pos, *tmp;

	LIST_FOR_EACH_ENTRY_SAFE(tmp, pos, chg, ice_chs_chg, list_entry)
		if (tmp->type == ICE_TCAM_ADD && tmp->tcam_idx == idx) {
			LIST_DEL(&tmp->list_entry);
			ice_free(hw, tmp);
		}
}

/**
 * ice_prof_tcam_ena_dis - add enable or disable TCAM change
 * @hw: pointer to the HW struct
 * @blk: hardware block
 * @enable: true to enable, false to disable
 * @vsig: the VSIG of the TCAM entry
 * @tcam: pointer the TCAM info structure of the TCAM to disable
 * @chg: the change list
 *
 * This function appends an enable or disable TCAM entry in the change log
 */
static enum ice_status
ice_prof_tcam_ena_dis(struct ice_hw *hw, enum ice_block blk, bool enable,
		      u16 vsig, struct ice_tcam_inf *tcam,
		      struct LIST_HEAD_TYPE *chg)
{
	enum ice_status status;
	struct ice_chs_chg *p;

	u8 vl_msk[ICE_TCAM_KEY_VAL_SZ] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
	u8 dc_msk[ICE_TCAM_KEY_VAL_SZ] = { 0xFF, 0xFF, 0x00, 0x00, 0x00 };
	u8 nm_msk[ICE_TCAM_KEY_VAL_SZ] = { 0x00, 0x00, 0x00, 0x00, 0x00 };

	/* if disabling, free the TCAM */
	if (!enable) {
		status = ice_rel_tcam_idx(hw, blk, tcam->tcam_idx);

		/* if we have already created a change for this TCAM entry, then
		 * we need to remove that entry, in order to prevent writing to
		 * a TCAM entry we no longer will have ownership of.
		 */
		ice_rem_chg_tcam_ent(hw, tcam->tcam_idx, chg);
		tcam->tcam_idx = 0;
		tcam->in_use = 0;
		return status;
	}

	/* for re-enabling, reallocate a TCAM */
	status = ice_alloc_tcam_ent(hw, blk, true, &tcam->tcam_idx);
	if (status)
		return status;

	/* add TCAM to change list */
	p = (struct ice_chs_chg *)ice_malloc(hw, sizeof(*p));
	if (!p)
		return ICE_ERR_NO_MEMORY;

	status = ice_tcam_write_entry(hw, blk, tcam->tcam_idx, tcam->prof_id,
				      tcam->ptg, vsig, 0, 0, vl_msk, dc_msk,
				      nm_msk);
	if (status)
		goto err_ice_prof_tcam_ena_dis;

	tcam->in_use = 1;

	p->type = ICE_TCAM_ADD;
	p->add_tcam_idx = true;
	p->prof_id = tcam->prof_id;
	p->ptg = tcam->ptg;
	p->vsig = 0;
	p->tcam_idx = tcam->tcam_idx;

	/* log change */
	LIST_ADD(&p->list_entry, chg);

	return ICE_SUCCESS;

err_ice_prof_tcam_ena_dis:
	ice_free(hw, p);
	return status;
}

/**
 * ice_adj_prof_priorities - adjust profile based on priorities
 * @hw: pointer to the HW struct
 * @blk: hardware block
 * @vsig: the VSIG for which to adjust profile priorities
 * @chg: the change list
 */
static enum ice_status
ice_adj_prof_priorities(struct ice_hw *hw, enum ice_block blk, u16 vsig,
			struct LIST_HEAD_TYPE *chg)
{
	ice_declare_bitmap(ptgs_used, ICE_XLT1_CNT);
	enum ice_status status = ICE_SUCCESS;
	struct ice_vsig_prof *t;
	u16 idx;

	ice_zero_bitmap(ptgs_used, ICE_XLT1_CNT);
	idx = vsig & ICE_VSIG_IDX_M;

	/* Priority is based on the order in which the profiles are added. The
	 * newest added profile has highest priority and the oldest added
	 * profile has the lowest priority. Since the profile property list for
	 * a VSIG is sorted from newest to oldest, this code traverses the list
	 * in order and enables the first of each PTG that it finds (that is not
	 * already enabled); it also disables any duplicate PTGs that it finds
	 * in the older profiles (that are currently enabled).
	 */

	LIST_FOR_EACH_ENTRY(t, &hw->blk[blk].xlt2.vsig_tbl[idx].prop_lst,
			    ice_vsig_prof, list) {
		u16 i;

		for (i = 0; i < t->tcam_count; i++) {
			bool used;

			/* Scan the priorities from newest to oldest.
			 * Make sure that the newest profiles take priority.
			 */
			used = ice_is_bit_set(ptgs_used, t->tcam[i].ptg);

			if (used && t->tcam[i].in_use) {
				/* need to mark this PTG as never match, as it
				 * was already in use and therefore duplicate
				 * (and lower priority)
				 */
				status = ice_prof_tcam_ena_dis(hw, blk, false,
							       vsig,
							       &t->tcam[i],
							       chg);
				if (status)
					return status;
			} else if (!used && !t->tcam[i].in_use) {
				/* need to enable this PTG, as it in not in use
				 * and not enabled (highest priority)
				 */
				status = ice_prof_tcam_ena_dis(hw, blk, true,
							       vsig,
							       &t->tcam[i],
							       chg);
				if (status)
					return status;
			}

			/* keep track of used ptgs */
			ice_set_bit(t->tcam[i].ptg, ptgs_used);
		}
	}

	return status;
}

/**
 * ice_add_prof_id_vsig - add profile to VSIG
 * @hw: pointer to the HW struct
 * @blk: hardware block
 * @vsig: the VSIG to which this profile is to be added
 * @hdl: the profile handle indicating the profile to add
 * @rev: true to add entries to the end of the list
 * @chg: the change list
 */
static enum ice_status
ice_add_prof_id_vsig(struct ice_hw *hw, enum ice_block blk, u16 vsig, u64 hdl,
		     bool rev, struct LIST_HEAD_TYPE *chg)
{
	/* Masks that ignore flags */
	u8 vl_msk[ICE_TCAM_KEY_VAL_SZ] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
	u8 dc_msk[ICE_TCAM_KEY_VAL_SZ] = { 0xFF, 0xFF, 0x00, 0x00, 0x00 };
	u8 nm_msk[ICE_TCAM_KEY_VAL_SZ] = { 0x00, 0x00, 0x00, 0x00, 0x00 };
	enum ice_status status = ICE_SUCCESS;
	struct ice_prof_map *map;
	struct ice_vsig_prof *t;
	struct ice_chs_chg *p;
	u16 vsig_idx, i;

	/* Error, if this VSIG already has this profile */
	if (ice_has_prof_vsig(hw, blk, vsig, hdl))
		return ICE_ERR_ALREADY_EXISTS;

	/* new VSIG profile structure */
	t = (struct ice_vsig_prof *)ice_malloc(hw, sizeof(*t));
	if (!t)
		return ICE_ERR_NO_MEMORY;

	ice_acquire_lock(&hw->blk[blk].es.prof_map_lock);
	/* Get the details on the profile specified by the handle ID */
	map = ice_search_prof_id(hw, blk, hdl);
	if (!map) {
		status = ICE_ERR_DOES_NOT_EXIST;
		goto err_ice_add_prof_id_vsig;
	}

	t->profile_cookie = map->profile_cookie;
	t->prof_id = map->prof_id;
	t->tcam_count = map->ptg_cnt;

	/* create TCAM entries */
	for (i = 0; i < map->ptg_cnt; i++) {
		u16 tcam_idx;

		/* add TCAM to change list */
		p = (struct ice_chs_chg *)ice_malloc(hw, sizeof(*p));
		if (!p) {
			status = ICE_ERR_NO_MEMORY;
			goto err_ice_add_prof_id_vsig;
		}

		/* allocate the TCAM entry index */
		status = ice_alloc_tcam_ent(hw, blk, true, &tcam_idx);
		if (status) {
			ice_free(hw, p);
			goto err_ice_add_prof_id_vsig;
		}

		t->tcam[i].ptg = map->ptg[i];
		t->tcam[i].prof_id = map->prof_id;
		t->tcam[i].tcam_idx = tcam_idx;
		t->tcam[i].in_use = true;

		p->type = ICE_TCAM_ADD;
		p->add_tcam_idx = true;
		p->prof_id = t->tcam[i].prof_id;
		p->ptg = t->tcam[i].ptg;
		p->vsig = vsig;
		p->tcam_idx = t->tcam[i].tcam_idx;

		/* write the TCAM entry */
		status = ice_tcam_write_entry(hw, blk, t->tcam[i].tcam_idx,
					      t->tcam[i].prof_id,
					      t->tcam[i].ptg, vsig, 0, 0,
					      vl_msk, dc_msk, nm_msk);
		if (status) {
			ice_free(hw, p);
			goto err_ice_add_prof_id_vsig;
		}

		/* log change */
		LIST_ADD(&p->list_entry, chg);
	}

	/* add profile to VSIG */
	vsig_idx = vsig & ICE_VSIG_IDX_M;
	if (rev)
		LIST_ADD_TAIL(&t->list,
			      &hw->blk[blk].xlt2.vsig_tbl[vsig_idx].prop_lst);
	else
		LIST_ADD(&t->list,
			 &hw->blk[blk].xlt2.vsig_tbl[vsig_idx].prop_lst);

	ice_release_lock(&hw->blk[blk].es.prof_map_lock);
	return status;

err_ice_add_prof_id_vsig:
	ice_release_lock(&hw->blk[blk].es.prof_map_lock);
	/* let caller clean up the change list */
	ice_free(hw, t);
	return status;
}

/**
 * ice_create_prof_id_vsig - add a new VSIG with a single profile
 * @hw: pointer to the HW struct
 * @blk: hardware block
 * @vsi: the initial VSI that will be in VSIG
 * @hdl: the profile handle of the profile that will be added to the VSIG
 * @chg: the change list
 */
static enum ice_status
ice_create_prof_id_vsig(struct ice_hw *hw, enum ice_block blk, u16 vsi, u64 hdl,
			struct LIST_HEAD_TYPE *chg)
{
	enum ice_status status;
	struct ice_chs_chg *p;
	u16 new_vsig;

	p = (struct ice_chs_chg *)ice_malloc(hw, sizeof(*p));
	if (!p)
		return ICE_ERR_NO_MEMORY;

	new_vsig = ice_vsig_alloc(hw, blk);
	if (!new_vsig) {
		status = ICE_ERR_HW_TABLE;
		goto err_ice_create_prof_id_vsig;
	}

	status = ice_move_vsi(hw, blk, vsi, new_vsig, chg);
	if (status)
		goto err_ice_create_prof_id_vsig;

	status = ice_add_prof_id_vsig(hw, blk, new_vsig, hdl, false, chg);
	if (status)
		goto err_ice_create_prof_id_vsig;

	p->type = ICE_VSIG_ADD;
	p->vsi = vsi;
	p->orig_vsig = ICE_DEFAULT_VSIG;
	p->vsig = new_vsig;

	LIST_ADD(&p->list_entry, chg);

	return ICE_SUCCESS;

err_ice_create_prof_id_vsig:
	/* let caller clean up the change list */
	ice_free(hw, p);
	return status;
}

/**
 * ice_create_vsig_from_lst - create a new VSIG with a list of profiles
 * @hw: pointer to the HW struct
 * @blk: hardware block
 * @vsi: the initial VSI that will be in VSIG
 * @lst: the list of profile that will be added to the VSIG
 * @new_vsig: return of new VSIG
 * @chg: the change list
 */
static enum ice_status
ice_create_vsig_from_lst(struct ice_hw *hw, enum ice_block blk, u16 vsi,
			 struct LIST_HEAD_TYPE *lst, u16 *new_vsig,
			 struct LIST_HEAD_TYPE *chg)
{
	struct ice_vsig_prof *t;
	enum ice_status status;
	u16 vsig;

	vsig = ice_vsig_alloc(hw, blk);
	if (!vsig)
		return ICE_ERR_HW_TABLE;

	status = ice_move_vsi(hw, blk, vsi, vsig, chg);
	if (status)
		return status;

	LIST_FOR_EACH_ENTRY(t, lst, ice_vsig_prof, list) {
		/* Reverse the order here since we are copying the list */
		status = ice_add_prof_id_vsig(hw, blk, vsig, t->profile_cookie,
					      true, chg);
		if (status)
			return status;
	}

	*new_vsig = vsig;

	return ICE_SUCCESS;
}

/**
 * ice_find_prof_vsig - find a VSIG with a specific profile handle
 * @hw: pointer to the HW struct
 * @blk: hardware block
 * @hdl: the profile handle of the profile to search for
 * @vsig: returns the VSIG with the matching profile
 */
static bool
ice_find_prof_vsig(struct ice_hw *hw, enum ice_block blk, u64 hdl, u16 *vsig)
{
	struct ice_vsig_prof *t;
	enum ice_status status;
	struct LIST_HEAD_TYPE lst;

	INIT_LIST_HEAD(&lst);

	t = (struct ice_vsig_prof *)ice_malloc(hw, sizeof(*t));
	if (!t)
		return false;

	t->profile_cookie = hdl;
	LIST_ADD(&t->list, &lst);

	status = ice_find_dup_props_vsig(hw, blk, &lst, vsig);

	LIST_DEL(&t->list);
	ice_free(hw, t);

	return status == ICE_SUCCESS;
}

/**
 * ice_add_vsi_flow - add VSI flow
 * @hw: pointer to the HW struct
 * @blk: hardware block
 * @vsi: input VSI
 * @vsig: target VSIG to include the input VSI
 *
 * Calling this function will add the VSI to a given VSIG and
 * update the HW tables accordingly. This call can be used to
 * add multiple VSIs to a VSIG if we know beforehand that those
 * VSIs have the same characteristics of the VSIG. This will
 * save time in generating a new VSIG and TCAMs till a match is
 * found and subsequent rollback when a matching VSIG is found.
 */
enum ice_status
ice_add_vsi_flow(struct ice_hw *hw, enum ice_block blk, u16 vsi, u16 vsig)
{
	struct ice_chs_chg *tmp, *del;
	struct LIST_HEAD_TYPE chg;
	enum ice_status status;

	/* if target VSIG is default the move is invalid */
	if ((vsig & ICE_VSIG_IDX_M) == ICE_DEFAULT_VSIG)
		return ICE_ERR_PARAM;

	INIT_LIST_HEAD(&chg);

	/* move VSI to the VSIG that matches */
	status = ice_move_vsi(hw, blk, vsi, vsig, &chg);
	/* update hardware if success */
	if (!status)
		status = ice_upd_prof_hw(hw, blk, &chg);

	LIST_FOR_EACH_ENTRY_SAFE(del, tmp, &chg, ice_chs_chg, list_entry) {
		LIST_DEL(&del->list_entry);
		ice_free(hw, del);
	}

	return status;
}

/**
 * ice_add_prof_id_flow - add profile flow
 * @hw: pointer to the HW struct
 * @blk: hardware block
 * @vsi: the VSI to enable with the profile specified by ID
 * @hdl: profile handle
 *
 * Calling this function will update the hardware tables to enable the
 * profile indicated by the ID parameter for the VSIs specified in the VSI
 * array. Once successfully called, the flow will be enabled.
 */
enum ice_status
ice_add_prof_id_flow(struct ice_hw *hw, enum ice_block blk, u16 vsi, u64 hdl)
{
	struct ice_vsig_prof *tmp1, *del1;
	struct ice_chs_chg *tmp, *del;
	struct LIST_HEAD_TYPE union_lst;
	enum ice_status status;
	struct LIST_HEAD_TYPE chg;
	u16 vsig;

	INIT_LIST_HEAD(&union_lst);
	INIT_LIST_HEAD(&chg);

	/* Get profile */
	status = ice_get_prof(hw, blk, hdl, &chg);
	if (status)
		return status;

	/* determine if VSI is already part of a VSIG */
	status = ice_vsig_find_vsi(hw, blk, vsi, &vsig);
	if (!status && vsig) {
		bool only_vsi;
		u16 or_vsig;
		u16 ref;

		/* found in VSIG */
		or_vsig = vsig;

		/* make sure that there is no overlap/conflict between the new
		 * characteristics and the existing ones; we don't support that
		 * scenario
		 */
		if (ice_has_prof_vsig(hw, blk, vsig, hdl)) {
			status = ICE_ERR_ALREADY_EXISTS;
			goto err_ice_add_prof_id_flow;
		}

		/* last VSI in the VSIG? */
		status = ice_vsig_get_ref(hw, blk, vsig, &ref);
		if (status)
			goto err_ice_add_prof_id_flow;
		only_vsi = (ref == 1);

		/* create a union of the current profiles and the one being
		 * added
		 */
		status = ice_get_profs_vsig(hw, blk, vsig, &union_lst);
		if (status)
			goto err_ice_add_prof_id_flow;

		status = ice_add_prof_to_lst(hw, blk, &union_lst, hdl);
		if (status)
			goto err_ice_add_prof_id_flow;

		/* search for an existing VSIG with an exact charc match */
		status = ice_find_dup_props_vsig(hw, blk, &union_lst, &vsig);
		if (!status) {
			/* move VSI to the VSIG that matches */
			status = ice_move_vsi(hw, blk, vsi, vsig, &chg);
			if (status)
				goto err_ice_add_prof_id_flow;

			/* VSI has been moved out of or_vsig. If the or_vsig had
			 * only that VSI it is now empty and can be removed.
			 */
			if (only_vsi) {
				status = ice_rem_vsig(hw, blk, or_vsig, &chg);
				if (status)
					goto err_ice_add_prof_id_flow;
			}
		} else if (only_vsi) {
			/* If the original VSIG only contains one VSI, then it
			 * will be the requesting VSI. In this case the VSI is
			 * not sharing entries and we can simply add the new
			 * profile to the VSIG.
			 */
			status = ice_add_prof_id_vsig(hw, blk, vsig, hdl, false,
						      &chg);
			if (status)
				goto err_ice_add_prof_id_flow;

			/* Adjust priorities */
			status = ice_adj_prof_priorities(hw, blk, vsig, &chg);
			if (status)
				goto err_ice_add_prof_id_flow;
		} else {
			/* No match, so we need a new VSIG */
			status = ice_create_vsig_from_lst(hw, blk, vsi,
							  &union_lst, &vsig,
							  &chg);
			if (status)
				goto err_ice_add_prof_id_flow;

			/* Adjust priorities */
			status = ice_adj_prof_priorities(hw, blk, vsig, &chg);
			if (status)
				goto err_ice_add_prof_id_flow;
		}
	} else {
		/* need to find or add a VSIG */
		/* search for an existing VSIG with an exact charc match */
		if (ice_find_prof_vsig(hw, blk, hdl, &vsig)) {
			/* found an exact match */
			/* add or move VSI to the VSIG that matches */
			status = ice_move_vsi(hw, blk, vsi, vsig, &chg);
			if (status)
				goto err_ice_add_prof_id_flow;
		} else {
			/* we did not find an exact match */
			/* we need to add a VSIG */
			status = ice_create_prof_id_vsig(hw, blk, vsi, hdl,
							 &chg);
			if (status)
				goto err_ice_add_prof_id_flow;
		}
	}

	/* update hardware */
	if (!status)
		status = ice_upd_prof_hw(hw, blk, &chg);

err_ice_add_prof_id_flow:
	LIST_FOR_EACH_ENTRY_SAFE(del, tmp, &chg, ice_chs_chg, list_entry) {
		LIST_DEL(&del->list_entry);
		ice_free(hw, del);
	}

	LIST_FOR_EACH_ENTRY_SAFE(del1, tmp1, &union_lst, ice_vsig_prof, list) {
		LIST_DEL(&del1->list);
		ice_free(hw, del1);
	}

	return status;
}

/**
 * ice_add_flow - add flow
 * @hw: pointer to the HW struct
 * @blk: hardware block
 * @vsi: array of VSIs to enable with the profile specified by ID
 * @count: number of elements in the VSI array
 * @id: profile tracking ID
 *
 * Calling this function will update the hardware tables to enable the
 * profile indicated by the ID parameter for the VSIs specified in the VSI
 * array. Once successfully called, the flow will be enabled.
 */
enum ice_status
ice_add_flow(struct ice_hw *hw, enum ice_block blk, u16 vsi[], u8 count,
	     u64 id)
{
	u16 i;

	for (i = 0; i < count; i++) {
		enum ice_status status;

		status = ice_add_prof_id_flow(hw, blk, vsi[i], id);
		if (status)
			return status;
	}

	return ICE_SUCCESS;
}

/**
 * ice_rem_prof_from_list - remove a profile from list
 * @hw: pointer to the HW struct
 * @lst: list to remove the profile from
 * @hdl: the profile handle indicating the profile to remove
 */
static enum ice_status
ice_rem_prof_from_list(struct ice_hw *hw, struct LIST_HEAD_TYPE *lst, u64 hdl)
{
	struct ice_vsig_prof *ent, *tmp;

	LIST_FOR_EACH_ENTRY_SAFE(ent, tmp, lst, ice_vsig_prof, list)
		if (ent->profile_cookie == hdl) {
			LIST_DEL(&ent->list);
			ice_free(hw, ent);
			return ICE_SUCCESS;
		}

	return ICE_ERR_DOES_NOT_EXIST;
}

/**
 * ice_rem_prof_id_flow - remove flow
 * @hw: pointer to the HW struct
 * @blk: hardware block
 * @vsi: the VSI from which to remove the profile specified by ID
 * @hdl: profile tracking handle
 *
 * Calling this function will update the hardware tables to remove the
 * profile indicated by the ID parameter for the VSIs specified in the VSI
 * array. Once successfully called, the flow will be disabled.
 */
enum ice_status
ice_rem_prof_id_flow(struct ice_hw *hw, enum ice_block blk, u16 vsi, u64 hdl)
{
	struct ice_vsig_prof *tmp1, *del1;
	struct ice_chs_chg *tmp, *del;
	struct LIST_HEAD_TYPE chg, copy;
	enum ice_status status;
	u16 vsig;

	INIT_LIST_HEAD(&copy);
	INIT_LIST_HEAD(&chg);

	/* determine if VSI is already part of a VSIG */
	status = ice_vsig_find_vsi(hw, blk, vsi, &vsig);
	if (!status && vsig) {
		bool last_profile;
		bool only_vsi;
		u16 ref;

		/* found in VSIG */
		last_profile = ice_vsig_prof_id_count(hw, blk, vsig) == 1;
		status = ice_vsig_get_ref(hw, blk, vsig, &ref);
		if (status)
			goto err_ice_rem_prof_id_flow;
		only_vsi = (ref == 1);

		if (only_vsi) {
			/* If the original VSIG only contains one reference,
			 * which will be the requesting VSI, then the VSI is not
			 * sharing entries and we can simply remove the specific
			 * characteristics from the VSIG.
			 */

			if (last_profile) {
				/* If there are no profiles left for this VSIG,
				 * then simply remove the VSIG.
				 */
				status = ice_rem_vsig(hw, blk, vsig, &chg);
				if (status)
					goto err_ice_rem_prof_id_flow;
			} else {
				status = ice_rem_prof_id_vsig(hw, blk, vsig,
							      hdl, &chg);
				if (status)
					goto err_ice_rem_prof_id_flow;

				/* Adjust priorities */
				status = ice_adj_prof_priorities(hw, blk, vsig,
								 &chg);
				if (status)
					goto err_ice_rem_prof_id_flow;
			}

		} else {
			/* Make a copy of the VSIG's list of Profiles */
			status = ice_get_profs_vsig(hw, blk, vsig, &copy);
			if (status)
				goto err_ice_rem_prof_id_flow;

			/* Remove specified profile entry from the list */
			status = ice_rem_prof_from_list(hw, &copy, hdl);
			if (status)
				goto err_ice_rem_prof_id_flow;

			if (LIST_EMPTY(&copy)) {
				status = ice_move_vsi(hw, blk, vsi,
						      ICE_DEFAULT_VSIG, &chg);
				if (status)
					goto err_ice_rem_prof_id_flow;

			} else if (!ice_find_dup_props_vsig(hw, blk, &copy,
							    &vsig)) {
				/* found an exact match */
				/* add or move VSI to the VSIG that matches */
				/* Search for a VSIG with a matching profile
				 * list
				 */

				/* Found match, move VSI to the matching VSIG */
				status = ice_move_vsi(hw, blk, vsi, vsig, &chg);
				if (status)
					goto err_ice_rem_prof_id_flow;
			} else {
				/* since no existing VSIG supports this
				 * characteristic pattern, we need to create a
				 * new VSIG and TCAM entries
				 */
				status = ice_create_vsig_from_lst(hw, blk, vsi,
								  &copy, &vsig,
								  &chg);
				if (status)
					goto err_ice_rem_prof_id_flow;

				/* Adjust priorities */
				status = ice_adj_prof_priorities(hw, blk, vsig,
								 &chg);
				if (status)
					goto err_ice_rem_prof_id_flow;
			}
		}
	} else {
		status = ICE_ERR_DOES_NOT_EXIST;
	}

	/* update hardware tables */
	if (!status)
		status = ice_upd_prof_hw(hw, blk, &chg);

err_ice_rem_prof_id_flow:
	LIST_FOR_EACH_ENTRY_SAFE(del, tmp, &chg, ice_chs_chg, list_entry) {
		LIST_DEL(&del->list_entry);
		ice_free(hw, del);
	}

	LIST_FOR_EACH_ENTRY_SAFE(del1, tmp1, &copy, ice_vsig_prof, list) {
		LIST_DEL(&del1->list);
		ice_free(hw, del1);
	}

	return status;
}

/**
 * ice_rem_flow - remove flow
 * @hw: pointer to the HW struct
 * @blk: hardware block
 * @vsi: array of VSIs from which to remove the profile specified by ID
 * @count: number of elements in the VSI array
 * @id: profile tracking ID
 *
 * The function will remove flows from the specified VSIs that were enabled
 * using ice_add_flow. The ID value will indicated which profile will be
 * removed. Once successfully called, the flow will be disabled.
 */
enum ice_status
ice_rem_flow(struct ice_hw *hw, enum ice_block blk, u16 vsi[], u8 count,
	     u64 id)
{
	u16 i;

	for (i = 0; i < count; i++) {
		enum ice_status status;

		status = ice_rem_prof_id_flow(hw, blk, vsi[i], id);
		if (status)
			return status;
	}

	return ICE_SUCCESS;
}
