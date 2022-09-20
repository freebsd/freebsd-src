/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright © 2021-2022 Dmitry Salychev
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

#ifndef	_DPAA2_MCP_H
#define	_DPAA2_MCP_H

#include <sys/rman.h>
#include <sys/condvar.h>
#include <sys/mutex.h>

#include "dpaa2_types.h"

/*
 * DPAA2 MC command interface helper routines.
 */

#define DPAA2_PORTAL_TIMEOUT		100000	/* us */
#define DPAA2_MCP_MEM_WIDTH		0x40 /* Minimal size of the MC portal. */
#define DPAA2_MCP_MAX_RESOURCES		1 /* resources per DPMCP: 1 SYS_MEM */

/*
 * Portal flags.
 *
 * TODO: Use the same flags for both MC and software portals.
 */
#define DPAA2_PORTAL_DEF		0x0u
#define DPAA2_PORTAL_NOWAIT_ALLOC	0x2u	/* Do not sleep during init */
#define DPAA2_PORTAL_LOCKED		0x4000u	/* Wait till portal's unlocked */
#define DPAA2_PORTAL_DESTROYED		0x8000u /* Terminate any operations */

/* Command flags. */
#define DPAA2_CMD_DEF			0x0u
#define DPAA2_CMD_HIGH_PRIO		0x80u	/* High priority command */
#define DPAA2_CMD_INTR_DIS		0x100u	/* Disable cmd finished intr */
#define DPAA2_CMD_NOWAIT_ALLOC		0x8000u	/* Do not sleep during init */

/* DPAA2 command return codes. */
#define DPAA2_CMD_STAT_OK		0x0	/* Set by MC on success */
#define DPAA2_CMD_STAT_READY		0x1	/* Ready to be processed */
#define DPAA2_CMD_STAT_AUTH_ERR		0x3	/* Illegal object-portal-icid */
#define DPAA2_CMD_STAT_NO_PRIVILEGE	0x4	/* No privilege */
#define DPAA2_CMD_STAT_DMA_ERR		0x5	/* DMA or I/O error */
#define DPAA2_CMD_STAT_CONFIG_ERR	0x6	/* Invalid/conflicting params */
#define DPAA2_CMD_STAT_TIMEOUT		0x7	/* Command timed out */
#define DPAA2_CMD_STAT_NO_RESOURCE	0x8	/* No DPAA2 resources */
#define DPAA2_CMD_STAT_NO_MEMORY	0x9	/* No memory available */
#define DPAA2_CMD_STAT_BUSY		0xA	/* Device is busy */
#define DPAA2_CMD_STAT_UNSUPPORTED_OP	0xB	/* Unsupported operation */
#define DPAA2_CMD_STAT_INVALID_STATE	0xC	/* Invalid state */
/* Driver-specific return codes. */
#define DPAA2_CMD_STAT_UNKNOWN_OBJ	0xFD	/* Unknown DPAA2 object. */
#define DPAA2_CMD_STAT_EINVAL		0xFE	/* Invalid argument */
#define DPAA2_CMD_STAT_ERR		0xFF	/* General error */

/* Object's memory region flags. */
#define DPAA2_RC_REG_CACHEABLE		0x1	/* Cacheable memory mapping */

#define DPAA2_HW_FLAG_HIGH_PRIO		0x80u
#define DPAA2_SW_FLAG_INTR_DIS		0x01u

#define DPAA2_CMD_PARAMS_N		7u
#define DPAA2_LABEL_SZ			16

/* ------------------------- MNG command IDs -------------------------------- */
#define CMD_MNG_BASE_VERSION	1
#define CMD_MNG_ID_OFFSET	4

#define CMD_MNG(id)	(((id) << CMD_MNG_ID_OFFSET) | CMD_MNG_BASE_VERSION)

#define CMDID_MNG_GET_VER			CMD_MNG(0x831)
#define CMDID_MNG_GET_SOC_VER			CMD_MNG(0x832)
#define CMDID_MNG_GET_CONT_ID			CMD_MNG(0x830)

/* ------------------------- DPRC command IDs ------------------------------- */
#define CMD_RC_BASE_VERSION	1
#define CMD_RC_2ND_VERSION	2
#define CMD_RC_3RD_VERSION	3
#define CMD_RC_ID_OFFSET	4

