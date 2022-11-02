/*-
 * Copyright (c) 2018 VMware, Inc.
 *
 * SPDX-License-Identifier: (BSD-2-Clause OR GPL-2.0)
 *
 * $FreeBSD$
 */

#ifndef _PVSCSI_H_
#define _PVSCSI_H_

#define	MASK(v)	((1 << (v)) - 1)

#define	PCI_VENDOR_ID_VMWARE		0x15ad
#define	PCI_DEVICE_ID_VMWARE_PVSCSI	0x07c0

enum pvscsi_reg_offset {
	PVSCSI_REG_OFFSET_COMMAND		= 0x0000,
	PVSCSI_REG_OFFSET_COMMAND_DATA		= 0x0004,
	PVSCSI_REG_OFFSET_COMMAND_STATUS	= 0x0008,
	PVSCSI_REG_OFFSET_LAST_STS_0		= 0x0100,
	PVSCSI_REG_OFFSET_LAST_STS_1		= 0x0104,
	PVSCSI_REG_OFFSET_LAST_STS_2		= 0x0108,
	PVSCSI_REG_OFFSET_LAST_STS_3		= 0x010c,
	PVSCSI_REG_OFFSET_INTR_STATUS		= 0x100c,
	PVSCSI_REG_OFFSET_INTR_MASK		= 0x2010,
	PVSCSI_REG_OFFSET_KICK_NON_RW_IO	= 0x3014,
	PVSCSI_REG_OFFSET_DEBUG			= 0x3018,
	PVSCSI_REG_OFFSET_KICK_RW_IO		= 0x4018,
};

enum pvscsi_commands {
	PVSCSI_CMD_FIRST			= 0,

	PVSCSI_CMD_ADAPTER_RESET		= 1,
	PVSCSI_CMD_ISSUE_SCSI			= 2,
	PVSCSI_CMD_SETUP_RINGS			= 3,
	PVSCSI_CMD_RESET_BUS			= 4,
	PVSCSI_CMD_RESET_DEVICE			= 5,
	PVSCSI_CMD_ABORT_CMD			= 6,
	PVSCSI_CMD_CONFIG			= 7,
	PVSCSI_CMD_SETUP_MSG_RING		= 8,
	PVSCSI_CMD_DEVICE_UNPLUG		= 9,
	PVSCSI_CMD_SETUP_REQCALLTHRESHOLD	= 10,
	PVSCSI_CMD_GET_MAX_TARGETS		= 11,

	PVSCSI_CMD_LAST				= 12,
};

struct pvscsi_cmd_desc_reset_device {
	uint32_t	target;
	uint8_t		lun[8];
};

struct pvscsi_cmd_desc_abort_cmd {
	uint64_t	context;
	uint32_t	target;
	uint32_t	pad;
};

#define	PVSCSI_SETUP_RINGS_MAX_NUM_PAGES	32
#define	PVSCSI_SETUP_MSG_RING_MAX_NUM_PAGES	16

struct pvscsi_cmd_desc_setup_rings {
	uint32_t	req_ring_num_pages;
	uint32_t	cmp_ring_num_pages;
	uint64_t	rings_state_ppn;
	uint64_t	req_ring_ppns[PVSCSI_SETUP_RINGS_MAX_NUM_PAGES];
	uint64_t	cmp_ring_ppns[PVSCSI_SETUP_RINGS_MAX_NUM_PAGES];
};

struct pvscsi_cmd_desc_setup_msg_ring {
	uint32_t	num_pages;
	uint32_t	pad_;
	uint64_t	ring_ppns[PVSCSI_SETUP_MSG_RING_MAX_NUM_PAGES];
};

struct pvscsi_rings_state {
	uint32_t	req_prod_idx;
	uint32_t	req_cons_idx;
	uint32_t	req_num_entries_log2;
	uint32_t	cmp_prod_idx;
	uint32_t	cmp_cons_idx;
	uint32_t	cmp_num_entries_log2;
	uint32_t	req_call_threshold;
	uint8_t		_pad[100];
	uint32_t	msg_prod_idx;
	uint32_t	msg_cons_idx;
	uint32_t	msg_num_entries_log2;
};

#define	PVSCSI_FLAG_CMD_WITH_SG_LIST	(1 << 0)
#define	PVSCSI_FLAG_CMD_OUT_OF_BAND_CDB	(1 << 1)
#define	PVSCSI_FLAG_CMD_DIR_NONE	(1 << 2)
#define	PVSCSI_FLAG_CMD_DIR_TOHOST	(1 << 3)
#define	PVSCSI_FLAG_CMD_DIR_TODEVICE	(1 << 4)

#define	PVSCSI_FLAG_RESERVED_MASK	(~MASK(5))

#define	PVSCSI_INTR_CMPL_0	(1 << 0)
#define	PVSCSI_INTR_CMPL_1	(1 << 1)
#define	PVSCSI_INTR_CMPL_MASK	MASK(2)

#define	PVSCSI_INTR_MSG_0	(1 << 2)
#define	PVSCSI_INTR_MSG_1	(1 << 3)
#define	PVSCSI_INTR_MSG_MASK	(MASK(2) << 2)

