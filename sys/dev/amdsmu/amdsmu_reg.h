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

#endif /* _AMDSMU_REG_H_ */