#define CMD_RC(id)	(((id) << CMD_RC_ID_OFFSET) | CMD_RC_BASE_VERSION)
#define CMD_RC_V2(id)	(((id) << CMD_RC_ID_OFFSET) | CMD_RC_2ND_VERSION)
#define CMD_RC_V3(id)	(((id) << CMD_RC_ID_OFFSET) | CMD_RC_3RD_VERSION)

#define CMDID_RC_OPEN				CMD_RC(0x805)
#define CMDID_RC_CLOSE				CMD_RC(0x800)
#define CMDID_RC_GET_API_VERSION		CMD_RC(0xA05)
#define CMDID_RC_GET_ATTR			CMD_RC(0x004)
#define CMDID_RC_RESET_CONT			CMD_RC(0x005)
#define CMDID_RC_RESET_CONT_V2			CMD_RC_V2(0x005)
#define CMDID_RC_SET_IRQ			CMD_RC(0x010)
#define CMDID_RC_SET_IRQ_ENABLE			CMD_RC(0x012)
#define CMDID_RC_SET_IRQ_MASK			CMD_RC(0x014)
#define CMDID_RC_GET_IRQ_STATUS			CMD_RC(0x016)
#define CMDID_RC_CLEAR_IRQ_STATUS		CMD_RC(0x017)
#define CMDID_RC_GET_CONT_ID			CMD_RC(0x830)
#define CMDID_RC_GET_OBJ_COUNT			CMD_RC(0x159)
#define CMDID_RC_GET_OBJ			CMD_RC(0x15A)
#define CMDID_RC_GET_OBJ_DESC			CMD_RC(0x162)
#define CMDID_RC_GET_OBJ_REG			CMD_RC(0x15E)
#define CMDID_RC_GET_OBJ_REG_V2			CMD_RC_V2(0x15E)
#define CMDID_RC_GET_OBJ_REG_V3			CMD_RC_V3(0x15E)
#define CMDID_RC_SET_OBJ_IRQ			CMD_RC(0x15F)
#define CMDID_RC_GET_CONN			CMD_RC(0x16C)

/* ------------------------- DPIO command IDs ------------------------------- */
#define CMD_IO_BASE_VERSION	1
#define CMD_IO_ID_OFFSET	4

#define CMD_IO(id)	(((id) << CMD_IO_ID_OFFSET) | CMD_IO_BASE_VERSION)

#define CMDID_IO_OPEN				CMD_IO(0x803)
#define CMDID_IO_CLOSE				CMD_IO(0x800)
#define CMDID_IO_ENABLE				CMD_IO(0x002)
#define CMDID_IO_DISABLE			CMD_IO(0x003)
#define CMDID_IO_GET_ATTR			CMD_IO(0x004)
#define CMDID_IO_RESET				CMD_IO(0x005)
#define CMDID_IO_SET_IRQ_ENABLE			CMD_IO(0x012)
#define CMDID_IO_SET_IRQ_MASK			CMD_IO(0x014)
#define CMDID_IO_GET_IRQ_STATUS			CMD_IO(0x016)
#define CMDID_IO_ADD_STATIC_DQ_CHAN		CMD_IO(0x122)

/* ------------------------- DPNI command IDs ------------------------------- */
#define CMD_NI_BASE_VERSION	1
#define CMD_NI_2ND_VERSION	2
#define CMD_NI_4TH_VERSION	4
#define CMD_NI_ID_OFFSET	4

#define CMD_NI(id)	(((id) << CMD_NI_ID_OFFSET) | CMD_NI_BASE_VERSION)
#define CMD_NI_V2(id)	(((id) << CMD_NI_ID_OFFSET) | CMD_NI_2ND_VERSION)
#define CMD_NI_V4(id)	(((id) << CMD_NI_ID_OFFSET) | CMD_NI_4TH_VERSION)