#define	PVSCSI_INTR_ALL_SUPPORTED	MASK(4)

struct pvscsi_ring_req_desc {
	uint64_t	context;
	uint64_t	data_addr;
	uint64_t	data_len;
	uint64_t	sense_addr;
	uint32_t	sense_len;
	uint32_t	flags;
	uint8_t		cdb[16];
	uint8_t		cdb_len;
	uint8_t		lun[8];
	uint8_t		tag;
	uint8_t		bus;
	uint8_t		target;
	uint16_t	vcpu_hint;
	uint8_t		unused[58];
};

struct pvscsi_ring_cmp_desc {
	uint64_t	context;
	uint64_t	data_len;
	uint32_t	sense_len;
	uint16_t	host_status;
	uint16_t	scsi_status;
	uint32_t	_pad[2];
};

#define	PVSCSI_MAX_SG_ENTRIES_PER_SEGMENT	128
#define	PVSCSI_MAX_NUM_SG_SEGMENTS		128
#define	PVSCSI_SGE_FLAG_CHAIN_ELEMENT		(1 << 0)

struct pvscsi_sg_element {
	uint64_t	addr;
	uint32_t	length;
	uint32_t	flags;
};

enum pvscsi_msg_type {
	PVSCSI_MSG_DEV_ADDED	= 0,
	PVSCSI_MSG_DEV_REMOVED	= 1,
	PVSCSI_MSG_LAST	= 2,
};

struct pvscsi_ring_msg_desc {
	uint32_t	type;
	uint32_t	args[31];
};

struct pvscsi_ring_msg_dev_status_changed {
	uint32_t	type;
	uint32_t	bus;
	uint32_t	target;
	uint8_t		lun[8];
	uint32_t	pad[27];
};

struct pvscsi_cmd_desc_setup_req_call {
	uint32_t	enable;
};

#define	PVSCSI_MAX_NUM_PAGES_REQ_RING	PVSCSI_SETUP_RINGS_MAX_NUM_PAGES
#define	PVSCSI_MAX_NUM_PAGES_CMP_RING	PVSCSI_SETUP_RINGS_MAX_NUM_PAGES
#define	PVSCSI_MAX_NUM_PAGES_MSG_RING	PVSCSI_SETUP_MSG_RING_MAX_NUM_PAGES

#define	PVSCSI_MAX_NUM_REQ_ENTRIES_PER_PAGE \
	(PAGE_SIZE / sizeof(struct pvscsi_ring_req_desc))
#define	PVSCSI_MAX_NUM_CMP_ENTRIES_PER_PAGE \
	(PAGE_SIZE / sizeof(struct pvscs_ring_cmp_desc))
#define	PVSCSI_MAX_NUM_MSG_ENTRIES_PER_PAGE \
	(PAGE_SIZE / sizeof(struct pvscsi_ring_msg_desc))

#define	PVSCSI_MAX_REQ_QUEUE_DEPTH \
	(PVSCSI_MAX_NUM_PAGES_REQ_RING * PVSCSI_MAX_NUM_REQ_ENTRIES_PER_PAGE)
#define	PVSCSI_MAX_CMP_QUEUE_DEPTH \
	(PVSCSI_MAX_NUM_PAGES_CMP_RING * PVSCSI_MAX_NUM_CMP_ENTRIES_PER_PAGE)
#define	PVSCSI_MAX_QUEUE_DEPTH \
	MAX(PVSCSI_MAX_REQ_QUEUE_DEPTH, PVSCSI_MAX_CMP_QUEUE_DEPTH)

enum pvscsi_host_status {
	BTSTAT_SUCCESS		= 0x00,
	BTSTAT_LINKED_COMMAND_COMPLETED			= 0x0a,
	BTSTAT_LINKED_COMMAND_COMPLETED_WITH_FLAG	= 0x0b,
	BTSTAT_DATA_UNDERRUN	= 0x0c,
	BTSTAT_SELTIMEO		= 0x11,
	BTSTAT_DATARUN		= 0x12,
	BTSTAT_BUSFREE		= 0x13,
	BTSTAT_INVPHASE		= 0x14,
	BTSTAT_INVCODE		= 0x15,
	BTSTAT_INVOPCODE	= 0x16,
	BTSTAT_LUNMISMATCH	= 0x17,
	BTSTAT_INVPARAM		= 0x1a,
	BTSTAT_SENSFAILED	= 0x1b,
	BTSTAT_TAGREJECT	= 0x1c,
	BTSTAT_BADMSG		= 0x1d,
	BTSTAT_HAHARDWARE	= 0x20,
	BTSTAT_NORESPONSE	= 0x21,
	BTSTAT_SENTRST		= 0x22,
	BTSTAT_RECVRST		= 0x23,
	BTSTAT_DISCONNECT	= 0x24,
	BTSTAT_BUSRESET		= 0x25,
	BTSTAT_ABORTQUEUE	= 0x26,
	BTSTAT_HASOFTWARE	= 0x27,
	BTSTAT_HATIMEOUT	= 0x30,
	BTSTAT_SCSIPARITY	= 0x34,
};

#endif /* !_PVSCSI_H_ */
