/******************************************************************************
  SPDX-License-Identifier: BSD-3-Clause

  Copyright (c) 2025, Intel Corporation
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

   3. Neither the name of the Intel Corporation nor the names of its
      contributors may be used to endorse or promote products derived from
      this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.

******************************************************************************/

#ifndef _IXGBE_TYPE_E610_H_
#define _IXGBE_TYPE_E610_H_


/* Generic defines */
#ifndef BIT
#define BIT(a) (1UL << (a))
#endif /* !BIT */
#ifndef BIT_ULL
#define BIT_ULL(a) (1ULL << (a))
#endif /* !BIT_ULL */
#ifndef BITS_PER_BYTE
#define BITS_PER_BYTE	8
#endif /* !BITS_PER_BYTE */
#ifndef DIVIDE_AND_ROUND_UP
#define DIVIDE_AND_ROUND_UP(a, b) (((a) + (b) - 1) / (b))
#endif /* !DIVIDE_AND_ROUND_UP */

#ifndef ROUND_UP
/**
 * ROUND_UP - round up to next arbitrary multiple (not a power of 2)
 * @a: value to round up
 * @b: arbitrary multiple
 *
 * Round up to the next multiple of the arbitrary b.
 */
#define ROUND_UP(a, b)	((b) * DIVIDE_AND_ROUND_UP((a), (b)))
#endif /* !ROUND_UP */

#define MAKEMASK(mask, shift) (mask << shift)

#define BYTES_PER_WORD	2
#define BYTES_PER_DWORD	4

#ifndef BITS_PER_LONG
#define BITS_PER_LONG		64
#endif /* !BITS_PER_LONG */
#ifndef BITS_PER_LONG_LONG
#define BITS_PER_LONG_LONG	64
#endif /* !BITS_PER_LONG_LONG */
#undef GENMASK
#define GENMASK(h, l) \
	(((~0UL) << (l)) & (~0UL >> (BITS_PER_LONG - 1 - (h))))
#undef GENMASK_ULL
#define GENMASK_ULL(h, l) \
	(((~0ULL) << (l)) & (~0ULL >> (BITS_PER_LONG_LONG - 1 - (h))))

/* Data type manipulation macros. */
#define HI_DWORD(x)	((u32)((((x) >> 16) >> 16) & 0xFFFFFFFF))
#define LO_DWORD(x)	((u32)((x) & 0xFFFFFFFF))
#define HI_WORD(x)	((u16)(((x) >> 16) & 0xFFFF))
#define LO_WORD(x)	((u16)((x) & 0xFFFF))
#define HI_BYTE(x)	((u8)(((x) >> 8) & 0xFF))
#define LO_BYTE(x)	((u8)((x) & 0xFF))

#ifndef MIN_T
#define MIN_T(_t, _a, _b)	min((_t)(_a), (_t)(_b))
#endif

#define IS_ASCII(_ch)	((_ch) < 0x80)

/**
 * ixgbe_struct_size - size of struct with C99 flexible array member
 * @ptr: pointer to structure
 * @field: flexible array member (last member of the structure)
 * @num: number of elements of that flexible array member
 */
#define ixgbe_struct_size(ptr, field, num) \
	(sizeof(*(ptr)) + sizeof(*(ptr)->field) * (num))

/* General E610 defines */
#define IXGBE_MAX_VSI			768

/* Auxiliary field, mask and shift definition for Shadow RAM and NVM Flash */
#define E610_SR_VPD_SIZE_WORDS		512
#define E610_SR_PCIE_ALT_SIZE_WORDS	512

/* Checksum and Shadow RAM pointers */
#define E610_SR_NVM_DEV_STARTER_VER		0x18
#define E610_NVM_VER_LO_SHIFT			0
#define E610_NVM_VER_LO_MASK			(0xff << E610_NVM_VER_LO_SHIFT)
#define E610_NVM_VER_HI_SHIFT			12
#define E610_NVM_VER_HI_MASK			(0xf << E610_NVM_VER_HI_SHIFT)
#define E610_SR_NVM_MAP_VER			0x29
#define E610_SR_NVM_EETRACK_LO			0x2D
#define E610_SR_NVM_EETRACK_HI			0x2E
#define E610_SR_VPD_PTR				0x2F
#define E610_SR_PCIE_ALT_AUTO_LOAD_PTR		0x3E
#define E610_SR_SW_CHECKSUM_WORD		0x3F
#define E610_SR_PFA_PTR				0x40
#define E610_SR_1ST_NVM_BANK_PTR		0x42
#define E610_SR_NVM_BANK_SIZE			0x43
#define E610_SR_1ST_OROM_BANK_PTR		0x44
#define E610_SR_OROM_BANK_SIZE			0x45
#define E610_SR_NETLIST_BANK_PTR		0x46
#define E610_SR_NETLIST_BANK_SIZE		0x47
#define E610_SR_POINTER_TYPE_BIT		BIT(15)
#define E610_SR_POINTER_MASK			0x7fff
#define E610_SR_HALF_4KB_SECTOR_UNITS		2048
#define E610_GET_PFA_POINTER_IN_WORDS(offset)				    \
    ((offset & E610_SR_POINTER_TYPE_BIT) == E610_SR_POINTER_TYPE_BIT) ?     \
        ((offset & E610_SR_POINTER_MASK) * E610_SR_HALF_4KB_SECTOR_UNITS) : \
        (offset & E610_SR_POINTER_MASK)

/* Checksum and Shadow RAM pointers */
#define E610_SR_NVM_CTRL_WORD		0x00
#define E610_SR_PBA_BLOCK_PTR		0x16

/* The Orom version topology */
#define IXGBE_OROM_VER_PATCH_SHIFT	0
#define IXGBE_OROM_VER_PATCH_MASK	(0xff << IXGBE_OROM_VER_PATCH_SHIFT)
#define IXGBE_OROM_VER_BUILD_SHIFT	8
#define IXGBE_OROM_VER_BUILD_MASK	(0xffff << IXGBE_OROM_VER_BUILD_SHIFT)
#define IXGBE_OROM_VER_SHIFT		24
#define IXGBE_OROM_VER_MASK		(0xff << IXGBE_OROM_VER_SHIFT)

/* CSS Header words */
#define IXGBE_NVM_CSS_HDR_LEN_L			0x02
#define IXGBE_NVM_CSS_HDR_LEN_H			0x03
#define IXGBE_NVM_CSS_SREV_L			0x14
#define IXGBE_NVM_CSS_SREV_H			0x15

/* Length of Authentication header section in words */
#define IXGBE_NVM_AUTH_HEADER_LEN		0x08

/* The Netlist ID Block is located after all of the Link Topology nodes. */
#define IXGBE_NETLIST_ID_BLK_SIZE		0x30
#define IXGBE_NETLIST_ID_BLK_OFFSET(n)		IXGBE_NETLIST_LINK_TOPO_OFFSET(0x0004 + 2 * (n))

/* netlist ID block field offsets (word offsets) */
#define IXGBE_NETLIST_ID_BLK_MAJOR_VER_LOW	0x02
#define IXGBE_NETLIST_ID_BLK_MAJOR_VER_HIGH	0x03
#define IXGBE_NETLIST_ID_BLK_MINOR_VER_LOW	0x04
#define IXGBE_NETLIST_ID_BLK_MINOR_VER_HIGH	0x05
#define IXGBE_NETLIST_ID_BLK_TYPE_LOW		0x06
#define IXGBE_NETLIST_ID_BLK_TYPE_HIGH		0x07
#define IXGBE_NETLIST_ID_BLK_REV_LOW		0x08
#define IXGBE_NETLIST_ID_BLK_REV_HIGH		0x09
#define IXGBE_NETLIST_ID_BLK_SHA_HASH_WORD(n)	(0x0A + (n))
#define IXGBE_NETLIST_ID_BLK_CUST_VER		0x2F

/* The Link Topology Netlist section is stored as a series of words. It is
 * stored in the NVM as a TLV, with the first two words containing the type
 * and length.
 */
#define IXGBE_NETLIST_LINK_TOPO_MOD_ID		0x011B
#define IXGBE_NETLIST_TYPE_OFFSET		0x0000
#define IXGBE_NETLIST_LEN_OFFSET		0x0001

/* The Link Topology section follows the TLV header. When reading the netlist
 * using ixgbe_read_netlist_module, we need to account for the 2-word TLV
 * header.
 */
#define IXGBE_NETLIST_LINK_TOPO_OFFSET(n)	((n) + 2)
#define IXGBE_LINK_TOPO_MODULE_LEN	IXGBE_NETLIST_LINK_TOPO_OFFSET(0x0000)
#define IXGBE_LINK_TOPO_NODE_COUNT	IXGBE_NETLIST_LINK_TOPO_OFFSET(0x0001)
#define IXGBE_LINK_TOPO_NODE_COUNT_M	MAKEMASK(0x3FF, 0)

/* Auxiliary field, mask and shift definition for Shadow RAM and NVM Flash */
#define IXGBE_SR_CTRL_WORD_1_S		0x06
#define IXGBE_SR_CTRL_WORD_1_M		(0x03 << IXGBE_SR_CTRL_WORD_1_S)
#define IXGBE_SR_CTRL_WORD_VALID	0x1
#define IXGBE_SR_CTRL_WORD_OROM_BANK	BIT(3)
#define IXGBE_SR_CTRL_WORD_NETLIST_BANK	BIT(4)
#define IXGBE_SR_CTRL_WORD_NVM_BANK	BIT(5)
#define IXGBE_SR_NVM_PTR_4KB_UNITS	BIT(15)

/* These macros strip from NVM Image Revision the particular part of NVM ver:
   major ver, minor ver and image id */
#define E610_NVM_MAJOR_VER(x)	((x & 0xF000) >> 12)
#define E610_NVM_MINOR_VER(x)	(x & 0x00FF)

/* Shadow RAM related */
#define IXGBE_SR_SECTOR_SIZE_IN_WORDS		0x800
#define IXGBE_SR_WORDS_IN_1KB			512
/* Checksum should be calculated such that after adding all the words,
 * including the checksum word itself, the sum should be 0xBABA.
 */
#define IXGBE_SR_SW_CHECKSUM_BASE		0xBABA

/* Netlist */
#define IXGBE_MAX_NETLIST_SIZE			10

/* General registers */

/* Firmware Status Register (GL_FWSTS) */
#define GL_FWSTS				0x00083048 /* Reset Source: POR */
#define GL_FWSTS_FWS0B_S			0
#define GL_FWSTS_FWS0B_M			MAKEMASK(0xFF, 0)
#define GL_FWSTS_FWROWD_S			8
#define GL_FWSTS_FWROWD_M			BIT(8)
#define GL_FWSTS_FWRI_S				9
#define GL_FWSTS_FWRI_M				BIT(9)
#define GL_FWSTS_FWS1B_S			16
#define GL_FWSTS_FWS1B_M			MAKEMASK(0xFF, 16)
#define GL_FWSTS_EP_PF0				BIT(24)
#define GL_FWSTS_EP_PF1				BIT(25)

/* Recovery mode values of Firmware Status 1 Byte (FWS1B) bitfield */
#define GL_FWSTS_FWS1B_RECOVERY_MODE_CORER_LEGACY  0x0B
#define GL_FWSTS_FWS1B_RECOVERY_MODE_GLOBR_LEGACY  0x0C
#define GL_FWSTS_FWS1B_RECOVERY_MODE_CORER         0x30
#define GL_FWSTS_FWS1B_RECOVERY_MODE_GLOBR         0x31
#define GL_FWSTS_FWS1B_RECOVERY_MODE_TRANSITION    0x32
#define GL_FWSTS_FWS1B_RECOVERY_MODE_NVM           0x33

/* Firmware Status (GL_MNG_FWSM) */
#define GL_MNG_FWSM				0x000B6134 /* Reset Source: POR */
#define GL_MNG_FWSM_FW_MODES_S			0
#define GL_MNG_FWSM_FW_MODES_M			MAKEMASK(0x7, 0)
#define GL_MNG_FWSM_RSV0_S			2
#define GL_MNG_FWSM_RSV0_M			MAKEMASK(0xFF, 2)
#define GL_MNG_FWSM_EEP_RELOAD_IND_S		10
#define GL_MNG_FWSM_EEP_RELOAD_IND_M		BIT(10)
#define GL_MNG_FWSM_RSV1_S			11
#define GL_MNG_FWSM_RSV1_M			MAKEMASK(0xF, 11)
#define GL_MNG_FWSM_RSV2_S			15
#define GL_MNG_FWSM_RSV2_M			BIT(15)
#define GL_MNG_FWSM_PCIR_AL_FAILURE_S		16
#define GL_MNG_FWSM_PCIR_AL_FAILURE_M		BIT(16)
#define GL_MNG_FWSM_POR_AL_FAILURE_S		17
#define GL_MNG_FWSM_POR_AL_FAILURE_M		BIT(17)
#define GL_MNG_FWSM_RSV3_S			18
#define GL_MNG_FWSM_RSV3_M			BIT(18)
#define GL_MNG_FWSM_EXT_ERR_IND_S		19
#define GL_MNG_FWSM_EXT_ERR_IND_M		MAKEMASK(0x3F, 19)
#define GL_MNG_FWSM_RSV4_S			25
#define GL_MNG_FWSM_RSV4_M			BIT(25)
#define GL_MNG_FWSM_RESERVED_11_S		26
#define GL_MNG_FWSM_RESERVED_11_M		MAKEMASK(0xF, 26)
#define GL_MNG_FWSM_RSV5_S			30
#define GL_MNG_FWSM_RSV5_M			MAKEMASK(0x3, 30)

/* FW mode indications */
#define GL_MNG_FWSM_FW_MODES_DEBUG_M           BIT(0)
#define GL_MNG_FWSM_FW_MODES_RECOVERY_M        BIT(1)
#define GL_MNG_FWSM_FW_MODES_ROLLBACK_M        BIT(2)

/* Global NVM General Status Register */
#define GLNVM_GENS				0x000B6100 /* Reset Source: POR */
#define GLNVM_GENS_NVM_PRES_S			0
#define GLNVM_GENS_NVM_PRES_M			BIT(0)
#define GLNVM_GENS_SR_SIZE_S			5
#define GLNVM_GENS_SR_SIZE_M			MAKEMASK(0x7, 5)
#define GLNVM_GENS_BANK1VAL_S			8
#define GLNVM_GENS_BANK1VAL_M			BIT(8)
#define GLNVM_GENS_ALT_PRST_S			23
#define GLNVM_GENS_ALT_PRST_M			BIT(23)
#define GLNVM_GENS_FL_AUTO_RD_S			25
#define GLNVM_GENS_FL_AUTO_RD_M			BIT(25)

/* Flash Access Register */
#define GLNVM_FLA				0x000B6108 /* Reset Source: POR */
#define GLNVM_FLA_LOCKED_S			6
#define GLNVM_FLA_LOCKED_M			BIT(6)

/* Bit Bang registers */
#define RDASB_MSGCTL				0x000B6820
#define RDASB_MSGCTL_HDR_DWS_S			0
#define RDASB_MSGCTL_EXP_RDW_S			8
#define RDASB_MSGCTL_CMDV_M			BIT(31)
#define RDASB_RSPCTL				0x000B6824
#define RDASB_RSPCTL_BAD_LENGTH_M		BIT(30)
#define RDASB_RSPCTL_NOT_SUCCESS_M		BIT(31)
#define RDASB_WHDR0				0x000B68F4
#define RDASB_WHDR1				0x000B68F8
#define RDASB_WHDR2				0x000B68FC
#define RDASB_WHDR3				0x000B6900
#define RDASB_WHDR4				0x000B6904
#define RDASB_RHDR0				0x000B6AFC
#define RDASB_RHDR0_RESPONSE_S			27
#define RDASB_RHDR0_RESPONSE_M			MAKEMASK(0x7, 27)
#define RDASB_RDATA0				0x000B6B00
#define RDASB_RDATA1				0x000B6B04