#define CMDID_NI_OPEN				CMD_NI(0x801)
#define CMDID_NI_CLOSE				CMD_NI(0x800)
#define CMDID_NI_ENABLE				CMD_NI(0x002)
#define CMDID_NI_DISABLE			CMD_NI(0x003)
#define CMDID_NI_GET_API_VER			CMD_NI(0xA01)
#define CMDID_NI_RESET				CMD_NI(0x005)
#define CMDID_NI_GET_ATTR			CMD_NI(0x004)
#define CMDID_NI_SET_BUF_LAYOUT			CMD_NI(0x265)
#define CMDID_NI_GET_TX_DATA_OFF		CMD_NI(0x212)
#define CMDID_NI_GET_PORT_MAC_ADDR		CMD_NI(0x263)
#define CMDID_NI_SET_PRIM_MAC_ADDR		CMD_NI(0x224)
#define CMDID_NI_GET_PRIM_MAC_ADDR		CMD_NI(0x225)
#define CMDID_NI_SET_LINK_CFG			CMD_NI(0x21A)
#define CMDID_NI_GET_LINK_CFG			CMD_NI(0x278)
#define CMDID_NI_GET_LINK_STATE			CMD_NI(0x215)
#define CMDID_NI_SET_QOS_TABLE			CMD_NI(0x240)
#define CMDID_NI_CLEAR_QOS_TABLE		CMD_NI(0x243)
#define CMDID_NI_SET_POOLS			CMD_NI(0x200)
#define CMDID_NI_SET_ERR_BEHAVIOR		CMD_NI(0x20B)
#define CMDID_NI_GET_QUEUE			CMD_NI(0x25F)
#define CMDID_NI_SET_QUEUE			CMD_NI(0x260)
#define CMDID_NI_GET_QDID			CMD_NI(0x210)
#define CMDID_NI_ADD_MAC_ADDR			CMD_NI(0x226)
#define CMDID_NI_REMOVE_MAC_ADDR		CMD_NI(0x227)
#define CMDID_NI_CLEAR_MAC_FILTERS		CMD_NI(0x228)
#define CMDID_NI_SET_MFL			CMD_NI(0x216)
#define CMDID_NI_SET_OFFLOAD			CMD_NI(0x26C)
#define CMDID_NI_SET_IRQ_MASK			CMD_NI(0x014)
#define CMDID_NI_SET_IRQ_ENABLE			CMD_NI(0x012)
#define CMDID_NI_GET_IRQ_STATUS			CMD_NI(0x016)
#define CMDID_NI_SET_UNI_PROMISC		CMD_NI(0x222)
#define CMDID_NI_SET_MULTI_PROMISC		CMD_NI(0x220)
#define CMDID_NI_GET_STATISTICS			CMD_NI(0x25D)
#define CMDID_NI_SET_RX_TC_DIST			CMD_NI(0x235)

/* ------------------------- DPBP command IDs ------------------------------- */
#define CMD_BP_BASE_VERSION	1
#define CMD_BP_ID_OFFSET	4

#define CMD_BP(id)	(((id) << CMD_BP_ID_OFFSET) | CMD_BP_BASE_VERSION)

#define CMDID_BP_OPEN				CMD_BP(0x804)
#define CMDID_BP_CLOSE				CMD_BP(0x800)
#define CMDID_BP_ENABLE				CMD_BP(0x002)
#define CMDID_BP_DISABLE			CMD_BP(0x003)
#define CMDID_BP_GET_ATTR			CMD_BP(0x004)
#define CMDID_BP_RESET				CMD_BP(0x005)

/* ------------------------- DPMAC command IDs ------------------------------ */
#define CMD_MAC_BASE_VERSION	1
#define CMD_MAC_2ND_VERSION	2
#define CMD_MAC_ID_OFFSET	4

#define CMD_MAC(id)	(((id) << CMD_MAC_ID_OFFSET) | CMD_MAC_BASE_VERSION)
#define CMD_MAC_V2(id)	(((id) << CMD_MAC_ID_OFFSET) | CMD_MAC_2ND_VERSION)

#define CMDID_MAC_OPEN				CMD_MAC(0x80C)
#define CMDID_MAC_CLOSE				CMD_MAC(0x800)
#define CMDID_MAC_RESET				CMD_MAC(0x005)
#define CMDID_MAC_MDIO_READ			CMD_MAC(0x0C0)
#define CMDID_MAC_MDIO_WRITE			CMD_MAC(0x0C1)
#define CMDID_MAC_GET_ADDR			CMD_MAC(0x0C5)
#define CMDID_MAC_GET_ATTR			CMD_MAC(0x004)
#define CMDID_MAC_SET_LINK_STATE		CMD_MAC_V2(0x0C3)
#define CMDID_MAC_SET_IRQ_MASK			CMD_MAC(0x014)
#define CMDID_MAC_SET_IRQ_ENABLE		CMD_MAC(0x012)
#define CMDID_MAC_GET_IRQ_STATUS		CMD_MAC(0x016)

