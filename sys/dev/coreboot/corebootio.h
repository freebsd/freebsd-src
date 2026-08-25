/*
 * Copyright (c) 2026 Abdelkader Boudih <freebsd@seuros.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * coreboot(4) userland-visible definitions.
 *
 * This header is safe to include from both kernel and userland code.
 * It contains the /dev/cbmem ioctl interface, CBMEM ID constants,
 * and timestamp structures needed by the cbmem(8) utility.
 */

#ifndef _DEV_COREBOOT_COREBOOT_IOCTL_H_
#define _DEV_COREBOOT_COREBOOT_IOCTL_H_

#include <sys/types.h>
#include <sys/ioccom.h>

/*
 * CBMEM ID constants (from coreboot's BSD-3-Clause cbmem_id.h).
 * Used for human-readable CBMEM entry names in sysctl and ioctl output.
 */
#define CBMEM_ID_ACPI			0x41435049
#define CBMEM_ID_ACPI_GNVS		0x474e5653
#define CBMEM_ID_AFTER_CAR		0xc4787a93
#define CBMEM_ID_CBTABLE		0x43425442
#define CBMEM_ID_CBTABLE_FWD		0x43425443
#define CBMEM_ID_CBFS_RO_MCACHE	0x524d5346
#define CBMEM_ID_CBFS_RW_MCACHE	0x574d5346
#define CBMEM_ID_CONSOLE		0x434f4e53
#define CBMEM_ID_ELOG			0x454c4f47
#define CBMEM_ID_FMAP			0x464d4150
#define CBMEM_ID_FREESPACE		0x46524545
#define CBMEM_ID_FSP_RESERVED_MEMORY	0x46535052
#define CBMEM_ID_FSP_RUNTIME		0x52505346
#define CBMEM_ID_FSPM_VERSION		0x56505346
#define CBMEM_ID_IGD_OPREGION		0x4f444749
#define CBMEM_ID_IMD_ROOT		0xff4017ff
#define CBMEM_ID_IMD_SMALL		0x53a11439
#define CBMEM_ID_MEMINFO		0x494d454d
#define CBMEM_ID_MPTABLE		0x534d5054
#define CBMEM_ID_MRCDATA		0x4d524344
#define CBMEM_ID_PIRQ			0x49525154
#define CBMEM_ID_POWER_STATE		0x50535454
#define CBMEM_ID_RAM_OOPS		0x05430095
#define CBMEM_ID_RAMSTAGE		0x9a357a9e
#define CBMEM_ID_REFCODE		0x04efc0de
#define CBMEM_ID_RESUME			0x5245534d
#define CBMEM_ID_ROMSTAGE_INFO		0x47545352
#define CBMEM_ID_ROMSTAGE_RAM_STACK	0x90357ac4
#define CBMEM_ID_ROOT			0xff4007ff
#define CBMEM_ID_SMBIOS			0x534d4254
#define CBMEM_ID_SMM_COMBUFFER		0x53534d32
#define CBMEM_ID_SMM_SAVE_SPACE		0x07e9acee
#define CBMEM_ID_TIMESTAMP		0x54494d45
#define CBMEM_ID_TPM_CB_LOG		0x54435041
#define CBMEM_ID_VBOOT_WORKBUF		0x78007343
#define CBMEM_ID_VPD			0x56504420

#define CB_MAX_CBMEM_ENTRIES	64

/*
 * /dev/cbmem ioctl interface
 */
struct cbmem_entry_info {
	uint32_t	id;
	uint64_t	address;
	uint32_t	size;
	char		name[16];
};

struct cbmem_list {
	uint32_t	count;
	struct cbmem_entry_info entries[CB_MAX_CBMEM_ENTRIES];
};

struct cbmem_read_req {
	uint32_t	id;
	uint64_t	offset;
	uint32_t	size;
	void		*buffer;
};

#define CBMEM_IOC_LIST	_IOR('C', 1, struct cbmem_list)
#define CBMEM_IOC_READ	_IOWR('C', 2, struct cbmem_read_req)

/*
 * Timestamp structures - stored in a CBMEM region.
 * Used by both the kernel driver and cbmem(8) utility.
 */
struct cb_timestamp_entry {
	uint32_t	entry_id;
	int64_t		entry_stamp;
} __packed;

struct cb_timestamp_table {
	uint64_t	base_time;
	uint16_t	max_entries;
	uint16_t	tick_freq_mhz;
	uint32_t	num_entries;
	struct cb_timestamp_entry entries[];
} __packed;

/*
 * TPM CB log structures - stored in a CBMEM region.
 * Used by both the kernel driver and cbmem(8) utility.
 */
struct tpm_cb_log_entry {
	uint32_t	pcr;
	uint32_t	event_type;
	uint8_t		digest[20];	/* SHA-1 */
	uint32_t	data_len;
} __packed;

struct tpm_cb_log_table {
	uint16_t	max_entries;
	uint16_t	num_entries;
	uint32_t	entry_size;
} __packed;

#endif /* _DEV_COREBOOT_COREBOOT_IOCTL_H_ */
