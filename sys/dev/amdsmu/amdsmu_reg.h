/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 The FreeBSD Foundation
 *
 * This software was developed by Aymeric Wibo <obiwac@freebsd.org>
 * under sponsorship from the FreeBSD Foundation.
 */
#ifndef _AMDSMU_REG_H_
#define	_AMDSMU_REG_H_

#include <sys/types.h>

/*
 * TODO These are in common with amdtemp; should we find a way to factor these
 * out?  Also, there are way more of these.  I couldn't find a centralized place
 * which lists them though.
 */
#define PCI_DEVICEID_AMD_REMBRANDT_ROOT		0x14B5
#define PCI_DEVICEID_AMD_PHOENIX_ROOT		0x14E8
#define PCI_DEVICEID_AMD_STRIX_POINT_ROOT	0x14A4

#define SMU_INDEX_ADDRESS	0xB8
#define SMU_INDEX_DATA		0xBC

#define SMU_PHYSBASE_ADDR_LO	0x13B102E8
#define SMU_PHYSBASE_ADDR_HI	0x13B102EC

#define SMU_MEM_SIZE		0x1000
#define SMU_REG_SPACE_OFF	0x10000

#define SMU_REG_MESSAGE		0x538
#define SMU_REG_RESPONSE	0x980
#define SMU_REG_ARGUMENT	0x9BC
#define SMU_REG_IDLEMASK	0xD14

enum amdsmu_res {
	SMU_RES_WAIT		= 0x00,
	SMU_RES_OK		= 0x01,
	SMU_RES_REJECT_BUSY	= 0xFC,
	SMU_RES_REJECT_PREREQ	= 0xFD,
	SMU_RES_UNKNOWN		= 0xFE,
	SMU_RES_FAILED		= 0xFF,
};

enum amdsmu_msg {
	SMU_MSG_GETSMUVERSION		= 0x02,
	SMU_MSG_LOG_GETDRAM_ADDR_HI	= 0x04,
	SMU_MSG_LOG_GETDRAM_ADDR_LO	= 0x05,
	SMU_MSG_LOG_START		= 0x06,
	SMU_MSG_LOG_RESET		= 0x07,
	SMU_MSG_LOG_DUMP_DATA		= 0x08,
	SMU_MSG_GET_SUP_CONSTRAINTS	= 0x09,
};

/* XXX Copied from Linux struct smu_metrics. */
struct amdsmu_metrics {
	uint32_t table_version;
	uint32_t hint_count;
	uint32_t s0i3_last_entry_status;
	uint32_t time_last_in_s0i2;
	uint64_t time_last_entering_s0i3;
	uint64_t total_time_entering_s0i3;
	uint64_t time_last_resuming;
	uint64_t total_time_resuming;
	uint64_t time_last_in_s0i3;
	uint64_t total_time_in_s0i3;
	uint64_t time_last_in_sw_drips;
	uint64_t total_time_in_sw_drips;
	/*
	 * This is how long each IP block was active for (us), i.e., blocking
	 * entry to S0i3.  In Linux, these are called "timecondition_notmet_*".
	 *
	 * XXX Total active time for IP blocks seems to be buggy and reporting
	 * garbage (at least on Phoenix), so it's disabled for now.  The last
	 * active time for the USB4_0 IP block also seems to be buggy.
	 */
	uint64_t ip_block_last_active_time[32];
#ifdef IP_BLOCK_TOTAL_ACTIVE_TIME
	uint64_t ip_block_total_active_time[32];
#endif
} __attribute__((packed));

#endif /* _AMDSMU_REG_H_ */