/* ------------------------- DPCON command IDs ------------------------------ */
#define CMD_CON_BASE_VERSION	1
#define CMD_CON_ID_OFFSET	4

#define CMD_CON(id)	(((id) << CMD_CON_ID_OFFSET) | CMD_CON_BASE_VERSION)

#define CMDID_CON_OPEN				CMD_CON(0x808)
#define CMDID_CON_CLOSE				CMD_CON(0x800)
#define CMDID_CON_ENABLE			CMD_CON(0x002)
#define CMDID_CON_DISABLE			CMD_CON(0x003)
#define CMDID_CON_GET_ATTR			CMD_CON(0x004)
#define CMDID_CON_RESET				CMD_CON(0x005)
#define CMDID_CON_SET_NOTIF			CMD_CON(0x100)

/* ------------------------- DPMCP command IDs ------------------------------ */
#define CMD_MCP_BASE_VERSION	1
#define CMD_MCP_2ND_VERSION	2
#define CMD_MCP_ID_OFFSET	4

#define CMD_MCP(id)	(((id) << CMD_MCP_ID_OFFSET) | CMD_MCP_BASE_VERSION)
#define CMD_MCP_V2(id)	(((id) << CMD_MCP_ID_OFFSET) | CMD_MCP_2ND_VERSION)

#define CMDID_MCP_CREATE			CMD_MCP_V2(0x90B)
#define CMDID_MCP_DESTROY			CMD_MCP(0x98B)
#define CMDID_MCP_OPEN				CMD_MCP(0x80B)
#define CMDID_MCP_CLOSE				CMD_MCP(0x800)
#define CMDID_MCP_RESET				CMD_MCP(0x005)

#define DPAA2_MCP_LOCK(__mcp, __flags) do {		\
	mtx_assert(&(__mcp)->lock, MA_NOTOWNED);	\
	mtx_lock(&(__mcp)->lock);			\
	*(__flags) = (__mcp)->flags;			\
	(__mcp)->flags |= DPAA2_PORTAL_LOCKED;		\
} while (0)

#define DPAA2_MCP_UNLOCK(__mcp) do {		\
	mtx_assert(&(__mcp)->lock, MA_OWNED);	\
	(__mcp)->flags &= ~DPAA2_PORTAL_LOCKED;	\
	mtx_unlock(&(__mcp)->lock);		\
} while (0)

enum dpaa2_rc_region_type {
	DPAA2_RC_REG_MC_PORTAL,
	DPAA2_RC_REG_QBMAN_PORTAL
};

/**
 * @brief Helper object to interact with the MC portal.
 *
 * res:			Unmapped portal's I/O memory.
 * map:			Mapped portal's I/O memory.
 * lock:		Lock to send a command to the portal and wait for the
 *			result.
 * flags:		Current state of the object.
 * rc_api_major:	Major version of the DPRC API.
 * rc_api_minor:	Minor version of the DPRC API.
 */
struct dpaa2_mcp {
	struct resource *res;
	struct resource_map *map;
	struct mtx	lock;
	uint16_t	flags;
	uint16_t	rc_api_major;
	uint16_t	rc_api_minor;
};

/**
 * @brief Command object holds data to be written to the MC portal.
 *
 * header:	8 least significant bytes of the MC portal.
 * params:	Parameters to pass together with the command to MC. Might keep
 *		command execution results.
 *
 * NOTE: 64 bytes.
 */
struct dpaa2_cmd {
	uint64_t	header;
	uint64_t	params[DPAA2_CMD_PARAMS_N];
};

/**
 * @brief Helper object to access fields of the MC command header.
 *
 * srcid:	The SoC architected source ID of the submitter. This field is
 *		reserved and cannot be written by the driver.
 * flags_hw:	Bits from 8 to 15 of the command header. Most of them are
 *		reserved at the moment.
 * status:	Command ready/status. This field is used as the handshake field
 *		between MC and the driver. MC reports command completion with
 *		success/error codes in this field.
 * flags_sw:	...
 * token:	...
 * cmdid:	...
 *
 * NOTE: 8 bytes.
 */