/* SPI Registers */
#define SPISB_MSGCTL				0x000B7020
#define SPISB_MSGCTL_HDR_DWS_S			0
#define SPISB_MSGCTL_EXP_RDW_S			8
#define SPISB_MSGCTL_MSG_MODE_S			26
#define SPISB_MSGCTL_TOKEN_MODE_S		28
#define SPISB_MSGCTL_BARCLR_S			30
#define SPISB_MSGCTL_CMDV_S			31
#define SPISB_MSGCTL_CMDV_M			BIT(31)
#define SPISB_RSPCTL				0x000B7024
#define SPISB_RSPCTL_BAD_LENGTH_M		BIT(30)
#define SPISB_RSPCTL_NOT_SUCCESS_M		BIT(31)
#define SPISB_WHDR0				0x000B70F4
#define SPISB_WHDR0_DEST_SEL_S			12
#define SPISB_WHDR0_OPCODE_SEL_S		16
#define SPISB_WHDR0_TAG_S			24
#define SPISB_WHDR1				0x000B70F8
#define SPISB_WHDR2				0x000B70FC
#define SPISB_RDATA				0x000B7300
#define SPISB_WDATA				0x000B7100

/* Firmware Reset Count register */
#define GL_FWRESETCNT				0x00083100 /* Reset Source: POR */
#define GL_FWRESETCNT_FWRESETCNT_S		0
#define GL_FWRESETCNT_FWRESETCNT_M		MAKEMASK(0xFFFFFFFF, 0)

/* Admin Command Interface (ACI) registers */
#define PF_HIDA(_i)			(0x00085000 + ((_i) * 4))
#define PF_HIDA_2(_i)			(0x00085020 + ((_i) * 4))
#define PF_HIBA(_i)			(0x00084000 + ((_i) * 4))
#define PF_HICR				0x00082048

#define PF_HIDA_MAX_INDEX		15
#define PF_HIBA_MAX_INDEX		1023

#define PF_HICR_EN			BIT(0)
#define PF_HICR_C			BIT(1)
#define PF_HICR_SV			BIT(2)
#define PF_HICR_EV			BIT(3)

#define GL_HIDA(_i)			(0x00082000 + ((_i) * 4))
#define GL_HIDA_2(_i)			(0x00082020 + ((_i) * 4))
#define GL_HIBA(_i)			(0x00081000 + ((_i) * 4))
#define GL_HICR				0x00082040

#define GL_HIDA_MAX_INDEX		15
#define GL_HIBA_MAX_INDEX		1023

#define GL_HICR_C			BIT(1)
#define GL_HICR_SV			BIT(2)
#define GL_HICR_EV			BIT(3)

#define GL_HICR_EN			0x00082044

#define GL_HICR_EN_CHECK		BIT(0)

/* Admin Command Interface (ACI) defines */
/* Defines that help manage the driver vs FW API checks.
 */
#define IXGBE_FW_API_VER_BRANCH		0x00
#define IXGBE_FW_API_VER_MAJOR		0x01
#define IXGBE_FW_API_VER_MINOR		0x07
#define IXGBE_FW_API_VER_DIFF_ALLOWED	0x02

#define IXGBE_ACI_DESC_SIZE		32
#define IXGBE_ACI_DESC_SIZE_IN_DWORDS	IXGBE_ACI_DESC_SIZE / BYTES_PER_DWORD

#define IXGBE_ACI_MAX_BUFFER_SIZE		4096    /* Size in bytes */
#define IXGBE_ACI_DESC_COOKIE_L_DWORD_OFFSET	3
#define IXGBE_ACI_SEND_DELAY_TIME_MS		10
#define IXGBE_ACI_SEND_MAX_EXECUTE		3
/* [ms] timeout of waiting for sync response */
#define IXGBE_ACI_SYNC_RESPONSE_TIMEOUT		100000
/* [ms] timeout of waiting for async response */
#define IXGBE_ACI_ASYNC_RESPONSE_TIMEOUT	150000
/* [ms] timeout of waiting for resource release */
#define IXGBE_ACI_RELEASE_RES_TIMEOUT		10000

/* Timestamp spacing for Tools ACI: queue is active if spacing is within the range [LO..HI] */
#define IXGBE_TOOLS_ACI_ACTIVE_STAMP_SPACING_LO      0
#define IXGBE_TOOLS_ACI_ACTIVE_STAMP_SPACING_HI      200

/* Timestamp spacing for Tools ACI: queue is expired if spacing is outside the range [LO..HI] */
#define IXGBE_TOOLS_ACI_EXPIRED_STAMP_SPACING_LO     -5
#define IXGBE_TOOLS_ACI_EXPIRED_STAMP_SPACING_HI     205

/* FW defined boundary for a large buffer, 4k >= Large buffer > 512 bytes */
#define IXGBE_ACI_LG_BUF		512

/* Flags sub-structure
 * |0  |1  |2  |3  |4  |5  |6  |7  |8  |9  |10 |11 |12 |13 |14 |15 |
 * |DD |CMP|ERR|VFE| * *  RESERVED * * |LB |RD |VFC|BUF|SI |EI |FE |
 */

/* command flags and offsets */
#define IXGBE_ACI_FLAG_DD_S	0
#define IXGBE_ACI_FLAG_CMP_S	1
#define IXGBE_ACI_FLAG_ERR_S	2
#define IXGBE_ACI_FLAG_VFE_S	3
#define IXGBE_ACI_FLAG_LB_S	9
#define IXGBE_ACI_FLAG_RD_S	10
#define IXGBE_ACI_FLAG_VFC_S	11
#define IXGBE_ACI_FLAG_BUF_S	12
#define IXGBE_ACI_FLAG_SI_S	13
#define IXGBE_ACI_FLAG_EI_S	14
#define IXGBE_ACI_FLAG_FE_S	15

#define IXGBE_ACI_FLAG_DD		BIT(IXGBE_ACI_FLAG_DD_S)  /* 0x1    */
#define IXGBE_ACI_FLAG_CMP		BIT(IXGBE_ACI_FLAG_CMP_S) /* 0x2    */
#define IXGBE_ACI_FLAG_ERR		BIT(IXGBE_ACI_FLAG_ERR_S) /* 0x4    */
#define IXGBE_ACI_FLAG_VFE		BIT(IXGBE_ACI_FLAG_VFE_S) /* 0x8    */
#define IXGBE_ACI_FLAG_LB		BIT(IXGBE_ACI_FLAG_LB_S)  /* 0x200  */
#define IXGBE_ACI_FLAG_RD		BIT(IXGBE_ACI_FLAG_RD_S)  /* 0x400  */
#define IXGBE_ACI_FLAG_VFC		BIT(IXGBE_ACI_FLAG_VFC_S) /* 0x800  */
#define IXGBE_ACI_FLAG_BUF		BIT(IXGBE_ACI_FLAG_BUF_S) /* 0x1000 */
#define IXGBE_ACI_FLAG_SI		BIT(IXGBE_ACI_FLAG_SI_S)  /* 0x2000 */
#define IXGBE_ACI_FLAG_EI		BIT(IXGBE_ACI_FLAG_EI_S)  /* 0x4000 */
#define IXGBE_ACI_FLAG_FE		BIT(IXGBE_ACI_FLAG_FE_S)  /* 0x8000 */

/* Admin Command Interface (ACI) error codes */
enum ixgbe_aci_err {
	IXGBE_ACI_RC_OK			= 0,  /* Success */
	IXGBE_ACI_RC_EPERM		= 1,  /* Operation not permitted */
	IXGBE_ACI_RC_ENOENT		= 2,  /* No such element */
	IXGBE_ACI_RC_ESRCH		= 3,  /* Bad opcode */
	IXGBE_ACI_RC_EINTR		= 4,  /* Operation interrupted */
	IXGBE_ACI_RC_EIO		= 5,  /* I/O error */
	IXGBE_ACI_RC_ENXIO		= 6,  /* No such resource */
	IXGBE_ACI_RC_E2BIG		= 7,  /* Arg too long */
	IXGBE_ACI_RC_EAGAIN		= 8,  /* Try again */
	IXGBE_ACI_RC_ENOMEM		= 9,  /* Out of memory */
	IXGBE_ACI_RC_EACCES		= 10, /* Permission denied */
	IXGBE_ACI_RC_EFAULT		= 11, /* Bad address */
	IXGBE_ACI_RC_EBUSY		= 12, /* Device or resource busy */
	IXGBE_ACI_RC_EEXIST		= 13, /* Object already exists */
	IXGBE_ACI_RC_EINVAL		= 14, /* Invalid argument */
	IXGBE_ACI_RC_ENOTTY		= 15, /* Not a typewriter */
	IXGBE_ACI_RC_ENOSPC		= 16, /* No space left or allocation failure */
	IXGBE_ACI_RC_ENOSYS		= 17, /* Function not implemented */
	IXGBE_ACI_RC_ERANGE		= 18, /* Parameter out of range */
	IXGBE_ACI_RC_EFLUSHED		= 19, /* Cmd flushed due to prev cmd error */
	IXGBE_ACI_RC_BAD_ADDR		= 20, /* Descriptor contains a bad pointer */
	IXGBE_ACI_RC_EMODE		= 21, /* Op not allowed in current dev mode */
	IXGBE_ACI_RC_EFBIG		= 22, /* File too big */
	IXGBE_ACI_RC_ESBCOMP		= 23, /* SB-IOSF completion unsuccessful */
	IXGBE_ACI_RC_ENOSEC		= 24, /* Missing security manifest */
	IXGBE_ACI_RC_EBADSIG		= 25, /* Bad RSA signature */
	IXGBE_ACI_RC_ESVN		= 26, /* SVN number prohibits this package */
	IXGBE_ACI_RC_EBADMAN		= 27, /* Manifest hash mismatch */
	IXGBE_ACI_RC_EBADBUF		= 28, /* Buffer hash mismatches manifest */
	IXGBE_ACI_RC_EACCES_BMCU	= 29, /* BMC Update in progress */
};

/* Admin Command Interface (ACI) opcodes */
enum ixgbe_aci_opc {
	ixgbe_aci_opc_get_ver				= 0x0001,
	ixgbe_aci_opc_driver_ver			= 0x0002,
	ixgbe_aci_opc_get_exp_err			= 0x0005,

	/* resource ownership */
	ixgbe_aci_opc_req_res				= 0x0008,
	ixgbe_aci_opc_release_res			= 0x0009,

	/* device/function capabilities */
	ixgbe_aci_opc_list_func_caps			= 0x000A,
	ixgbe_aci_opc_list_dev_caps			= 0x000B,

	/* safe disable of RXEN */
	ixgbe_aci_opc_disable_rxen			= 0x000C,

	/* FW events */
	ixgbe_aci_opc_get_fw_event			= 0x0014,

	/* PHY commands */
	ixgbe_aci_opc_get_phy_caps			= 0x0600,
	ixgbe_aci_opc_set_phy_cfg			= 0x0601,
	ixgbe_aci_opc_restart_an			= 0x0605,
	ixgbe_aci_opc_get_link_status			= 0x0607,
	ixgbe_aci_opc_set_event_mask			= 0x0613,
	ixgbe_aci_opc_get_link_topo			= 0x06E0,
	ixgbe_aci_opc_read_i2c				= 0x06E2,
	ixgbe_aci_opc_write_i2c				= 0x06E3,
	ixgbe_aci_opc_read_mdio				= 0x06E4,
	ixgbe_aci_opc_write_mdio			= 0x06E5,
	ixgbe_aci_opc_set_gpio_by_func			= 0x06E6,
	ixgbe_aci_opc_get_gpio_by_func			= 0x06E7,
	ixgbe_aci_opc_set_port_id_led			= 0x06E9,
	ixgbe_aci_opc_set_gpio				= 0x06EC,
	ixgbe_aci_opc_get_gpio				= 0x06ED,
	ixgbe_aci_opc_sff_eeprom			= 0x06EE,
	ixgbe_aci_opc_prog_topo_dev_nvm			= 0x06F2,
	ixgbe_aci_opc_read_topo_dev_nvm			= 0x06F3,

	/* NVM commands */
	ixgbe_aci_opc_nvm_read				= 0x0701,
	ixgbe_aci_opc_nvm_erase				= 0x0702,
	ixgbe_aci_opc_nvm_write				= 0x0703,
	ixgbe_aci_opc_nvm_cfg_read			= 0x0704,
	ixgbe_aci_opc_nvm_cfg_write			= 0x0705,
	ixgbe_aci_opc_nvm_checksum			= 0x0706,
	ixgbe_aci_opc_nvm_write_activate		= 0x0707,
	ixgbe_aci_opc_nvm_sr_dump			= 0x0707,
	ixgbe_aci_opc_nvm_save_factory_settings		= 0x0708,
	ixgbe_aci_opc_nvm_update_empr			= 0x0709,
	ixgbe_aci_opc_nvm_pkg_data			= 0x070A,
	ixgbe_aci_opc_nvm_pass_component_tbl		= 0x070B,
	ixgbe_aci_opc_nvm_sanitization			= 0x070C,

	/* Alternate Structure Commands */
	ixgbe_aci_opc_write_alt_direct			= 0x0900,
	ixgbe_aci_opc_write_alt_indirect		= 0x0901,
	ixgbe_aci_opc_read_alt_direct			= 0x0902,
	ixgbe_aci_opc_read_alt_indirect			= 0x0903,
	ixgbe_aci_opc_done_alt_write			= 0x0904,
	ixgbe_aci_opc_clear_port_alt_write		= 0x0906,

	ixgbe_aci_opc_temp_tca_event			= 0x0C94,

	/* debug commands */
	ixgbe_aci_opc_debug_dump_internals		= 0xFF08,

	/* SystemDiagnostic commands */
	ixgbe_aci_opc_set_health_status_config		= 0xFF20,
	ixgbe_aci_opc_get_supported_health_status_codes	= 0xFF21,
	ixgbe_aci_opc_get_health_status			= 0xFF22,
	ixgbe_aci_opc_clear_health_status		= 0xFF23,

	/* FW Logging Commands */
	ixgbe_aci_opc_fw_logs_config			= 0xFF30,
	ixgbe_aci_opc_fw_logs_register			= 0xFF31,
	ixgbe_aci_opc_fw_logs_query			= 0xFF32,
	ixgbe_aci_opc_fw_logs_event			= 0xFF33,
	ixgbe_aci_opc_fw_logs_get			= 0xFF34,
	ixgbe_aci_opc_fw_logs_clear			= 0xFF35
};

/* This macro is used to generate a compilation error if a structure
 * is not exactly the correct length. It gives a divide by zero error if the
 * structure is not of the correct size, otherwise it creates an enum that is
 * never used.
 */