struct dpaa2_cmd_header {
	uint8_t		srcid;
	uint8_t		flags_hw;
	uint8_t		status;
	uint8_t		flags_sw;
	uint16_t	token;
	uint16_t	cmdid;
} __packed;

/**
 * @brief Information about DPAA2 object.
 *
 * id:		ID of a logical object resource.
 * vendor:	Object vendor identifier.
 * irq_count:	Number of interrupts supported by the object.
 * reg_count:	Number of mappable regions supported by the object.
 * state:	Object state (combination of states).
 * ver_major:	Major version of the object.
 * ver_minor:	Minor version of the object.
 * flags:	Object attributes flags.
 * type:	...
 * label:	...
 */
struct dpaa2_obj {
	uint32_t	id;
	uint16_t	vendor;
	uint8_t		irq_count;
	uint8_t		reg_count;
	uint32_t	state;
	uint16_t	ver_major;
	uint16_t	ver_minor;
	uint16_t	flags;
	uint8_t		label[DPAA2_LABEL_SZ];
	enum dpaa2_dev_type type;
};

/**
 * @brief Attributes of the DPRC object.
 *
 * cont_id:	Container ID.
 * portal_id:	Container's portal ID.
 * options:	Container's options as set at container's creation.
 * icid:	Container's isolation context ID.
 */
struct dpaa2_rc_attr {
	uint32_t	cont_id;
	uint32_t	portal_id;
	uint32_t	options;
	uint32_t	icid;
};

/**
 * @brief Description of the object's memory region.
 *
 * base_paddr:	Region base physical address.
 * base_offset:	Region base offset.
 * size:	Region size (in bytes).
 * flags:	Region flags (cacheable, etc.)
 * type:	Type of a software portal this region belongs to.
 */
struct dpaa2_rc_obj_region {
	uint64_t	base_paddr;
	uint64_t	base_offset;
	uint32_t	size;
	uint32_t	flags;
	enum dpaa2_rc_region_type type;
};

/**
 * @brief DPAA2 endpoint descriptor.
 *
 * obj_id:	Endpoint object ID.
 * if_id:	Interface ID; for endpoints with multiple interfaces
 *		(DPSW, DPDMUX), 0 - otherwise.
 * type:	Endpoint object type, null-terminated string.
 */
struct dpaa2_ep_desc {
	uint32_t	obj_id;
	uint32_t	if_id;
	enum dpaa2_dev_type type;
};

/**
 * @brief Configuration of the channel data availability notification (CDAN).
 *
 * qman_ctx:	Context value provided with each CDAN message.
 * dpio_id:	DPIO object ID configured with a notification channel.
 * prior:	Priority selection within the DPIO channel; valid values
 *		are 0-7, depending on the number of priorities in that channel.
 */
struct dpaa2_con_notif_cfg {
	uint64_t	qman_ctx;
	uint32_t	dpio_id;
	uint8_t		prior;
};

/**
 * @brief Attributes of the DPMCP object.
 *
 * id:		 DPMCP object ID.
 * options:	 Options of the MC portal (disabled high-prio commands, etc.).
 */
struct dpaa2_mcp_attr {
	uint32_t		id;
	uint32_t		options;
};

/**
 * @brief Software context for the DPAA2 MC portal.
 */
struct dpaa2_mcp_softc {
	device_t		 dev;
	struct dpaa2_mcp_attr	 attr;

	struct resource 	*res[DPAA2_MCP_MAX_RESOURCES];
	struct resource_map	 map[DPAA2_MCP_MAX_RESOURCES];
};

int	dpaa2_mcp_init_portal(struct dpaa2_mcp **mcp, struct resource *res,
	    struct resource_map *map, uint16_t flags);
int	dpaa2_mcp_init_command(struct dpaa2_cmd **cmd, uint16_t flags);
void	dpaa2_mcp_free_portal(struct dpaa2_mcp *mcp);
void	dpaa2_mcp_free_command(struct dpaa2_cmd *cmd);

/* to quickly update command token */
struct dpaa2_cmd *dpaa2_mcp_tk(struct dpaa2_cmd *cmd, uint16_t token);
/* to quickly update command flags */
struct dpaa2_cmd *dpaa2_mcp_f(struct dpaa2_cmd *cmd, uint16_t flags);

#endif /* _DPAA2_MCP_H */