#define IXGBE_CHECK_STRUCT_LEN(n, X) enum ixgbe_static_assert_enum_##X \
	{ ixgbe_static_assert_##X = (n) / ((sizeof(struct X) == (n)) ? 1 : 0) }

/* This macro is used to generate a compilation error if a variable-length
 * structure is not exactly the correct length assuming a single element of
 * the variable-length object as the last element of the structure. It gives
 * a divide by zero error if the structure is not of the correct size,
 * otherwise it creates an enum that is never used.
 */
#define IXGBE_CHECK_VAR_LEN_STRUCT_LEN(n, X, T) enum ixgbe_static_assert_enum_##X \
	{ ixgbe_static_assert_##X = (n) / \
	  (((sizeof(struct X) + sizeof(T)) == (n)) ? 1 : 0) }

/* This macro is used to ensure that parameter structures (i.e. structures
 * in the params union member of struct ixgbe_aci_desc) are 16 bytes in length.
 *
 * NOT intended to be used to check the size of an indirect command/response
 * additional data buffer (e.g. struct foo) which should just happen to be 16
 * bytes (instead, use IXGBE_CHECK_STRUCT_LEN(16, foo) for that).
 */
#define IXGBE_CHECK_PARAM_LEN(X)	IXGBE_CHECK_STRUCT_LEN(16, X)

struct ixgbe_aci_cmd_generic {
	__le32 param0;
	__le32 param1;
	__le32 addr_high;
	__le32 addr_low;
};

IXGBE_CHECK_PARAM_LEN(ixgbe_aci_cmd_generic);

/* Get version (direct 0x0001) */
struct ixgbe_aci_cmd_get_ver {
	__le32 rom_ver;
	__le32 fw_build;
	u8 fw_branch;
	u8 fw_major;
	u8 fw_minor;
	u8 fw_patch;
	u8 api_branch;
	u8 api_major;
	u8 api_minor;
	u8 api_patch;
};

IXGBE_CHECK_PARAM_LEN(ixgbe_aci_cmd_get_ver);

#define IXGBE_DRV_VER_STR_LEN_E610	32

struct ixgbe_driver_ver {
	u8 major_ver;
	u8 minor_ver;
	u8 build_ver;
	u8 subbuild_ver;
	u8 driver_string[IXGBE_DRV_VER_STR_LEN_E610];
};

/* Send driver version (indirect 0x0002) */
struct ixgbe_aci_cmd_driver_ver {
	u8 major_ver;
	u8 minor_ver;
	u8 build_ver;
	u8 subbuild_ver;
	u8 reserved[4];
	__le32 addr_high;
	__le32 addr_low;
};

IXGBE_CHECK_PARAM_LEN(ixgbe_aci_cmd_driver_ver);

/* Get Expanded Error Code (0x0005, direct) */
struct ixgbe_aci_cmd_get_exp_err {
	__le32 reason;
#define IXGBE_ACI_EXPANDED_ERROR_NOT_PROVIDED	0xFFFFFFFF
	__le32 identifier;
	u8 rsvd[8];
};

IXGBE_CHECK_PARAM_LEN(ixgbe_aci_cmd_get_exp_err);

/* FW update timeout definitions are in milliseconds */
#define IXGBE_NVM_TIMEOUT		180000
#define IXGBE_CHANGE_LOCK_TIMEOUT	1000
#define IXGBE_GLOBAL_CFG_LOCK_TIMEOUT	3000

enum ixgbe_aci_res_access_type {
	IXGBE_RES_READ = 1,
	IXGBE_RES_WRITE
};

enum ixgbe_aci_res_ids {
	IXGBE_NVM_RES_ID = 1,
	IXGBE_SPD_RES_ID,
	IXGBE_CHANGE_LOCK_RES_ID,
	IXGBE_GLOBAL_CFG_LOCK_RES_ID
};

/* Request resource ownership (direct 0x0008)
 * Release resource ownership (direct 0x0009)
 */
struct ixgbe_aci_cmd_req_res {
	__le16 res_id;
#define IXGBE_ACI_RES_ID_NVM		1
#define IXGBE_ACI_RES_ID_SDP		2
#define IXGBE_ACI_RES_ID_CHNG_LOCK	3
#define IXGBE_ACI_RES_ID_GLBL_LOCK	4
	__le16 access_type;
#define IXGBE_ACI_RES_ACCESS_READ	1
#define IXGBE_ACI_RES_ACCESS_WRITE	2

	/* Upon successful completion, FW writes this value and driver is
	 * expected to release resource before timeout. This value is provided
	 * in milliseconds.
	 */
	__le32 timeout;
#define IXGBE_ACI_RES_NVM_READ_DFLT_TIMEOUT_MS	3000
#define IXGBE_ACI_RES_NVM_WRITE_DFLT_TIMEOUT_MS	180000
#define IXGBE_ACI_RES_CHNG_LOCK_DFLT_TIMEOUT_MS	1000
#define IXGBE_ACI_RES_GLBL_LOCK_DFLT_TIMEOUT_MS	3000
	/* For SDP: pin ID of the SDP */
	__le32 res_number;
	/* Status is only used for IXGBE_ACI_RES_ID_GLBL_LOCK */
	__le16 status;
#define IXGBE_ACI_RES_GLBL_SUCCESS		0
#define IXGBE_ACI_RES_GLBL_IN_PROG		1
#define IXGBE_ACI_RES_GLBL_DONE			2
	u8 reserved[2];
};

IXGBE_CHECK_PARAM_LEN(ixgbe_aci_cmd_req_res);

/* Get function capabilities (indirect 0x000A)
 * Get device capabilities (indirect 0x000B)
 */
struct ixgbe_aci_cmd_list_caps {
	u8 cmd_flags;
	u8 pf_index;
	u8 reserved[2];
	__le32 count;
	__le32 addr_high;
	__le32 addr_low;
};

IXGBE_CHECK_PARAM_LEN(ixgbe_aci_cmd_list_caps);

/* Device/Function buffer entry, repeated per reported capability */
struct ixgbe_aci_cmd_list_caps_elem {
	__le16 cap;
#define IXGBE_ACI_CAPS_VALID_FUNCTIONS			0x0005
#define IXGBE_ACI_MAX_VALID_FUNCTIONS			0x8
#define IXGBE_ACI_CAPS_SRIOV				0x0012
#define IXGBE_ACI_CAPS_VF				0x0013
#define IXGBE_ACI_CAPS_VMDQ				0x0014
#define IXGBE_ACI_CAPS_VSI				0x0017
#define IXGBE_ACI_CAPS_DCB				0x0018
#define IXGBE_ACI_CAPS_RSS				0x0040
#define IXGBE_ACI_CAPS_RXQS				0x0041
#define IXGBE_ACI_CAPS_TXQS				0x0042
#define IXGBE_ACI_CAPS_MSIX				0x0043
#define IXGBE_ACI_CAPS_FD				0x0045
#define IXGBE_ACI_CAPS_MAX_MTU				0x0047
#define IXGBE_ACI_CAPS_NVM_VER				0x0048
#define IXGBE_ACI_CAPS_INLINE_IPSEC			0x0070
#define IXGBE_ACI_CAPS_NUM_ENABLED_PORTS		0x0072
#define IXGBE_ACI_CAPS_PCIE_RESET_AVOIDANCE		0x0076
#define IXGBE_ACI_CAPS_POST_UPDATE_RESET_RESTRICT	0x0077
#define IXGBE_ACI_CAPS_NVM_MGMT				0x0080
#define IXGBE_ACI_CAPS_EXT_TOPO_DEV_IMG0		0x0081
#define IXGBE_ACI_CAPS_EXT_TOPO_DEV_IMG1		0x0082
#define IXGBE_ACI_CAPS_EXT_TOPO_DEV_IMG2		0x0083
#define IXGBE_ACI_CAPS_EXT_TOPO_DEV_IMG3		0x0084
#define IXGBE_ACI_CAPS_OROM_RECOVERY_UPDATE		0x0090
#define IXGBE_ACI_CAPS_NEXT_CLUSTER_ID			0x0096
	u8 major_ver;
	u8 minor_ver;
	/* Number of resources described by this capability */
	__le32 number;
	/* Only meaningful for some types of resources */
	__le32 logical_id;
	/* Only meaningful for some types of resources */
	__le32 phys_id;
	__le64 rsvd1;
	__le64 rsvd2;
};

IXGBE_CHECK_STRUCT_LEN(32, ixgbe_aci_cmd_list_caps_elem);

/* Disable RXEN (direct 0x000C) */
struct ixgbe_aci_cmd_disable_rxen {
	u8 lport_num;
	u8 reserved[15];
};

IXGBE_CHECK_PARAM_LEN(ixgbe_aci_cmd_disable_rxen);

/* Get FW Event (indirect 0x0014) */
struct ixgbe_aci_cmd_get_fw_event {
	__le16 fw_buf_status;
#define IXGBE_ACI_GET_FW_EVENT_STATUS_OBTAINED	BIT(0)
#define IXGBE_ACI_GET_FW_EVENT_STATUS_PENDING	BIT(1)
	u8 rsvd[14];
};

IXGBE_CHECK_PARAM_LEN(ixgbe_aci_cmd_get_fw_event);

/* Get PHY capabilities (indirect 0x0600) */
struct ixgbe_aci_cmd_get_phy_caps {
	u8 lport_num;
	u8 reserved;
	__le16 param0;
	/* 18.0 - Report qualified modules */
#define IXGBE_ACI_GET_PHY_RQM		BIT(0)
	/* 18.1 - 18.3 : Report mode
	 * 000b - Report topology capabilities, without media
	 * 001b - Report topology capabilities, with media
	 * 010b - Report Active configuration
	 * 011b - Report PHY Type and FEC mode capabilities
	 * 100b - Report Default capabilities
	 */
#define IXGBE_ACI_REPORT_MODE_S			1
#define IXGBE_ACI_REPORT_MODE_M			(7 << IXGBE_ACI_REPORT_MODE_S)
#define IXGBE_ACI_REPORT_TOPO_CAP_NO_MEDIA	0
#define IXGBE_ACI_REPORT_TOPO_CAP_MEDIA		BIT(1)
#define IXGBE_ACI_REPORT_ACTIVE_CFG		BIT(2)
#define IXGBE_ACI_REPORT_DFLT_CFG		BIT(3)
	__le32 reserved1;
	__le32 addr_high;
	__le32 addr_low;
};

IXGBE_CHECK_PARAM_LEN(ixgbe_aci_cmd_get_phy_caps);

/* This is #define of PHY type (Extended):
 * The first set of defines is for phy_type_low.
 */
#define IXGBE_PHY_TYPE_LOW_100BASE_TX		BIT_ULL(0)
#define IXGBE_PHY_TYPE_LOW_100M_SGMII		BIT_ULL(1)
#define IXGBE_PHY_TYPE_LOW_1000BASE_T		BIT_ULL(2)
#define IXGBE_PHY_TYPE_LOW_1000BASE_SX		BIT_ULL(3)
#define IXGBE_PHY_TYPE_LOW_1000BASE_LX		BIT_ULL(4)
#define IXGBE_PHY_TYPE_LOW_1000BASE_KX		BIT_ULL(5)
#define IXGBE_PHY_TYPE_LOW_1G_SGMII		BIT_ULL(6)
#define IXGBE_PHY_TYPE_LOW_2500BASE_T		BIT_ULL(7)
#define IXGBE_PHY_TYPE_LOW_2500BASE_X		BIT_ULL(8)
#define IXGBE_PHY_TYPE_LOW_2500BASE_KX		BIT_ULL(9)
#define IXGBE_PHY_TYPE_LOW_5GBASE_T		BIT_ULL(10)
#define IXGBE_PHY_TYPE_LOW_5GBASE_KR		BIT_ULL(11)
#define IXGBE_PHY_TYPE_LOW_10GBASE_T		BIT_ULL(12)
#define IXGBE_PHY_TYPE_LOW_10G_SFI_DA		BIT_ULL(13)
#define IXGBE_PHY_TYPE_LOW_10GBASE_SR		BIT_ULL(14)
#define IXGBE_PHY_TYPE_LOW_10GBASE_LR		BIT_ULL(15)
#define IXGBE_PHY_TYPE_LOW_10GBASE_KR_CR1	BIT_ULL(16)
#define IXGBE_PHY_TYPE_LOW_10G_SFI_AOC_ACC	BIT_ULL(17)
#define IXGBE_PHY_TYPE_LOW_10G_SFI_C2C		BIT_ULL(18)
#define IXGBE_PHY_TYPE_LOW_MAX_INDEX		18
/* The second set of defines is for phy_type_high. */
#define IXGBE_PHY_TYPE_HIGH_10BASE_T		BIT_ULL(1)
#define IXGBE_PHY_TYPE_HIGH_10M_SGMII		BIT_ULL(2)
#define IXGBE_PHY_TYPE_HIGH_2500M_SGMII		BIT_ULL(56)
#define IXGBE_PHY_TYPE_HIGH_100M_USXGMII	BIT_ULL(57)
#define IXGBE_PHY_TYPE_HIGH_1G_USXGMII		BIT_ULL(58)
#define IXGBE_PHY_TYPE_HIGH_2500M_USXGMII	BIT_ULL(59)
#define IXGBE_PHY_TYPE_HIGH_5G_USXGMII		BIT_ULL(60)
#define IXGBE_PHY_TYPE_HIGH_10G_USXGMII		BIT_ULL(61)
#define IXGBE_PHY_TYPE_HIGH_MAX_INDEX		61

struct ixgbe_aci_cmd_get_phy_caps_data {
	__le64 phy_type_low; /* Use values from IXGBE_PHY_TYPE_LOW_* */
	__le64 phy_type_high; /* Use values from IXGBE_PHY_TYPE_HIGH_* */
	u8 caps;
#define IXGBE_ACI_PHY_EN_TX_LINK_PAUSE			BIT(0)
#define IXGBE_ACI_PHY_EN_RX_LINK_PAUSE			BIT(1)
#define IXGBE_ACI_PHY_LOW_POWER_MODE			BIT(2)
#define IXGBE_ACI_PHY_EN_LINK				BIT(3)
#define IXGBE_ACI_PHY_AN_MODE				BIT(4)
#define IXGBE_ACI_PHY_EN_MOD_QUAL			BIT(5)
#define IXGBE_ACI_PHY_EN_LESM				BIT(6)
#define IXGBE_ACI_PHY_EN_AUTO_FEC			BIT(7)
#define IXGBE_ACI_PHY_CAPS_MASK				MAKEMASK(0xff, 0)
	u8 low_power_ctrl_an;
#define IXGBE_ACI_PHY_EN_D3COLD_LOW_POWER_AUTONEG	BIT(0)
#define IXGBE_ACI_PHY_AN_EN_CLAUSE28			BIT(1)
#define IXGBE_ACI_PHY_AN_EN_CLAUSE73			BIT(2)
#define IXGBE_ACI_PHY_AN_EN_CLAUSE37			BIT(3)
	__le16 eee_cap;
#define IXGBE_ACI_PHY_EEE_EN_100BASE_TX			BIT(0)
#define IXGBE_ACI_PHY_EEE_EN_1000BASE_T			BIT(1)
#define IXGBE_ACI_PHY_EEE_EN_10GBASE_T			BIT(2)
#define IXGBE_ACI_PHY_EEE_EN_1000BASE_KX		BIT(3)
#define IXGBE_ACI_PHY_EEE_EN_10GBASE_KR			BIT(4)
#define IXGBE_ACI_PHY_EEE_EN_25GBASE_KR			BIT(5)
#define IXGBE_ACI_PHY_EEE_EN_10BASE_T			BIT(11)
	__le16 eeer_value;
	u8 phy_id_oui[4]; /* PHY/Module ID connected on the port */
	u8 phy_fw_ver[8];
	u8 link_fec_options;
#define IXGBE_ACI_PHY_FEC_10G_KR_40G_KR4_EN		BIT(0)
#define IXGBE_ACI_PHY_FEC_10G_KR_40G_KR4_REQ		BIT(1)
#define IXGBE_ACI_PHY_FEC_25G_RS_528_REQ		BIT(2)
#define IXGBE_ACI_PHY_FEC_25G_KR_REQ			BIT(3)
#define IXGBE_ACI_PHY_FEC_25G_RS_544_REQ		BIT(4)
#define IXGBE_ACI_PHY_FEC_25G_RS_CLAUSE91_EN		BIT(6)
#define IXGBE_ACI_PHY_FEC_25G_KR_CLAUSE74_EN		BIT(7)
#define IXGBE_ACI_PHY_FEC_MASK				MAKEMASK(0xdf, 0)
	u8 module_compliance_enforcement;
#define IXGBE_ACI_MOD_ENFORCE_STRICT_MODE		BIT(0)
	u8 extended_compliance_code;
#define IXGBE_ACI_MODULE_TYPE_TOTAL_BYTE		3
	u8 module_type[IXGBE_ACI_MODULE_TYPE_TOTAL_BYTE];
#define IXGBE_ACI_MOD_TYPE_BYTE0_SFP_PLUS		0xA0
#define IXGBE_ACI_MOD_TYPE_BYTE0_QSFP_PLUS		0x80
#define IXGBE_ACI_MOD_TYPE_IDENT			1
#define IXGBE_ACI_MOD_TYPE_BYTE1_SFP_PLUS_CU_PASSIVE	BIT(0)
#define IXGBE_ACI_MOD_TYPE_BYTE1_SFP_PLUS_CU_ACTIVE	BIT(1)
#define IXGBE_ACI_MOD_TYPE_BYTE1_10G_BASE_SR		BIT(4)
#define IXGBE_ACI_MOD_TYPE_BYTE1_10G_BASE_LR		BIT(5)
#define IXGBE_ACI_MOD_TYPE_BYTE1_10G_BASE_LRM		BIT(6)
#define IXGBE_ACI_MOD_TYPE_BYTE1_10G_BASE_ER		BIT(7)
#define IXGBE_ACI_MOD_TYPE_BYTE2_SFP_PLUS		0xA0
#define IXGBE_ACI_MOD_TYPE_BYTE2_QSFP_PLUS		0x86
	u8 qualified_module_count;
	u8 rsvd2[7];	/* Bytes 47:41 reserved */
#define IXGBE_ACI_QUAL_MOD_COUNT_MAX			16
	struct {
		u8 v_oui[3];
		u8 rsvd3;
		u8 v_part[16];
		__le32 v_rev;
		__le64 rsvd4;
	} qual_modules[IXGBE_ACI_QUAL_MOD_COUNT_MAX];
};

IXGBE_CHECK_STRUCT_LEN(560, ixgbe_aci_cmd_get_phy_caps_data);

/* Set PHY capabilities (direct 0x0601)
 * NOTE: This command must be followed by setup link and restart auto-neg
 */
struct ixgbe_aci_cmd_set_phy_cfg {
	u8 reserved[8];
	__le32 addr_high;
	__le32 addr_low;
};

IXGBE_CHECK_PARAM_LEN(ixgbe_aci_cmd_set_phy_cfg);

/* Set PHY config command data structure */
struct ixgbe_aci_cmd_set_phy_cfg_data {
	__le64 phy_type_low; /* Use values from IXGBE_PHY_TYPE_LOW_* */
	__le64 phy_type_high; /* Use values from IXGBE_PHY_TYPE_HIGH_* */
	u8 caps;
#define IXGBE_ACI_PHY_ENA_VALID_MASK		MAKEMASK(0xef, 0)
#define IXGBE_ACI_PHY_ENA_TX_PAUSE_ABILITY	BIT(0)
#define IXGBE_ACI_PHY_ENA_RX_PAUSE_ABILITY	BIT(1)
#define IXGBE_ACI_PHY_ENA_LOW_POWER		BIT(2)
#define IXGBE_ACI_PHY_ENA_LINK			BIT(3)
#define IXGBE_ACI_PHY_ENA_AUTO_LINK_UPDT	BIT(5)
#define IXGBE_ACI_PHY_ENA_LESM			BIT(6)
#define IXGBE_ACI_PHY_ENA_AUTO_FEC		BIT(7)
	u8 low_power_ctrl_an;
	__le16 eee_cap; /* Value from ixgbe_aci_get_phy_caps */
	__le16 eeer_value; /* Use defines from ixgbe_aci_get_phy_caps */
	u8 link_fec_opt; /* Use defines from ixgbe_aci_get_phy_caps */
	u8 module_compliance_enforcement;
};

IXGBE_CHECK_STRUCT_LEN(24, ixgbe_aci_cmd_set_phy_cfg_data);

/* Restart AN command data structure (direct 0x0605)
 * Also used for response, with only the lport_num field present.
 */
struct ixgbe_aci_cmd_restart_an {
	u8 reserved[2];
	u8 cmd_flags;
#define IXGBE_ACI_RESTART_AN_LINK_RESTART	BIT(1)
#define IXGBE_ACI_RESTART_AN_LINK_ENABLE	BIT(2)
	u8 reserved2[13];
};

IXGBE_CHECK_PARAM_LEN(ixgbe_aci_cmd_restart_an);

#pragma pack(1)
/* Get link status (indirect 0x0607), also used for Link Status Event */
struct ixgbe_aci_cmd_get_link_status {
	u8 reserved[2];
	u8 cmd_flags;
#define IXGBE_ACI_LSE_M				0x3
#define IXGBE_ACI_LSE_NOP			0x0
#define IXGBE_ACI_LSE_DIS			0x2
#define IXGBE_ACI_LSE_ENA			0x3
	/* only response uses this flag */
#define IXGBE_ACI_LSE_IS_ENABLED		0x1
	u8 reserved2[5];
	__le32 addr_high;
	__le32 addr_low;
};

IXGBE_CHECK_PARAM_LEN(ixgbe_aci_cmd_get_link_status);

/* Get link status response data structure, also used for Link Status Event */
struct ixgbe_aci_cmd_get_link_status_data {
	u8 topo_media_conflict;
#define IXGBE_ACI_LINK_TOPO_CONFLICT		BIT(0)
#define IXGBE_ACI_LINK_MEDIA_CONFLICT		BIT(1)
#define IXGBE_ACI_LINK_TOPO_CORRUPT		BIT(2)
#define IXGBE_ACI_LINK_TOPO_UNREACH_PRT		BIT(4)
#define IXGBE_ACI_LINK_TOPO_UNDRUTIL_PRT	BIT(5)
#define IXGBE_ACI_LINK_TOPO_UNDRUTIL_MEDIA	BIT(6)
#define IXGBE_ACI_LINK_TOPO_UNSUPP_MEDIA	BIT(7)
	u8 link_cfg_err;
#define IXGBE_ACI_LINK_CFG_ERR				BIT(0)
#define IXGBE_ACI_LINK_CFG_COMPLETED			BIT(1)
#define IXGBE_ACI_LINK_ACT_PORT_OPT_INVAL		BIT(2)
#define IXGBE_ACI_LINK_FEAT_ID_OR_CONFIG_ID_INVAL	BIT(3)
#define IXGBE_ACI_LINK_TOPO_CRITICAL_SDP_ERR		BIT(4)
#define IXGBE_ACI_LINK_MODULE_POWER_UNSUPPORTED		BIT(5)
#define IXGBE_ACI_LINK_EXTERNAL_PHY_LOAD_FAILURE	BIT(6)
#define IXGBE_ACI_LINK_INVAL_MAX_POWER_LIMIT		BIT(7)
	u8 link_info;
#define IXGBE_ACI_LINK_UP		BIT(0)	/* Link Status */
#define IXGBE_ACI_LINK_FAULT		BIT(1)
#define IXGBE_ACI_LINK_FAULT_TX		BIT(2)
#define IXGBE_ACI_LINK_FAULT_RX		BIT(3)
#define IXGBE_ACI_LINK_FAULT_REMOTE	BIT(4)
#define IXGBE_ACI_LINK_UP_PORT		BIT(5)	/* External Port Link Status */
#define IXGBE_ACI_MEDIA_AVAILABLE	BIT(6)
#define IXGBE_ACI_SIGNAL_DETECT		BIT(7)
	u8 an_info;
#define IXGBE_ACI_AN_COMPLETED		BIT(0)
#define IXGBE_ACI_LP_AN_ABILITY		BIT(1)
#define IXGBE_ACI_PD_FAULT		BIT(2)	/* Parallel Detection Fault */
#define IXGBE_ACI_FEC_EN		BIT(3)
#define IXGBE_ACI_PHY_LOW_POWER		BIT(4)	/* Low Power State */
#define IXGBE_ACI_LINK_PAUSE_TX		BIT(5)
#define IXGBE_ACI_LINK_PAUSE_RX		BIT(6)
#define IXGBE_ACI_QUALIFIED_MODULE	BIT(7)
	u8 ext_info;
#define IXGBE_ACI_LINK_PHY_TEMP_ALARM	BIT(0)
#define IXGBE_ACI_LINK_EXCESSIVE_ERRORS	BIT(1)	/* Excessive Link Errors */
	/* Port Tx Suspended */
#define IXGBE_ACI_LINK_TX_S		2
#define IXGBE_ACI_LINK_TX_M		(0x03 << IXGBE_ACI_LINK_TX_S)
#define IXGBE_ACI_LINK_TX_ACTIVE	0
#define IXGBE_ACI_LINK_TX_DRAINED	1
#define IXGBE_ACI_LINK_TX_FLUSHED	3
	u8 lb_status;
#define IXGBE_ACI_LINK_LB_PHY_LCL	BIT(0)
#define IXGBE_ACI_LINK_LB_PHY_RMT	BIT(1)
#define IXGBE_ACI_LINK_LB_MAC_LCL	BIT(2)
#define IXGBE_ACI_LINK_LB_PHY_IDX_S	3
#define IXGBE_ACI_LINK_LB_PHY_IDX_M	(0x7 << IXGBE_ACI_LB_PHY_IDX_S)
	__le16 max_frame_size;
	u8 cfg;
#define IXGBE_ACI_LINK_25G_KR_FEC_EN		BIT(0)
#define IXGBE_ACI_LINK_25G_RS_528_FEC_EN	BIT(1)
#define IXGBE_ACI_LINK_25G_RS_544_FEC_EN	BIT(2)
#define IXGBE_ACI_FEC_MASK			MAKEMASK(0x7, 0)
	/* Pacing Config */
#define IXGBE_ACI_CFG_PACING_S		3
#define IXGBE_ACI_CFG_PACING_M		(0xF << IXGBE_ACI_CFG_PACING_S)
#define IXGBE_ACI_CFG_PACING_TYPE_M	BIT(7)
#define IXGBE_ACI_CFG_PACING_TYPE_AVG	0
#define IXGBE_ACI_CFG_PACING_TYPE_FIXED	IXGBE_ACI_CFG_PACING_TYPE_M
	/* External Device Power Ability */
	u8 power_desc;
#define IXGBE_ACI_PWR_CLASS_M			0x3F
#define IXGBE_ACI_LINK_PWR_BASET_LOW_HIGH	0
#define IXGBE_ACI_LINK_PWR_BASET_HIGH		1
#define IXGBE_ACI_LINK_PWR_QSFP_CLASS_1		0
#define IXGBE_ACI_LINK_PWR_QSFP_CLASS_2		1
#define IXGBE_ACI_LINK_PWR_QSFP_CLASS_3		2
#define IXGBE_ACI_LINK_PWR_QSFP_CLASS_4		3
	__le16 link_speed;
#define IXGBE_ACI_LINK_SPEED_M			0x7FF
#define IXGBE_ACI_LINK_SPEED_10MB		BIT(0)
#define IXGBE_ACI_LINK_SPEED_100MB		BIT(1)
#define IXGBE_ACI_LINK_SPEED_1000MB		BIT(2)
#define IXGBE_ACI_LINK_SPEED_2500MB		BIT(3)
#define IXGBE_ACI_LINK_SPEED_5GB		BIT(4)
#define IXGBE_ACI_LINK_SPEED_10GB		BIT(5)
#define IXGBE_ACI_LINK_SPEED_20GB		BIT(6)
#define IXGBE_ACI_LINK_SPEED_25GB		BIT(7)
#define IXGBE_ACI_LINK_SPEED_40GB		BIT(8)
#define IXGBE_ACI_LINK_SPEED_50GB		BIT(9)
#define IXGBE_ACI_LINK_SPEED_100GB		BIT(10)
#define IXGBE_ACI_LINK_SPEED_200GB		BIT(11)
#define IXGBE_ACI_LINK_SPEED_UNKNOWN		BIT(15)
	__le16 reserved3; /* Aligns next field to 8-byte boundary */
	u8 ext_fec_status;
#define IXGBE_ACI_LINK_RS_272_FEC_EN	BIT(0) /* RS 272 FEC enabled */
	u8 reserved4;
	__le64 phy_type_low; /* Use values from IXGBE_PHY_TYPE_LOW_* */
	__le64 phy_type_high; /* Use values from IXGBE_PHY_TYPE_HIGH_* */
	/* Get link status version 2 link partner data */
	__le64 lp_phy_type_low; /* Use values from IXGBE_PHY_TYPE_LOW_* */
	__le64 lp_phy_type_high; /* Use values from IXGBE_PHY_TYPE_HIGH_* */
	u8 lp_fec_adv;
#define IXGBE_ACI_LINK_LP_10G_KR_FEC_CAP	BIT(0)
#define IXGBE_ACI_LINK_LP_25G_KR_FEC_CAP	BIT(1)
#define IXGBE_ACI_LINK_LP_RS_528_FEC_CAP	BIT(2)
#define IXGBE_ACI_LINK_LP_50G_KR_272_FEC_CAP	BIT(3)
#define IXGBE_ACI_LINK_LP_100G_KR_272_FEC_CAP	BIT(4)
#define IXGBE_ACI_LINK_LP_200G_KR_272_FEC_CAP	BIT(5)
	u8 lp_fec_req;
#define IXGBE_ACI_LINK_LP_10G_KR_FEC_REQ	BIT(0)
#define IXGBE_ACI_LINK_LP_25G_KR_FEC_REQ	BIT(1)
#define IXGBE_ACI_LINK_LP_RS_528_FEC_REQ	BIT(2)
#define IXGBE_ACI_LINK_LP_KR_272_FEC_REQ	BIT(3)
	u8 lp_flowcontrol;
#define IXGBE_ACI_LINK_LP_PAUSE_ADV		BIT(0)
#define IXGBE_ACI_LINK_LP_ASM_DIR_ADV		BIT(1)
	u8 reserved5[5];
};
#pragma pack()

IXGBE_CHECK_STRUCT_LEN(56, ixgbe_aci_cmd_get_link_status_data);

/* Set event mask command (direct 0x0613) */
struct ixgbe_aci_cmd_set_event_mask {
	u8	reserved[8];
	__le16	event_mask;
#define IXGBE_ACI_LINK_EVENT_UPDOWN		BIT(1)
#define IXGBE_ACI_LINK_EVENT_MEDIA_NA		BIT(2)
#define IXGBE_ACI_LINK_EVENT_LINK_FAULT		BIT(3)
#define IXGBE_ACI_LINK_EVENT_PHY_TEMP_ALARM	BIT(4)
#define IXGBE_ACI_LINK_EVENT_EXCESSIVE_ERRORS	BIT(5)
#define IXGBE_ACI_LINK_EVENT_SIGNAL_DETECT	BIT(6)
#define IXGBE_ACI_LINK_EVENT_AN_COMPLETED	BIT(7)
#define IXGBE_ACI_LINK_EVENT_MODULE_QUAL_FAIL	BIT(8)
#define IXGBE_ACI_LINK_EVENT_PORT_TX_SUSPENDED	BIT(9)
#define IXGBE_ACI_LINK_EVENT_TOPO_CONFLICT	BIT(10)
#define IXGBE_ACI_LINK_EVENT_MEDIA_CONFLICT	BIT(11)
#define IXGBE_ACI_LINK_EVENT_PHY_FW_LOAD_FAIL	BIT(12)
	u8	reserved1[6];
};

IXGBE_CHECK_PARAM_LEN(ixgbe_aci_cmd_set_event_mask);

struct ixgbe_aci_cmd_link_topo_params {
	u8 lport_num;
	u8 lport_num_valid;
#define IXGBE_ACI_LINK_TOPO_PORT_NUM_VALID	BIT(0)
	u8 node_type_ctx;
#define IXGBE_ACI_LINK_TOPO_NODE_TYPE_S		0
#define IXGBE_ACI_LINK_TOPO_NODE_TYPE_M		(0xF << IXGBE_ACI_LINK_TOPO_NODE_TYPE_S)
#define IXGBE_ACI_LINK_TOPO_NODE_TYPE_PHY	0
#define IXGBE_ACI_LINK_TOPO_NODE_TYPE_GPIO_CTRL	1
#define IXGBE_ACI_LINK_TOPO_NODE_TYPE_MUX_CTRL	2
#define IXGBE_ACI_LINK_TOPO_NODE_TYPE_LED_CTRL	3
#define IXGBE_ACI_LINK_TOPO_NODE_TYPE_LED	4
#define IXGBE_ACI_LINK_TOPO_NODE_TYPE_THERMAL	5
#define IXGBE_ACI_LINK_TOPO_NODE_TYPE_CAGE	6
#define IXGBE_ACI_LINK_TOPO_NODE_TYPE_MEZZ	7
#define IXGBE_ACI_LINK_TOPO_NODE_TYPE_ID_EEPROM	8
#define IXGBE_ACI_LINK_TOPO_NODE_TYPE_GPS	11
#define IXGBE_ACI_LINK_TOPO_NODE_CTX_S		4
#define IXGBE_ACI_LINK_TOPO_NODE_CTX_M		\
				(0xF << IXGBE_ACI_LINK_TOPO_NODE_CTX_S)
#define IXGBE_ACI_LINK_TOPO_NODE_CTX_GLOBAL			0
#define IXGBE_ACI_LINK_TOPO_NODE_CTX_BOARD			1
#define IXGBE_ACI_LINK_TOPO_NODE_CTX_PORT			2
#define IXGBE_ACI_LINK_TOPO_NODE_CTX_NODE			3
#define IXGBE_ACI_LINK_TOPO_NODE_CTX_NODE_HANDLE		4
#define IXGBE_ACI_LINK_TOPO_NODE_CTX_DIRECT_BUS_ACCESS		5
#define IXGBE_ACI_LINK_TOPO_NODE_CTX_NODE_HANDLE_BUS_ADDRESS	6
	u8 index;
};

IXGBE_CHECK_STRUCT_LEN(4, ixgbe_aci_cmd_link_topo_params);

struct ixgbe_aci_cmd_link_topo_addr {
	struct ixgbe_aci_cmd_link_topo_params topo_params;
	__le16 handle;
#define IXGBE_ACI_LINK_TOPO_HANDLE_S	0
#define IXGBE_ACI_LINK_TOPO_HANDLE_M	(0x3FF << IXGBE_ACI_LINK_TOPO_HANDLE_S)
/* Used to decode the handle field */
#define IXGBE_ACI_LINK_TOPO_HANDLE_BRD_TYPE_M		BIT(9)
#define IXGBE_ACI_LINK_TOPO_HANDLE_BRD_TYPE_LOM		BIT(9)
#define IXGBE_ACI_LINK_TOPO_HANDLE_BRD_TYPE_MEZZ	0
#define IXGBE_ACI_LINK_TOPO_HANDLE_NODE_S		0
/* In case of a Mezzanine type */
#define IXGBE_ACI_LINK_TOPO_HANDLE_MEZZ_NODE_M	\
				(0x3F << IXGBE_ACI_LINK_TOPO_HANDLE_NODE_S)
#define IXGBE_ACI_LINK_TOPO_HANDLE_MEZZ_S	6
#define IXGBE_ACI_LINK_TOPO_HANDLE_MEZZ_M	\
				(0x7 << IXGBE_ACI_LINK_TOPO_HANDLE_MEZZ_S)
/* In case of a LOM type */
#define IXGBE_ACI_LINK_TOPO_HANDLE_LOM_NODE_M	\
				(0x1FF << IXGBE_ACI_LINK_TOPO_HANDLE_NODE_S)
};

IXGBE_CHECK_STRUCT_LEN(6, ixgbe_aci_cmd_link_topo_addr);

/* Get Link Topology Handle (direct, 0x06E0) */
struct ixgbe_aci_cmd_get_link_topo {
	struct ixgbe_aci_cmd_link_topo_addr addr;
	u8 node_part_num;
#define IXGBE_ACI_GET_LINK_TOPO_NODE_NR_PCA9575		0x21
#define IXGBE_ACI_GET_LINK_TOPO_NODE_NR_GEN_GPS		0x48
#define IXGBE_ACI_GET_LINK_TOPO_NODE_NR_E610_PTC	0x49
	u8 rsvd[9];
};

IXGBE_CHECK_PARAM_LEN(ixgbe_aci_cmd_get_link_topo);

/* Read/Write I2C (direct, 0x06E2/0x06E3) */
struct ixgbe_aci_cmd_i2c {
	struct ixgbe_aci_cmd_link_topo_addr topo_addr;
	__le16 i2c_addr;
	u8 i2c_params;
#define IXGBE_ACI_I2C_DATA_SIZE_S		0
#define IXGBE_ACI_I2C_DATA_SIZE_M		(0xF << IXGBE_ACI_I2C_DATA_SIZE_S)
#define IXGBE_ACI_I2C_ADDR_TYPE_M		BIT(4)
#define IXGBE_ACI_I2C_ADDR_TYPE_7BIT		0
#define IXGBE_ACI_I2C_ADDR_TYPE_10BIT		IXGBE_ACI_I2C_ADDR_TYPE_M
#define IXGBE_ACI_I2C_DATA_OFFSET_S		5
#define IXGBE_ACI_I2C_DATA_OFFSET_M		(0x3 << IXGBE_ACI_I2C_DATA_OFFSET_S)
#define IXGBE_ACI_I2C_USE_REPEATED_START	BIT(7)
	u8 rsvd;
	__le16 i2c_bus_addr;
#define IXGBE_ACI_I2C_ADDR_7BIT_MASK		0x7F
#define IXGBE_ACI_I2C_ADDR_10BIT_MASK		0x3FF
	u8 i2c_data[4]; /* Used only by write command, reserved in read. */
};

IXGBE_CHECK_PARAM_LEN(ixgbe_aci_cmd_i2c);

/* Read I2C Response (direct, 0x06E2) */
struct ixgbe_aci_cmd_read_i2c_resp {
	u8 i2c_data[16];
};

IXGBE_CHECK_PARAM_LEN(ixgbe_aci_cmd_read_i2c_resp);

/* Read/Write MDIO (direct, 0x06E4/0x06E5) */
struct ixgbe_aci_cmd_mdio {
	struct ixgbe_aci_cmd_link_topo_addr topo_addr;
	u8 mdio_device_addr;
#define IXGBE_ACI_MDIO_DEV_S		0
#define IXGBE_ACI_MDIO_DEV_M		(0x1F << IXGBE_ACI_MDIO_DEV_S)
#define IXGBE_ACI_MDIO_CLAUSE_22	BIT(5)
#define IXGBE_ACI_MDIO_CLAUSE_45	BIT(6)
	u8 mdio_bus_address;
#define IXGBE_ACI_MDIO_BUS_ADDR_S 0
#define IXGBE_ACI_MDIO_BUS_ADDR_M (0x1F << IXGBE_ACI_MDIO_BUS_ADDR_S)
	__le16 offset;
	__le16 data; /* Input in write cmd, output in read cmd. */
	u8 rsvd1[4];
};

IXGBE_CHECK_PARAM_LEN(ixgbe_aci_cmd_mdio);

/* Set/Get GPIO By Function (direct, 0x06E6/0x06E7) */
struct ixgbe_aci_cmd_gpio_by_func {
	struct ixgbe_aci_cmd_link_topo_addr topo_addr;
	u8 io_func_num;
#define IXGBE_ACI_GPIO_FUNC_S	0
#define IXGBE_ACI_GPIO_FUNC_M	(0x1F << IXGBE_ACI_GPIO_IO_FUNC_NUM_S)
	u8 io_value; /* Input in write cmd, output in read cmd. */
#define IXGBE_ACI_GPIO_ON	BIT(0)
#define IXGBE_ACI_GPIO_OFF	0
	u8 rsvd[8];
};

IXGBE_CHECK_PARAM_LEN(ixgbe_aci_cmd_gpio_by_func);

/* Set Port Identification LED (direct, 0x06E9) */
struct ixgbe_aci_cmd_set_port_id_led {
	u8 lport_num;
	u8 lport_num_valid;
#define IXGBE_ACI_PORT_ID_PORT_NUM_VALID	BIT(0)
	u8 ident_mode;
#define IXGBE_ACI_PORT_IDENT_LED_BLINK		BIT(0)
#define IXGBE_ACI_PORT_IDENT_LED_ORIG		0
	u8 rsvd[13];
};

IXGBE_CHECK_PARAM_LEN(ixgbe_aci_cmd_set_port_id_led);

/* Set/Get GPIO (direct, 0x06EC/0x06ED) */
struct ixgbe_aci_cmd_gpio {
	__le16 gpio_ctrl_handle;
#define IXGBE_ACI_GPIO_HANDLE_S	0
#define IXGBE_ACI_GPIO_HANDLE_M	(0x3FF << IXGBE_ACI_GPIO_HANDLE_S)
	u8 gpio_num;
	u8 gpio_val;
	u8 rsvd[12];
};

IXGBE_CHECK_PARAM_LEN(ixgbe_aci_cmd_gpio);

/* Read/Write SFF EEPROM command (indirect 0x06EE) */
struct ixgbe_aci_cmd_sff_eeprom {
	u8 lport_num;
	u8 lport_num_valid;
#define IXGBE_ACI_SFF_PORT_NUM_VALID		BIT(0)
	__le16 i2c_bus_addr;
#define IXGBE_ACI_SFF_I2CBUS_7BIT_M		0x7F
#define IXGBE_ACI_SFF_I2CBUS_10BIT_M		0x3FF
#define IXGBE_ACI_SFF_I2CBUS_TYPE_M		BIT(10)
#define IXGBE_ACI_SFF_I2CBUS_TYPE_7BIT		0
#define IXGBE_ACI_SFF_I2CBUS_TYPE_10BIT		IXGBE_ACI_SFF_I2CBUS_TYPE_M
#define IXGBE_ACI_SFF_PAGE_BANK_CTRL_S		11
#define IXGBE_ACI_SFF_PAGE_BANK_CTRL_M		(0x3 << IXGBE_ACI_SFF_PAGE_BANK_CTRL_S)
#define IXGBE_ACI_SFF_NO_PAGE_BANK_UPDATE	0
#define IXGBE_ACI_SFF_UPDATE_PAGE		1
#define IXGBE_ACI_SFF_UPDATE_BANK		2
#define IXGBE_ACI_SFF_UPDATE_PAGE_BANK		3
#define IXGBE_ACI_SFF_IS_WRITE			BIT(15)
	__le16 i2c_offset;
	u8 module_bank;
	u8 module_page;
	__le32 addr_high;
	__le32 addr_low;
};

IXGBE_CHECK_PARAM_LEN(ixgbe_aci_cmd_sff_eeprom);

/* Program Topology Device NVM (direct, 0x06F2) */
struct ixgbe_aci_cmd_prog_topo_dev_nvm {
	struct ixgbe_aci_cmd_link_topo_params topo_params;
	u8 rsvd[12];
};

IXGBE_CHECK_PARAM_LEN(ixgbe_aci_cmd_prog_topo_dev_nvm);

/* Read Topology Device NVM (direct, 0x06F3) */
struct ixgbe_aci_cmd_read_topo_dev_nvm {
	struct ixgbe_aci_cmd_link_topo_params topo_params;
	__le32 start_address;
#define IXGBE_ACI_READ_TOPO_DEV_NVM_DATA_READ_SIZE 8
	u8 data_read[IXGBE_ACI_READ_TOPO_DEV_NVM_DATA_READ_SIZE];
};

IXGBE_CHECK_PARAM_LEN(ixgbe_aci_cmd_read_topo_dev_nvm);

/* NVM Read command (indirect 0x0701)
 * NVM Erase commands (direct 0x0702)
 * NVM Write commands (indirect 0x0703)
 * NVM Write Activate commands (direct 0x0707)
 * NVM Shadow RAM Dump commands (direct 0x0707)
 */
struct ixgbe_aci_cmd_nvm {
#define IXGBE_ACI_NVM_MAX_OFFSET	0xFFFFFF
	__le16 offset_low;
	u8 offset_high; /* For Write Activate offset_high is used as flags2 */
	u8 cmd_flags;
#define IXGBE_ACI_NVM_LAST_CMD		BIT(0)
#define IXGBE_ACI_NVM_PCIR_REQ		BIT(0)	/* Used by NVM Write reply */
#define IXGBE_ACI_NVM_PRESERVATION_S	1 /* Used by NVM Write Activate only */
#define IXGBE_ACI_NVM_PRESERVATION_M	(3 << IXGBE_ACI_NVM_PRESERVATION_S)
#define IXGBE_ACI_NVM_NO_PRESERVATION	(0 << IXGBE_ACI_NVM_PRESERVATION_S)
#define IXGBE_ACI_NVM_PRESERVE_ALL	BIT(1)
#define IXGBE_ACI_NVM_FACTORY_DEFAULT	(2 << IXGBE_ACI_NVM_PRESERVATION_S)
#define IXGBE_ACI_NVM_PRESERVE_SELECTED	(3 << IXGBE_ACI_NVM_PRESERVATION_S)
#define IXGBE_ACI_NVM_ACTIV_SEL_NVM	BIT(3) /* Write Activate/SR Dump only */
#define IXGBE_ACI_NVM_ACTIV_SEL_OROM	BIT(4)
#define IXGBE_ACI_NVM_ACTIV_SEL_NETLIST	BIT(5)
#define IXGBE_ACI_NVM_SPECIAL_UPDATE	BIT(6)
#define IXGBE_ACI_NVM_REVERT_LAST_ACTIV	BIT(6) /* Write Activate only */
#define IXGBE_ACI_NVM_ACTIV_SEL_MASK	MAKEMASK(0x7, 3)
#define IXGBE_ACI_NVM_FLASH_ONLY		BIT(7)
#define IXGBE_ACI_NVM_RESET_LVL_M		MAKEMASK(0x3, 0) /* Write reply only */
#define IXGBE_ACI_NVM_POR_FLAG		0
#define IXGBE_ACI_NVM_PERST_FLAG	1
#define IXGBE_ACI_NVM_EMPR_FLAG		2
#define IXGBE_ACI_NVM_EMPR_ENA		BIT(0) /* Write Activate reply only */
	/* For Write Activate, several flags are sent as part of a separate
	 * flags2 field using a separate byte. For simplicity of the software
	 * interface, we pass the flags as a 16 bit value so these flags are
	 * all offset by 8 bits
	 */
#define IXGBE_ACI_NVM_ACTIV_REQ_EMPR	BIT(8) /* NVM Write Activate only */
	__le16 module_typeid;
	__le16 length;
#define IXGBE_ACI_NVM_ERASE_LEN	0xFFFF
	__le32 addr_high;
	__le32 addr_low;
};

/* NVM Module_Type ID, needed offset and read_len for struct ixgbe_aci_cmd_nvm. */
#define IXGBE_ACI_NVM_SECTOR_UNIT		4096 /* In Bytes */
#define IXGBE_ACI_NVM_WORD_UNIT			2 /* In Bytes */

#define IXGBE_ACI_NVM_START_POINT		0
#define IXGBE_ACI_NVM_EMP_SR_PTR_OFFSET		0x90
#define IXGBE_ACI_NVM_EMP_SR_PTR_RD_LEN		2 /* In Bytes */
#define IXGBE_ACI_NVM_EMP_SR_PTR_M		MAKEMASK(0x7FFF, 0)
#define IXGBE_ACI_NVM_EMP_SR_PTR_TYPE_S		15
#define IXGBE_ACI_NVM_EMP_SR_PTR_TYPE_M		BIT(15)
#define IXGBE_ACI_NVM_EMP_SR_PTR_TYPE_SECTOR	1

#define IXGBE_ACI_NVM_LLDP_CFG_PTR_OFFSET	0x46
#define IXGBE_ACI_NVM_LLDP_CFG_HEADER_LEN	2 /* In Bytes */
#define IXGBE_ACI_NVM_LLDP_CFG_PTR_RD_LEN	2 /* In Bytes */

#define IXGBE_ACI_NVM_LLDP_PRESERVED_MOD_ID		0x129
#define IXGBE_ACI_NVM_CUR_LLDP_PERSIST_RD_OFFSET	2 /* In Bytes */
#define IXGBE_ACI_NVM_LLDP_STATUS_M			MAKEMASK(0xF, 0)
#define IXGBE_ACI_NVM_LLDP_STATUS_M_LEN			4 /* In Bits */
#define IXGBE_ACI_NVM_LLDP_STATUS_RD_LEN		4 /* In Bytes */

#define IXGBE_ACI_NVM_MINSREV_MOD_ID		0x130

IXGBE_CHECK_PARAM_LEN(ixgbe_aci_cmd_nvm);

/* Used for reading and writing MinSRev using 0x0701 and 0x0703. Note that the
 * type field is excluded from the section when reading and writing from
 * a module using the module_typeid field with these AQ commands.
 */
struct ixgbe_aci_cmd_nvm_minsrev {
	__le16 length;
	__le16 validity;
#define IXGBE_ACI_NVM_MINSREV_NVM_VALID		BIT(0)
#define IXGBE_ACI_NVM_MINSREV_OROM_VALID	BIT(1)
	__le16 nvm_minsrev_l;
	__le16 nvm_minsrev_h;
	__le16 orom_minsrev_l;
	__le16 orom_minsrev_h;
};

IXGBE_CHECK_STRUCT_LEN(12, ixgbe_aci_cmd_nvm_minsrev);

/* Used for 0x0704 as well as for 0x0705 commands */
struct ixgbe_aci_cmd_nvm_cfg {
	u8	cmd_flags;
#define IXGBE_ACI_ANVM_MULTIPLE_ELEMS	BIT(0)
#define IXGBE_ACI_ANVM_IMMEDIATE_FIELD	BIT(1)
#define IXGBE_ACI_ANVM_NEW_CFG		BIT(2)
	u8	reserved;
	__le16 count;
	__le16 id;
	u8 reserved1[2];
	__le32 addr_high;
	__le32 addr_low;
};

IXGBE_CHECK_PARAM_LEN(ixgbe_aci_cmd_nvm_cfg);

struct ixgbe_aci_cmd_nvm_cfg_data {
	__le16 field_id;
	__le16 field_options;
	__le16 field_value;
};

IXGBE_CHECK_STRUCT_LEN(6, ixgbe_aci_cmd_nvm_cfg_data);

/* NVM Checksum Command (direct, 0x0706) */
struct ixgbe_aci_cmd_nvm_checksum {
	u8 flags;
#define IXGBE_ACI_NVM_CHECKSUM_VERIFY	BIT(0)
#define IXGBE_ACI_NVM_CHECKSUM_RECALC	BIT(1)
	u8 rsvd;
	__le16 checksum; /* Used only by response */
#define IXGBE_ACI_NVM_CHECKSUM_CORRECT	0xBABA
	u8 rsvd2[12];
};

IXGBE_CHECK_PARAM_LEN(ixgbe_aci_cmd_nvm_checksum);

/* Used for NVM Sanitization command - 0x070C */
struct ixgbe_aci_cmd_nvm_sanitization {
	u8 cmd_flags;
#define IXGBE_ACI_SANITIZE_REQ_READ			0
#define IXGBE_ACI_SANITIZE_REQ_OPERATE			BIT(0)

#define IXGBE_ACI_SANITIZE_READ_SUBJECT_NVM_BITS	0
#define IXGBE_ACI_SANITIZE_READ_SUBJECT_NVM_STATE	BIT(1)
#define IXGBE_ACI_SANITIZE_OPERATE_SUBJECT_CLEAR	0
	u8 values;
#define IXGBE_ACI_SANITIZE_NVM_BITS_HOST_CLEAN_SUPPORT	BIT(0)
#define IXGBE_ACI_SANITIZE_NVM_BITS_BMC_CLEAN_SUPPORT	BIT(2)
#define IXGBE_ACI_SANITIZE_NVM_STATE_HOST_CLEAN_DONE	BIT(0)
#define IXGBE_ACI_SANITIZE_NVM_STATE_HOST_CLEAN_SUCCESS	BIT(1)
#define IXGBE_ACI_SANITIZE_NVM_STATE_BMC_CLEAN_DONE	BIT(2)
#define IXGBE_ACI_SANITIZE_NVM_STATE_BMC_CLEAN_SUCCESS	BIT(3)
#define IXGBE_ACI_SANITIZE_OPERATE_HOST_CLEAN_DONE	BIT(0)
#define IXGBE_ACI_SANITIZE_OPERATE_HOST_CLEAN_SUCCESS	BIT(1)
#define IXGBE_ACI_SANITIZE_OPERATE_BMC_CLEAN_DONE	BIT(2)
#define IXGBE_ACI_SANITIZE_OPERATE_BMC_CLEAN_SUCCESS	BIT(3)
	u8 reserved[14];
};

IXGBE_CHECK_PARAM_LEN(ixgbe_aci_cmd_nvm_sanitization);

/* Write/Read Alternate - Direct (direct 0x0900/0x0902) */
struct ixgbe_aci_cmd_read_write_alt_direct {
	__le32 dword0_addr;
	__le32 dword0_value;
	__le32 dword1_addr;
	__le32 dword1_value;
};

IXGBE_CHECK_PARAM_LEN(ixgbe_aci_cmd_read_write_alt_direct);

/* Write/Read Alternate - Indirect (indirect 0x0901/0x0903) */
struct ixgbe_aci_cmd_read_write_alt_indirect {
	__le32 base_dword_addr;
	__le32 num_dwords;
	__le32 addr_high;
	__le32 addr_low;
};

IXGBE_CHECK_PARAM_LEN(ixgbe_aci_cmd_read_write_alt_indirect);

/* Done Alternate Write (direct 0x0904) */
struct ixgbe_aci_cmd_done_alt_write {
	u8 flags;
#define IXGBE_ACI_CMD_UEFI_BIOS_MODE	BIT(0)
#define IXGBE_ACI_RESP_RESET_NEEDED	BIT(1)
	u8 reserved[15];
};

IXGBE_CHECK_PARAM_LEN(ixgbe_aci_cmd_done_alt_write);

/* Clear Port Alternate Write (direct 0x0906) */
struct ixgbe_aci_cmd_clear_port_alt_write {
	u8 reserved[16];
};

IXGBE_CHECK_PARAM_LEN(ixgbe_aci_cmd_clear_port_alt_write);

/* Get CGU abilities command response data structure (indirect 0x0C61) */
struct ixgbe_aci_cmd_get_cgu_abilities {
	u8 num_inputs;
	u8 num_outputs;
	u8 pps_dpll_idx;
	u8 synce_dpll_idx;
	__le32 max_in_freq;
	__le32 max_in_phase_adj;
	__le32 max_out_freq;
	__le32 max_out_phase_adj;
	u8 cgu_part_num;
	u8 rsvd[3];
};

IXGBE_CHECK_STRUCT_LEN(24, ixgbe_aci_cmd_get_cgu_abilities);

#define IXGBE_ACI_NODE_HANDLE_VALID	BIT(10)
#define IXGBE_ACI_NODE_HANDLE		MAKEMASK(0x3FF, 0)
#define IXGBE_ACI_DRIVING_CLK_NUM_SHIFT	10
#define IXGBE_ACI_DRIVING_CLK_NUM	MAKEMASK(0x3F, IXGBE_ACI_DRIVING_CLK_NUM_SHIFT)

/* Set CGU input config (direct 0x0C62) */
struct ixgbe_aci_cmd_set_cgu_input_config {
	u8 input_idx;
	u8 flags1;
#define IXGBE_ACI_SET_CGU_IN_CFG_FLG1_UPDATE_FREQ	BIT(6)
#define IXGBE_ACI_SET_CGU_IN_CFG_FLG1_UPDATE_DELAY	BIT(7)
	u8 flags2;
#define IXGBE_ACI_SET_CGU_IN_CFG_FLG2_INPUT_EN		BIT(5)
#define IXGBE_ACI_SET_CGU_IN_CFG_FLG2_ESYNC_EN		BIT(6)
	u8 rsvd;
	__le32 freq;
	__le32 phase_delay;
	u8 rsvd2[2];
	__le16 node_handle;
};

IXGBE_CHECK_PARAM_LEN(ixgbe_aci_cmd_set_cgu_input_config);

/* Get CGU input config response descriptor structure (direct 0x0C63) */
struct ixgbe_aci_cmd_get_cgu_input_config {
	u8 input_idx;
	u8 status;
#define IXGBE_ACI_GET_CGU_IN_CFG_STATUS_LOS		BIT(0)
#define IXGBE_ACI_GET_CGU_IN_CFG_STATUS_SCM_FAIL	BIT(1)
#define IXGBE_ACI_GET_CGU_IN_CFG_STATUS_CFM_FAIL	BIT(2)
#define IXGBE_ACI_GET_CGU_IN_CFG_STATUS_GST_FAIL	BIT(3)
#define IXGBE_ACI_GET_CGU_IN_CFG_STATUS_PFM_FAIL	BIT(4)
#define IXGBE_ACI_GET_CGU_IN_CFG_STATUS_ESYNC_FAIL	BIT(6)
#define IXGBE_ACI_GET_CGU_IN_CFG_STATUS_ESYNC_CAP	BIT(7)
	u8 type;
#define IXGBE_ACI_GET_CGU_IN_CFG_TYPE_READ_ONLY		BIT(0)
#define IXGBE_ACI_GET_CGU_IN_CFG_TYPE_GPS		BIT(4)
#define IXGBE_ACI_GET_CGU_IN_CFG_TYPE_EXTERNAL		BIT(5)
#define IXGBE_ACI_GET_CGU_IN_CFG_TYPE_PHY		BIT(6)
	u8 flags1;
#define IXGBE_ACI_GET_CGU_IN_CFG_FLG1_PHASE_DELAY_SUPP	BIT(0)
#define IXGBE_ACI_GET_CGU_IN_CFG_FLG1_1PPS_SUPP		BIT(2)
#define IXGBE_ACI_GET_CGU_IN_CFG_FLG1_10MHZ_SUPP	BIT(3)
#define IXGBE_ACI_GET_CGU_IN_CFG_FLG1_ANYFREQ		BIT(7)
	__le32 freq;
	__le32 phase_delay;
	u8 flags2;
#define IXGBE_ACI_GET_CGU_IN_CFG_FLG2_INPUT_EN		BIT(5)
#define IXGBE_ACI_GET_CGU_IN_CFG_FLG2_ESYNC_EN		BIT(6)
	u8 rsvd[1];
	__le16 node_handle;
};

IXGBE_CHECK_PARAM_LEN(ixgbe_aci_cmd_get_cgu_input_config);

/* Set CGU output config (direct 0x0C64) */
struct ixgbe_aci_cmd_set_cgu_output_config {
	u8 output_idx;
	u8 flags;
#define IXGBE_ACI_SET_CGU_OUT_CFG_OUT_EN		BIT(0)
#define IXGBE_ACI_SET_CGU_OUT_CFG_ESYNC_EN		BIT(1)
#define IXGBE_ACI_SET_CGU_OUT_CFG_UPDATE_FREQ		BIT(2)
#define IXGBE_ACI_SET_CGU_OUT_CFG_UPDATE_PHASE		BIT(3)
#define IXGBE_ACI_SET_CGU_OUT_CFG_UPDATE_SRC_SEL	BIT(4)
	u8 src_sel;
#define IXGBE_ACI_SET_CGU_OUT_CFG_DPLL_SRC_SEL		MAKEMASK(0x1F, 0)
	u8 rsvd;
	__le32 freq;
	__le32 phase_delay;
	u8 rsvd2[2];
	__le16 node_handle;
};

IXGBE_CHECK_PARAM_LEN(ixgbe_aci_cmd_set_cgu_output_config);

/* Get CGU output config (direct 0x0C65) */
struct ixgbe_aci_cmd_get_cgu_output_config {
	u8 output_idx;
	u8 flags;
#define IXGBE_ACI_GET_CGU_OUT_CFG_OUT_EN		BIT(0)
#define IXGBE_ACI_GET_CGU_OUT_CFG_ESYNC_EN		BIT(1)
#define IXGBE_ACI_GET_CGU_OUT_CFG_ESYNC_ABILITY		BIT(2)
	u8 src_sel;
#define IXGBE_ACI_GET_CGU_OUT_CFG_DPLL_SRC_SEL_SHIFT	0
#define IXGBE_ACI_GET_CGU_OUT_CFG_DPLL_SRC_SEL \
	MAKEMASK(0x1F, IXGBE_ACI_GET_CGU_OUT_CFG_DPLL_SRC_SEL_SHIFT)
#define IXGBE_ACI_GET_CGU_OUT_CFG_DPLL_MODE_SHIFT	5
#define IXGBE_ACI_GET_CGU_OUT_CFG_DPLL_MODE \
	MAKEMASK(0x7, IXGBE_ACI_GET_CGU_OUT_CFG_DPLL_MODE_SHIFT)
	u8 rsvd;
	__le32 freq;
	__le32 src_freq;
	u8 rsvd2[2];
	__le16 node_handle;
};

IXGBE_CHECK_PARAM_LEN(ixgbe_aci_cmd_get_cgu_output_config);

/* Get CGU DPLL status (direct 0x0C66) */
struct ixgbe_aci_cmd_get_cgu_dpll_status {
	u8 dpll_num;
	u8 ref_state;
#define IXGBE_ACI_GET_CGU_DPLL_STATUS_REF_SW_LOS		BIT(0)
#define IXGBE_ACI_GET_CGU_DPLL_STATUS_REF_SW_SCM		BIT(1)
#define IXGBE_ACI_GET_CGU_DPLL_STATUS_REF_SW_CFM		BIT(2)
#define IXGBE_ACI_GET_CGU_DPLL_STATUS_REF_SW_GST		BIT(3)
#define IXGBE_ACI_GET_CGU_DPLL_STATUS_REF_SW_PFM		BIT(4)
#define IXGBE_ACI_GET_CGU_DPLL_STATUS_FAST_LOCK_EN		BIT(5)
#define IXGBE_ACI_GET_CGU_DPLL_STATUS_REF_SW_ESYNC		BIT(6)
	__le16 dpll_state;
#define IXGBE_ACI_GET_CGU_DPLL_STATUS_STATE_LOCK		BIT(0)
#define IXGBE_ACI_GET_CGU_DPLL_STATUS_STATE_HO			BIT(1)
#define IXGBE_ACI_GET_CGU_DPLL_STATUS_STATE_HO_READY		BIT(2)
#define IXGBE_ACI_GET_CGU_DPLL_STATUS_STATE_FLHIT		BIT(5)
#define IXGBE_ACI_GET_CGU_DPLL_STATUS_STATE_PSLHIT		BIT(7)
#define IXGBE_ACI_GET_CGU_DPLL_STATUS_STATE_CLK_REF_SHIFT	8
#define IXGBE_ACI_GET_CGU_DPLL_STATUS_STATE_CLK_REF_SEL		\
	MAKEMASK(0x1F, IXGBE_ACI_GET_CGU_DPLL_STATUS_STATE_CLK_REF_SHIFT)
#define IXGBE_ACI_GET_CGU_DPLL_STATUS_STATE_MODE_SHIFT		13
#define IXGBE_ACI_GET_CGU_DPLL_STATUS_STATE_MODE 		\
	MAKEMASK(0x7, IXGBE_ACI_GET_CGU_DPLL_STATUS_STATE_MODE_SHIFT)
	__le32 phase_offset_h;
	__le32 phase_offset_l;
	u8 eec_mode;
#define IXGBE_ACI_GET_CGU_DPLL_STATUS_EEC_MODE_1		0xA
#define IXGBE_ACI_GET_CGU_DPLL_STATUS_EEC_MODE_2		0xB
#define IXGBE_ACI_GET_CGU_DPLL_STATUS_EEC_MODE_UNKNOWN		0xF
	u8 rsvd[1];
	__le16 node_handle;
};

IXGBE_CHECK_PARAM_LEN(ixgbe_aci_cmd_get_cgu_dpll_status);

/* Set CGU DPLL config (direct 0x0C67) */
struct ixgbe_aci_cmd_set_cgu_dpll_config {
	u8 dpll_num;
	u8 ref_state;
#define IXGBE_ACI_SET_CGU_DPLL_CONFIG_REF_SW_LOS	BIT(0)
#define IXGBE_ACI_SET_CGU_DPLL_CONFIG_REF_SW_SCM	BIT(1)
#define IXGBE_ACI_SET_CGU_DPLL_CONFIG_REF_SW_CFM	BIT(2)
#define IXGBE_ACI_SET_CGU_DPLL_CONFIG_REF_SW_GST	BIT(3)
#define IXGBE_ACI_SET_CGU_DPLL_CONFIG_REF_SW_PFM	BIT(4)
#define IXGBE_ACI_SET_CGU_DPLL_CONFIG_REF_FLOCK_EN	BIT(5)
#define IXGBE_ACI_SET_CGU_DPLL_CONFIG_REF_SW_ESYNC	BIT(6)
	u8 rsvd;
	u8 config;
#define IXGBE_ACI_SET_CGU_DPLL_CONFIG_CLK_REF_SEL	MAKEMASK(0x1F, 0)
#define IXGBE_ACI_SET_CGU_DPLL_CONFIG_MODE		MAKEMASK(0x7, 5)
	u8 rsvd2[8];
	u8 eec_mode;
	u8 rsvd3[1];
	__le16 node_handle;
};

IXGBE_CHECK_PARAM_LEN(ixgbe_aci_cmd_set_cgu_dpll_config);

/* Set CGU reference priority (direct 0x0C68) */
struct ixgbe_aci_cmd_set_cgu_ref_prio {
	u8 dpll_num;
	u8 ref_idx;
	u8 ref_priority;
	u8 rsvd[11];
	__le16 node_handle;
};

IXGBE_CHECK_PARAM_LEN(ixgbe_aci_cmd_set_cgu_ref_prio);

/* Get CGU reference priority (direct 0x0C69) */
struct ixgbe_aci_cmd_get_cgu_ref_prio {
	u8 dpll_num;
	u8 ref_idx;
	u8 ref_priority; /* Valid only in response */
	u8 rsvd[13];
};

IXGBE_CHECK_PARAM_LEN(ixgbe_aci_cmd_get_cgu_ref_prio);

/* Get CGU info (direct 0x0C6A) */
struct ixgbe_aci_cmd_get_cgu_info {
	__le32 cgu_id;
	__le32 cgu_cfg_ver;
	__le32 cgu_fw_ver;
	u8 node_part_num;
	u8 dev_rev;
	__le16 node_handle;
};

IXGBE_CHECK_PARAM_LEN(ixgbe_aci_cmd_get_cgu_info);

struct ixgbe_aci_cmd_temp_tca_event {
        u8 event_desc;
#define IXGBE_TEMP_TCA_EVENT_DESC_SUBJ_SHIFT         0
#define IXGBE_TEMP_TCA_EVENT_DESC_SUBJ_NVM           0
#define IXGBE_TEMP_TCA_EVENT_DESC_SUBJ_EVENT_STATE   1
#define IXGBE_TEMP_TCA_EVENT_DESC_SUBJ_ALL           2

#define IXGBE_TEMP_TCA_EVENT_DESC_ALARM_SHIFT        2
#define IXGBE_TEMP_TCA_EVENT_DESC_WARNING_CLEARED    0
#define IXGBE_TEMP_TCA_EVENT_DESC_ALARM_CLEARED      1
#define IXGBE_TEMP_TCA_EVENT_DESC_WARNING_RAISED     2
#define IXGBE_TEMP_TCA_EVENT_DESC_ALARM_RAISED       3

        u8 reserved;
        __le16 temperature;
        __le16 thermal_sensor_max_value;
        __le16 thermal_sensor_min_value;
        __le32 addr_high;
        __le32 addr_low;
};

IXGBE_CHECK_PARAM_LEN(ixgbe_aci_cmd_temp_tca_event);

/* Debug Dump Internal Data (indirect 0xFF08) */
struct ixgbe_aci_cmd_debug_dump_internals {
	__le16 cluster_id; /* Expresses next cluster ID in response */
#define IXGBE_ACI_DBG_DUMP_CLUSTER_ID_LINK		0
#define IXGBE_ACI_DBG_DUMP_CLUSTER_ID_FULL_CSR_SPACE	1
	__le16 table_id; /* Used only for non-memory clusters */
	__le32 idx; /* In table entries for tables, in bytes for memory */
	__le32 addr_high;
	__le32 addr_low;
};

IXGBE_CHECK_PARAM_LEN(ixgbe_aci_cmd_debug_dump_internals);

/* Set Health Status (direct 0xFF20) */
struct ixgbe_aci_cmd_set_health_status_config {
	u8 event_source;
#define IXGBE_ACI_HEALTH_STATUS_SET_PF_SPECIFIC_MASK	BIT(0)
#define IXGBE_ACI_HEALTH_STATUS_SET_ALL_PF_MASK		BIT(1)
#define IXGBE_ACI_HEALTH_STATUS_SET_GLOBAL_MASK		BIT(2)
	u8 reserved[15];
};

IXGBE_CHECK_PARAM_LEN(ixgbe_aci_cmd_set_health_status_config);

#define IXGBE_ACI_HEALTH_STATUS_ERR_UNKNOWN_MOD_STRICT		0x101
#define IXGBE_ACI_HEALTH_STATUS_ERR_MOD_TYPE			0x102
#define IXGBE_ACI_HEALTH_STATUS_ERR_MOD_QUAL			0x103
#define IXGBE_ACI_HEALTH_STATUS_ERR_MOD_COMM			0x104
#define IXGBE_ACI_HEALTH_STATUS_ERR_MOD_CONFLICT		0x105
#define IXGBE_ACI_HEALTH_STATUS_ERR_MOD_NOT_PRESENT		0x106
#define IXGBE_ACI_HEALTH_STATUS_INFO_MOD_UNDERUTILIZED		0x107
#define IXGBE_ACI_HEALTH_STATUS_ERR_UNKNOWN_MOD_LENIENT		0x108
#define IXGBE_ACI_HEALTH_STATUS_ERR_MOD_DIAGNOSTIC_FEATURE	0x109
#define IXGBE_ACI_HEALTH_STATUS_ERR_INVALID_LINK_CFG		0x10B
#define IXGBE_ACI_HEALTH_STATUS_ERR_PORT_ACCESS			0x10C
#define IXGBE_ACI_HEALTH_STATUS_ERR_PORT_UNREACHABLE		0x10D
#define IXGBE_ACI_HEALTH_STATUS_INFO_PORT_SPEED_MOD_LIMITED	0x10F
#define IXGBE_ACI_HEALTH_STATUS_ERR_PARALLEL_FAULT		0x110
#define IXGBE_ACI_HEALTH_STATUS_INFO_PORT_SPEED_PHY_LIMITED	0x111
#define IXGBE_ACI_HEALTH_STATUS_ERR_NETLIST_TOPO		0x112
#define IXGBE_ACI_HEALTH_STATUS_ERR_NETLIST			0x113
#define IXGBE_ACI_HEALTH_STATUS_ERR_TOPO_CONFLICT		0x114
#define IXGBE_ACI_HEALTH_STATUS_ERR_LINK_HW_ACCESS		0x115
#define IXGBE_ACI_HEALTH_STATUS_ERR_LINK_RUNTIME		0x116
#define IXGBE_ACI_HEALTH_STATUS_ERR_DNL_INIT			0x117
#define IXGBE_ACI_HEALTH_STATUS_ERR_PHY_NVM_PROG		0x120
#define IXGBE_ACI_HEALTH_STATUS_ERR_PHY_FW_LOAD			0x121
#define IXGBE_ACI_HEALTH_STATUS_INFO_RECOVERY			0x500
#define IXGBE_ACI_HEALTH_STATUS_ERR_FLASH_ACCESS		0x501
#define IXGBE_ACI_HEALTH_STATUS_ERR_NVM_AUTH			0x502
#define IXGBE_ACI_HEALTH_STATUS_ERR_OROM_AUTH			0x503
#define IXGBE_ACI_HEALTH_STATUS_ERR_DDP_AUTH			0x504
#define IXGBE_ACI_HEALTH_STATUS_ERR_NVM_COMPAT			0x505
#define IXGBE_ACI_HEALTH_STATUS_ERR_OROM_COMPAT			0x506
#define IXGBE_ACI_HEALTH_STATUS_ERR_NVM_SEC_VIOLATION		0x507
#define IXGBE_ACI_HEALTH_STATUS_ERR_OROM_SEC_VIOLATION		0x508
#define IXGBE_ACI_HEALTH_STATUS_ERR_DCB_MIB			0x509
#define IXGBE_ACI_HEALTH_STATUS_ERR_MNG_TIMEOUT			0x50A
#define IXGBE_ACI_HEALTH_STATUS_ERR_BMC_RESET			0x50B
#define IXGBE_ACI_HEALTH_STATUS_ERR_LAST_MNG_FAIL		0x50C
#define IXGBE_ACI_HEALTH_STATUS_ERR_RESOURCE_ALLOC_FAIL		0x50D
#define IXGBE_ACI_HEALTH_STATUS_ERR_FW_LOOP			0x1000
#define IXGBE_ACI_HEALTH_STATUS_ERR_FW_PFR_FAIL			0x1001
#define IXGBE_ACI_HEALTH_STATUS_ERR_LAST_FAIL_AQ		0x1002

/* Get Health Status codes (indirect 0xFF21) */
struct ixgbe_aci_cmd_get_supported_health_status_codes {
	__le16 health_code_count;
	u8 reserved[6];
	__le32 addr_high;
	__le32 addr_low;
};

IXGBE_CHECK_PARAM_LEN(ixgbe_aci_cmd_get_supported_health_status_codes);

/* Get Health Status (indirect 0xFF22) */
struct ixgbe_aci_cmd_get_health_status {
	__le16 health_status_count;
	u8 reserved[6];
	__le32 addr_high;
	__le32 addr_low;
};

IXGBE_CHECK_PARAM_LEN(ixgbe_aci_cmd_get_health_status);

/* Get Health Status event buffer entry, (0xFF22)
 * repeated per reported health status
 */
struct ixgbe_aci_cmd_health_status_elem {
	__le16 health_status_code;
	__le16 event_source;
#define IXGBE_ACI_HEALTH_STATUS_PF		(0x1)
#define IXGBE_ACI_HEALTH_STATUS_PORT		(0x2)
#define IXGBE_ACI_HEALTH_STATUS_GLOBAL		(0x3)
	__le32 internal_data1;
#define IXGBE_ACI_HEALTH_STATUS_UNDEFINED_DATA	(0xDEADBEEF)
	__le32 internal_data2;
};

IXGBE_CHECK_STRUCT_LEN(12, ixgbe_aci_cmd_health_status_elem);

/* Clear Health Status (direct 0xFF23) */
struct ixgbe_aci_cmd_clear_health_status {
	__le32 reserved[4];
};

IXGBE_CHECK_PARAM_LEN(ixgbe_aci_cmd_clear_health_status);

enum ixgbe_aci_fw_logging_mod {
	IXGBE_ACI_FW_LOG_ID_GENERAL = 0,
	IXGBE_ACI_FW_LOG_ID_CTRL = 1,
	IXGBE_ACI_FW_LOG_ID_LINK = 2,
	IXGBE_ACI_FW_LOG_ID_LINK_TOPO = 3,
	IXGBE_ACI_FW_LOG_ID_DNL = 4,
	IXGBE_ACI_FW_LOG_ID_I2C = 5,
	IXGBE_ACI_FW_LOG_ID_SDP = 6,
	IXGBE_ACI_FW_LOG_ID_MDIO = 7,
	IXGBE_ACI_FW_LOG_ID_ADMINQ = 8,
	IXGBE_ACI_FW_LOG_ID_HDMA = 9,
	IXGBE_ACI_FW_LOG_ID_LLDP = 10,
	IXGBE_ACI_FW_LOG_ID_DCBX = 11,
	IXGBE_ACI_FW_LOG_ID_DCB = 12,
	IXGBE_ACI_FW_LOG_ID_XLR = 13,
	IXGBE_ACI_FW_LOG_ID_NVM = 14,
	IXGBE_ACI_FW_LOG_ID_AUTH = 15,
	IXGBE_ACI_FW_LOG_ID_VPD = 16,
	IXGBE_ACI_FW_LOG_ID_IOSF = 17,
	IXGBE_ACI_FW_LOG_ID_PARSER = 18,
	IXGBE_ACI_FW_LOG_ID_SW = 19,
	IXGBE_ACI_FW_LOG_ID_SCHEDULER = 20,
	IXGBE_ACI_FW_LOG_ID_TXQ = 21,
	IXGBE_ACI_FW_LOG_ID_ACL = 22,
	IXGBE_ACI_FW_LOG_ID_POST = 23,
	IXGBE_ACI_FW_LOG_ID_WATCHDOG = 24,
	IXGBE_ACI_FW_LOG_ID_TASK_DISPATCH = 25,
	IXGBE_ACI_FW_LOG_ID_MNG = 26,
	IXGBE_ACI_FW_LOG_ID_SYNCE = 27,
	IXGBE_ACI_FW_LOG_ID_HEALTH = 28,
	IXGBE_ACI_FW_LOG_ID_TSDRV = 29,
	IXGBE_ACI_FW_LOG_ID_PFREG = 30,
	IXGBE_ACI_FW_LOG_ID_MDLVER = 31,
	IXGBE_ACI_FW_LOG_ID_MAX = 32,
};

/* Only a single log level should be set and all log levels under the set value
 * are enabled, e.g. if log level is set to IXGBE_FWLOG_LEVEL_VERBOSE, then all
 * other log levels are included (except IXGBE_FWLOG_LEVEL_NONE)
 */
enum ixgbe_fwlog_level {
	IXGBE_FWLOG_LEVEL_NONE = 0,
	IXGBE_FWLOG_LEVEL_ERROR = 1,
	IXGBE_FWLOG_LEVEL_WARNING = 2,
	IXGBE_FWLOG_LEVEL_NORMAL = 3,
	IXGBE_FWLOG_LEVEL_VERBOSE = 4,
	IXGBE_FWLOG_LEVEL_INVALID, /* all values >= this entry are invalid */
};

struct ixgbe_fwlog_module_entry {
	/* module ID for the corresponding firmware logging event */
	u16 module_id;
	/* verbosity level for the module_id */
	u8 log_level;
};

struct ixgbe_fwlog_cfg {
	/* list of modules for configuring log level */
	struct ixgbe_fwlog_module_entry module_entries[IXGBE_ACI_FW_LOG_ID_MAX];
#define IXGBE_FWLOG_OPTION_ARQ_ENA		BIT(0)
#define IXGBE_FWLOG_OPTION_UART_ENA		BIT(1)
	/* set before calling ixgbe_fwlog_init() so the PF registers for firmware
	 * logging on initialization
	 */
#define IXGBE_FWLOG_OPTION_REGISTER_ON_INIT	BIT(2)
	/* set in the ixgbe_fwlog_get() response if the PF is registered for FW
	 * logging events over ARQ
	 */
#define IXGBE_FWLOG_OPTION_IS_REGISTERED	BIT(3)
	/* options used to configure firmware logging */
	u16 options;
	/* minimum number of log events sent per Admin Receive Queue event */
	u8 log_resolution;
};

struct ixgbe_fwlog_data {
	u16 data_size;
	u8 *data;
};

struct ixgbe_fwlog_ring {
	struct ixgbe_fwlog_data *rings;
	u16 size;
	u16 head;
	u16 tail;
};

#define IXGBE_FWLOG_RING_SIZE_DFLT 256
#define IXGBE_FWLOG_RING_SIZE_MAX 512

/* Set FW Logging configuration (indirect 0xFF30)
 * Register for FW Logging (indirect 0xFF31)
 * Query FW Logging (indirect 0xFF32)
 * FW Log Event (indirect 0xFF33)
 * Get FW Log (indirect 0xFF34)
 * Clear FW Log (indirect 0xFF35)
 */
struct ixgbe_aci_cmd_fw_log {
	u8 cmd_flags;
#define IXGBE_ACI_FW_LOG_CONF_UART_EN		BIT(0)
#define IXGBE_ACI_FW_LOG_CONF_AQ_EN		BIT(1)
#define IXGBE_ACI_FW_LOG_QUERY_REGISTERED	BIT(2)
#define IXGBE_ACI_FW_LOG_CONF_SET_VALID		BIT(3)
#define IXGBE_ACI_FW_LOG_AQ_REGISTER		BIT(0)
#define IXGBE_ACI_FW_LOG_AQ_QUERY		BIT(2)
#define IXGBE_ACI_FW_LOG_PERSISTENT		BIT(0)
	u8 rsp_flag;
#define IXGBE_ACI_FW_LOG_MORE_DATA		BIT(1)
	__le16 fw_rt_msb;
	union {
		struct {
			__le32 fw_rt_lsb;
		} sync;
		struct {
			__le16 log_resolution;
#define IXGBE_ACI_FW_LOG_MIN_RESOLUTION		(1)
#define IXGBE_ACI_FW_LOG_MAX_RESOLUTION		(128)
			__le16 mdl_cnt;
		} cfg;
	} ops;
	__le32 addr_high;
	__le32 addr_low;
};

IXGBE_CHECK_PARAM_LEN(ixgbe_aci_cmd_fw_log);

/* Response Buffer for:
 *    Set Firmware Logging Configuration (0xFF30)
 *    Query FW Logging (0xFF32)
 */
struct ixgbe_aci_cmd_fw_log_cfg_resp {
	__le16 module_identifier;
	u8 log_level;
	u8 rsvd0;
};

IXGBE_CHECK_STRUCT_LEN(4, ixgbe_aci_cmd_fw_log_cfg_resp);

/**
 * struct ixgbe_aq_desc - Admin Command (AC) descriptor
 * @flags: IXGBE_ACI_FLAG_* flags
 * @opcode: Admin command opcode
 * @datalen: length in bytes of indirect/external data buffer
 * @retval: return value from firmware
 * @cookie_high: opaque data high-half
 * @cookie_low: opaque data low-half
 * @params: command-specific parameters
 *
 * Descriptor format for commands the driver posts via the Admin Command Interface
 * (ACI). The firmware writes back onto the command descriptor and returns
 * the result of the command. Asynchronous events that are not an immediate
 * result of the command are written to the Admin Command Interface (ACI) using
 * the same descriptor format. Descriptors are in little-endian notation with
 * 32-bit words.
 */
struct ixgbe_aci_desc {
	__le16 flags;
	__le16 opcode;
	__le16 datalen;
	__le16 retval;
	__le32 cookie_high;
	__le32 cookie_low;
	union {
		u8 raw[16];
		struct ixgbe_aci_cmd_generic generic;
		struct ixgbe_aci_cmd_get_ver get_ver;
		struct ixgbe_aci_cmd_driver_ver driver_ver;
		struct ixgbe_aci_cmd_get_exp_err exp_err;
		struct ixgbe_aci_cmd_req_res res_owner;
		struct ixgbe_aci_cmd_list_caps get_cap;
		struct ixgbe_aci_cmd_disable_rxen disable_rxen;
		struct ixgbe_aci_cmd_get_fw_event get_fw_event;
		struct ixgbe_aci_cmd_get_phy_caps get_phy;
		struct ixgbe_aci_cmd_set_phy_cfg set_phy;
		struct ixgbe_aci_cmd_restart_an restart_an;
		struct ixgbe_aci_cmd_get_link_status get_link_status;
		struct ixgbe_aci_cmd_set_event_mask set_event_mask;
		struct ixgbe_aci_cmd_get_link_topo get_link_topo;
		struct ixgbe_aci_cmd_i2c read_write_i2c;
		struct ixgbe_aci_cmd_read_i2c_resp read_i2c_resp;
		struct ixgbe_aci_cmd_mdio read_write_mdio;
		struct ixgbe_aci_cmd_mdio read_mdio;
		struct ixgbe_aci_cmd_mdio write_mdio;
		struct ixgbe_aci_cmd_set_port_id_led set_port_id_led;
		struct ixgbe_aci_cmd_gpio_by_func read_write_gpio_by_func;
		struct ixgbe_aci_cmd_gpio read_write_gpio;
		struct ixgbe_aci_cmd_sff_eeprom read_write_sff_param;
		struct ixgbe_aci_cmd_prog_topo_dev_nvm prog_topo_dev_nvm;
		struct ixgbe_aci_cmd_read_topo_dev_nvm read_topo_dev_nvm;
		struct ixgbe_aci_cmd_nvm nvm;
		struct ixgbe_aci_cmd_nvm_cfg nvm_cfg;
		struct ixgbe_aci_cmd_nvm_checksum nvm_checksum;
		struct ixgbe_aci_cmd_read_write_alt_direct read_write_alt_direct;
		struct ixgbe_aci_cmd_read_write_alt_indirect read_write_alt_indirect;
		struct ixgbe_aci_cmd_done_alt_write done_alt_write;
		struct ixgbe_aci_cmd_clear_port_alt_write clear_port_alt_write;
		struct ixgbe_aci_cmd_debug_dump_internals debug_dump;
		struct ixgbe_aci_cmd_set_health_status_config
			set_health_status_config;
		struct ixgbe_aci_cmd_get_supported_health_status_codes
			get_supported_health_status_codes;
		struct ixgbe_aci_cmd_get_health_status get_health_status;
		struct ixgbe_aci_cmd_clear_health_status clear_health_status;
		struct ixgbe_aci_cmd_fw_log fw_log;
		struct ixgbe_aci_cmd_nvm_sanitization nvm_sanitization;
	} params;
};

/* E610-specific adapter context structures */

struct ixgbe_link_status {
	/* Refer to ixgbe_aci_phy_type for bits definition */
	u64 phy_type_low;
	u64 phy_type_high;
	u8 topo_media_conflict;
	u16 max_frame_size;
	u16 link_speed;
	u16 req_speeds;
	u8 link_cfg_err;
	u8 lse_ena;	/* Link Status Event notification */
	u8 link_info;
	u8 an_info;
	u8 ext_info;
	u8 fec_info;
	u8 pacing;
	/* Refer to #define from module_type[IXGBE_ACI_MODULE_TYPE_TOTAL_BYTE] of
	 * ixgbe_aci_get_phy_caps structure
	 */
	u8 module_type[IXGBE_ACI_MODULE_TYPE_TOTAL_BYTE];
};

/* Common HW capabilities for SW use */
struct ixgbe_hw_common_caps {
	/* Write CSR protection */
	u64 wr_csr_prot;
	u32 switching_mode;
	/* switching mode supported - EVB switching (including cloud) */
#define IXGBE_NVM_IMAGE_TYPE_EVB		0x0

	/* Manageability mode & supported protocols over MCTP */
	u32 mgmt_mode;
#define IXGBE_MGMT_MODE_PASS_THRU_MODE_M	0xF
#define IXGBE_MGMT_MODE_CTL_INTERFACE_M		0xF0
#define IXGBE_MGMT_MODE_REDIR_SB_INTERFACE_M	0xF00

	u32 mgmt_protocols_mctp;
#define IXGBE_MGMT_MODE_PROTO_RSVD	BIT(0)
#define IXGBE_MGMT_MODE_PROTO_PLDM	BIT(1)
#define IXGBE_MGMT_MODE_PROTO_OEM	BIT(2)
#define IXGBE_MGMT_MODE_PROTO_NC_SI	BIT(3)

	u32 os2bmc;
	u32 valid_functions;
	/* DCB capabilities */
	u32 active_tc_bitmap;
	u32 maxtc;

	/* RSS related capabilities */
	u32 rss_table_size;		/* 512 for PFs and 64 for VFs */
	u32 rss_table_entry_width;	/* RSS Entry width in bits */

	/* Tx/Rx queues */
	u32 num_rxq;			/* Number/Total Rx queues */
	u32 rxq_first_id;		/* First queue ID for Rx queues */
	u32 num_txq;			/* Number/Total Tx queues */
	u32 txq_first_id;		/* First queue ID for Tx queues */

	/* MSI-X vectors */
	u32 num_msix_vectors;
	u32 msix_vector_first_id;

	/* Max MTU for function or device */
	u32 max_mtu;

	/* WOL related */
	u32 num_wol_proxy_fltr;
	u32 wol_proxy_vsi_seid;

	/* LED/SDP pin count */
	u32 led_pin_num;
	u32 sdp_pin_num;

	/* LED/SDP - Supports up to 12 LED pins and 8 SDP signals */
#define IXGBE_MAX_SUPPORTED_GPIO_LED	12
#define IXGBE_MAX_SUPPORTED_GPIO_SDP	8
	u8 led[IXGBE_MAX_SUPPORTED_GPIO_LED];
	u8 sdp[IXGBE_MAX_SUPPORTED_GPIO_SDP];
	/* SR-IOV virtualization */
	u8 sr_iov_1_1;			/* SR-IOV enabled */
	/* VMDQ */
	u8 vmdq;			/* VMDQ supported */

	/* EVB capabilities */
	u8 evb_802_1_qbg;		/* Edge Virtual Bridging */
	u8 evb_802_1_qbh;		/* Bridge Port Extension */

	u8 dcb;
	u8 iscsi;
	u8 mgmt_cem;

	/* WoL and APM support */
#define IXGBE_WOL_SUPPORT_M		BIT(0)
#define IXGBE_ACPI_PROG_MTHD_M		BIT(1)
#define IXGBE_PROXY_SUPPORT_M		BIT(2)
	u8 apm_wol_support;
	u8 acpi_prog_mthd;
	u8 proxy_support;
	bool sec_rev_disabled;
	bool update_disabled;
	bool nvm_unified_update;
	bool netlist_auth;
#define IXGBE_NVM_MGMT_SEC_REV_DISABLED		BIT(0)
#define IXGBE_NVM_MGMT_UPDATE_DISABLED		BIT(1)
#define IXGBE_NVM_MGMT_UNIFIED_UPD_SUPPORT	BIT(3)
#define IXGBE_NVM_MGMT_NETLIST_AUTH_SUPPORT	BIT(5)
	bool no_drop_policy_support;
	/* PCIe reset avoidance */
	bool pcie_reset_avoidance; /* false: not supported, true: supported */
	/* Post update reset restriction */
	bool reset_restrict_support; /* false: not supported, true: supported */

	/* External topology device images within the NVM */
#define IXGBE_EXT_TOPO_DEV_IMG_COUNT	4
	u32 ext_topo_dev_img_ver_high[IXGBE_EXT_TOPO_DEV_IMG_COUNT];
	u32 ext_topo_dev_img_ver_low[IXGBE_EXT_TOPO_DEV_IMG_COUNT];
	u8 ext_topo_dev_img_part_num[IXGBE_EXT_TOPO_DEV_IMG_COUNT];
#define IXGBE_EXT_TOPO_DEV_IMG_PART_NUM_S	8
#define IXGBE_EXT_TOPO_DEV_IMG_PART_NUM_M	\
		MAKEMASK(0xFF, IXGBE_EXT_TOPO_DEV_IMG_PART_NUM_S)
	bool ext_topo_dev_img_load_en[IXGBE_EXT_TOPO_DEV_IMG_COUNT];
#define IXGBE_EXT_TOPO_DEV_IMG_LOAD_EN	BIT(0)
	bool ext_topo_dev_img_prog_en[IXGBE_EXT_TOPO_DEV_IMG_COUNT];
#define IXGBE_EXT_TOPO_DEV_IMG_PROG_EN	BIT(1)
	/* Support for OROM update in Recovery Mode. */
	bool orom_recovery_update;
	bool next_cluster_id_support;
};

#pragma pack(1)
struct ixgbe_orom_civd_info {
	u8 signature[4];	/* Must match ASCII '$CIV' characters */
	u8 checksum;		/* Simple modulo 256 sum of all structure bytes must equal 0 */
	__le32 combo_ver;	/* Combo Image Version number */
	u8 combo_name_len;	/* Length of the unicode combo image version string, max of 32 */
	__le16 combo_name[32];	/* Unicode string representing the Combo Image version */
};
#pragma pack()

/* Function specific capabilities */
struct ixgbe_hw_func_caps {
	struct ixgbe_hw_common_caps common_cap;
	u32 num_allocd_vfs;		/* Number of allocated VFs */
	u32 vf_base_id;			/* Logical ID of the first VF */
	u32 guar_num_vsi;
	bool no_drop_policy_ena;
};

/* Device wide capabilities */
struct ixgbe_hw_dev_caps {
	struct ixgbe_hw_common_caps common_cap;
	u32 num_vfs_exposed;		/* Total number of VFs exposed */
	u32 num_vsi_allocd_to_host;	/* Excluding EMP VSI */
	u32 num_flow_director_fltr;	/* Number of FD filters available */
	u32 num_funcs;
};

/* ACI event information */
struct ixgbe_aci_event {
	struct ixgbe_aci_desc desc;
	u16 msg_len;
	u16 buf_len;
	u8 *msg_buf;
};

struct ixgbe_aci_info {
	enum ixgbe_aci_err last_status;	/* last status of sent admin command */
	struct ixgbe_lock lock;		/* admin command interface lock */
};

/* Minimum Security Revision information */
struct ixgbe_minsrev_info {
	u32 nvm;
	u32 orom;
	u8 nvm_valid : 1;
	u8 orom_valid : 1;
};

/* Enumeration of which flash bank is desired to read from, either the active
 * bank or the inactive bank. Used to abstract 1st and 2nd bank notion from
 * code which just wants to read the active or inactive flash bank.
 */
enum ixgbe_bank_select {
	IXGBE_ACTIVE_FLASH_BANK,
	IXGBE_INACTIVE_FLASH_BANK,
};

/* Option ROM version information */
struct ixgbe_orom_info {
	u8 major;			/* Major version of OROM */
	u8 patch;			/* Patch version of OROM */
	u16 build;			/* Build version of OROM */
	u32 srev;			/* Security revision */
};

/* NVM version information */
struct ixgbe_nvm_info {
	u32 eetrack;
	u32 srev;
	u8 major;
	u8 minor;
};

/* netlist version information */
struct ixgbe_netlist_info {
	u32 major;			/* major high/low */
	u32 minor;			/* minor high/low */
	u32 type;			/* type high/low */
	u32 rev;			/* revision high/low */
	u32 hash;			/* SHA-1 hash word */
	u16 cust_ver;			/* customer version */
};

/* Enumeration of possible flash banks for the NVM, OROM, and Netlist modules
 * of the flash image.
 */
enum ixgbe_flash_bank {
	IXGBE_INVALID_FLASH_BANK,
	IXGBE_1ST_FLASH_BANK,
	IXGBE_2ND_FLASH_BANK,
};

/* information for accessing NVM, OROM, and Netlist flash banks */
struct ixgbe_bank_info {
	u32 nvm_ptr;				/* Pointer to 1st NVM bank */
	u32 nvm_size;				/* Size of NVM bank */
	u32 orom_ptr;				/* Pointer to 1st OROM bank */
	u32 orom_size;				/* Size of OROM bank */
	u32 netlist_ptr;			/* Pointer to 1st Netlist bank */
	u32 netlist_size;			/* Size of Netlist bank */
	enum ixgbe_flash_bank nvm_bank;		/* Active NVM bank */
	enum ixgbe_flash_bank orom_bank;	/* Active OROM bank */
	enum ixgbe_flash_bank netlist_bank;	/* Active Netlist bank */
};

/* Flash Chip Information */
struct ixgbe_flash_info {
	struct ixgbe_orom_info orom;		/* Option ROM version info */
	struct ixgbe_nvm_info nvm;		/* NVM version information */
	struct ixgbe_netlist_info netlist;	/* Netlist version info */
	struct ixgbe_bank_info banks;		/* Flash Bank information */
	u16 sr_words;				/* Shadow RAM size in words */
	u32 flash_size;				/* Size of available flash in bytes */
	u8 blank_nvm_mode;			/* is NVM empty (no FW present) */
};

#define IXGBE_NVM_CMD_READ		0x0000000B
#define IXGBE_NVM_CMD_WRITE		0x0000000C

/* NVM Access command */
struct ixgbe_nvm_access_cmd {
	u32 command;		/* NVM command: READ or WRITE */
	u32 offset;			/* Offset to read/write, in bytes */
	u32 data_size;		/* Size of data field, in bytes */
};

/* NVM Access data */
struct ixgbe_nvm_access_data {
	u32 regval;			/* Storage for register value */
};

#endif /* _IXGBE_TYPE_E610_H_ */
