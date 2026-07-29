/*
 * Copyright (c) 2026 Abdelkader Boudih <freebsd@seuros.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * coreboot(4) driver for FreeBSD
 *
 * Structures and constants derived from the coreboot table specification.
 */

#ifndef _DEV_COREBOOT_COREBOOT_H_
#define _DEV_COREBOOT_COREBOOT_H_

#include <sys/types.h>
#include <sys/conf.h>
#include <sys/sysctl.h>
#include <sys/bus.h>

#include <dev/coreboot/corebootio.h>

#define CB_HEADER_SIGNATURE	"LBIO"
#define CB_HEADER_SIG_LEN	4

/*
 * Memory scan range for coreboot table discovery.
 * Low memory (0x0–0x1000) contains a forward pointer to the real table.
 */
#define CB_SCAN_LOW_START	0x00000000
#define CB_SCAN_LOW_END		0x00001000
#define CB_SCAN_LOW_STEP	16

/*
 * Defensive limits for parsing untrusted firmware-provided lengths.
 */
#define CB_TABLE_ALIGN		4
#define CB_MAX_HEADER_BYTES	4096
#define CB_MAX_TABLE_BYTES	(1024 * 1024)
#define CB_MAX_TABLE_MAP_BYTES	(CB_MAX_HEADER_BYTES + CB_MAX_TABLE_BYTES)
#define CB_MAX_CONSOLE_BYTES	(1024 * 1024)

/*
 * Coreboot table record tags - only tags we actually parse.
 * Full enum preserved for forward compatibility (unknown tags are skipped).
 */
enum cb_tag {
	CB_TAG_UNUSED			= 0x0000,
	CB_TAG_MAINBOARD		= 0x0003,
	CB_TAG_VERSION			= 0x0004,
	CB_TAG_EXTRA_VERSION		= 0x0005,
	CB_TAG_BUILD			= 0x0006,
	CB_TAG_COMPILE_TIME		= 0x0007,
	CB_TAG_COMPILER			= 0x000b,
	CB_TAG_SERIAL			= 0x000f,
	CB_TAG_CONSOLE			= 0x0010,
	CB_TAG_FORWARD			= 0x0011,
	CB_TAG_FRAMEBUFFER		= 0x0012,
	CB_TAG_GPIO			= 0x0013,
	CB_TAG_TIMESTAMPS		= 0x0016,
	CB_TAG_CBMEM_CONSOLE		= 0x0017,
	CB_TAG_ACPI_GNVS		= 0x0024,
	CB_TAG_VERSION_TIMESTAMP	= 0x0026,
	CB_TAG_WIFI_CALIBRATION		= 0x0027,
	CB_TAG_SPI_FLASH		= 0x0029,
	CB_TAG_SERIALNO			= 0x002a,
	CB_TAG_VPD			= 0x002c,
	CB_TAG_BOOT_MEDIA_PARAMS	= 0x0030,
	CB_TAG_CBMEM_ENTRY		= 0x0031,
	CB_TAG_TSC_INFO			= 0x0032,
	CB_TAG_MAC_ADDRS		= 0x0033,
	CB_TAG_VBOOT_WORKBUF		= 0x0034,
	CB_TAG_MMC_INFO			= 0x0035,
	CB_TAG_TPM_CB_LOG		= 0x0036,
	CB_TAG_FMAP			= 0x0037,
	CB_TAG_PLATFORM_BLOB_VERSION	= 0x0038,
	CB_TAG_SMMSTOREV2		= 0x0039,
	CB_TAG_TPM_PPI_HANDOFF		= 0x003a,
	CB_TAG_BOARD_CONFIG		= 0x0040,
	CB_TAG_ACPI_CNVS		= 0x0041,
	CB_TAG_TYPE_C_INFO		= 0x0042,
	CB_TAG_ACPI_RSDP		= 0x0043,
	CB_TAG_PCIE			= 0x0044,
	CB_TAG_ROOT_BRIDGE_INFO		= 0x0048,
};

/*
 * Coreboot table header - located at a physical address found by scanning
 * low memory for the "LBIO" signature.
 */
struct cb_header {
	uint8_t		signature[CB_HEADER_SIG_LEN];
	uint32_t	header_bytes;
	uint32_t	header_checksum;
	uint32_t	table_bytes;
	uint32_t	table_checksum;
	uint32_t	table_entries;
} __packed;

/*
 * Generic record header - every table entry starts with this.
 */
struct cb_record {
	uint32_t	tag;
	uint32_t	size;
} __packed;

/*
 * CB_TAG_FORWARD - pointer to the real table in high memory.
 */
struct cb_forward {
	uint32_t	tag;
	uint32_t	size;
	uint64_t	forward;
} __packed;

/*
 * CB_TAG_MAINBOARD - board vendor and part number.
 * Strings are packed after the struct, indexed by vendor_idx and part_idx.
 */
struct cb_mainboard {
	uint32_t	tag;
	uint32_t	size;
	uint8_t		vendor_idx;
	uint8_t		part_idx;
	uint8_t		strings[];
} __packed;

/*
 * Variable-length string record - used by VERSION, BUILD, COMPILE_*, etc.
 */
struct cb_string {
	uint32_t	tag;
	uint32_t	size;
	uint8_t		string[];
} __packed;

/*
 * CB_TAG_SERIAL - serial port configuration.
 */
struct cb_serial {
	uint32_t	tag;
	uint32_t	size;
	uint32_t	type;
	uint32_t	baseaddr;
	uint32_t	baud;
	uint32_t	regwidth;
	uint32_t	input_hertz;
} __packed;

/*
 * CB_TAG_CBMEM_CONSOLE - pointer to firmware console ring buffer.
 */
struct cb_cbmem_ref {
	uint32_t	tag;
	uint32_t	size;
	uint64_t	cbmem_addr;
} __packed;

/*
 * CB_TAG_CBMEM_ENTRY - one per CBMEM region.
 */
struct cb_cbmem_entry {
	uint32_t	tag;
	uint32_t	size;
	uint64_t	address;
	uint32_t	entry_size;
	uint32_t	id;
} __packed;

/*
 * CB_TAG_TSC_INFO - TSC frequency.
 */
struct cb_tsc_info {
	uint32_t	tag;
	uint32_t	size;
	uint32_t	freq_khz;
} __packed;

/*
 * CB_TAG_VERSION_TIMESTAMP - build version timestamp.
 * timestamp is Unix time in seconds (seconds since 1970-01-01 UTC).
 */
struct cb_version_timestamp {
	uint32_t	tag;
	uint32_t	size;
	uint32_t	timestamp;
} __packed;

/*
 * CB_TAG_BOARD_CONFIG - board identification and firmware config.
 */
struct cb_board_config {
	uint32_t	tag;
	uint32_t	size;
	uint64_t	fw_config;
	uint32_t	board_id;
	uint32_t	ram_code;
	uint32_t	sku_id;
} __packed;

/*
 * CB_TAG_BOOT_MEDIA_PARAMS - offsets and sizes on boot media.
 */
struct cb_boot_media_params {
	uint32_t	tag;
	uint32_t	size;
	uint64_t	fmap_offset;
	uint64_t	cbfs_offset;
	uint64_t	cbfs_size;
	uint64_t	boot_media_size;
} __packed;

/*
 * CB_TAG_MMC_INFO - early eMMC/MMC status.
 */
struct cb_mmc_info {
	uint32_t	tag;
	uint32_t	size;
	int32_t		early_cmd1_status;
} __packed;

/*
 * CB_TAG_PCIE - PCIe controller base address.
 */
struct cb_pcie {
	uint32_t	tag;
	uint32_t	size;
	uint64_t	ctrl_base;
} __packed;

/*
 * CB_TAG_MAC_ADDRS - factory-provisioned MAC addresses.
 */
struct cb_mac_address {
	uint8_t		mac_addr[6];
	uint8_t		pad[2];
} __packed;

struct cb_macs {
	uint32_t	tag;
	uint32_t	size;
	uint32_t	count;
	struct cb_mac_address entries[];
} __packed;

/*
 * CB_TAG_SPI_FLASH - SPI flash chip parameters.
 */
struct cb_flash_mmap_window {
	uint32_t	flash_base;
	uint32_t	host_base;
	uint32_t	size;
} __packed;

struct cb_spi_flash {
	uint32_t	tag;
	uint32_t	size;
	uint32_t	flash_size;
	uint32_t	sector_size;
	uint8_t		erase_cmd;
	uint8_t		flags;
	uint16_t	reserved;
	uint32_t	mmap_count;
	struct cb_flash_mmap_window mmap_table[];
} __packed;

/*
 * CB_TAG_CONSOLE - firmware console type.
 */
struct cb_console {
	uint32_t	tag;
	uint32_t	size;
	uint16_t	type;
	uint8_t		pad[2];
} __packed;

#define CB_CONSOLE_SERIAL8250		0
#define CB_CONSOLE_VGA			1	/* obsolete */
#define CB_CONSOLE_EHCI			5
#define CB_CONSOLE_SERIAL8250MEM	6

/*
 * CB_TAG_FRAMEBUFFER - linear framebuffer info.
 * The orientation, flags, and pad fields were added later.
 * Minimum record size is up to reserved_mask_size (29 bytes).
 */
#define CB_FRAMEBUFFER_MIN_SIZE	29
struct cb_framebuffer {
	uint32_t	tag;
	uint32_t	size;
	uint64_t	physical_address;
	uint32_t	x_resolution;
	uint32_t	y_resolution;
	uint32_t	bytes_per_line;
	uint8_t		bits_per_pixel;
	uint8_t		red_mask_pos;
	uint8_t		red_mask_size;
	uint8_t		green_mask_pos;
	uint8_t		green_mask_size;
	uint8_t		blue_mask_pos;
	uint8_t		blue_mask_size;
	uint8_t		reserved_mask_pos;
	uint8_t		reserved_mask_size;
	uint8_t		orientation;
	uint8_t		flags;
	uint8_t		pad;
} __packed;

/*
 * CB_TAG_GPIO - GPIO pin states.
 */
struct cb_gpio {
	uint32_t	port;
	uint32_t	polarity;
	uint32_t	value;
	uint8_t		name[16];
} __packed;

struct cb_gpios {
	uint32_t	tag;
	uint32_t	size;
	uint32_t	count;
	struct cb_gpio	entries[];
} __packed;

/*
 * CB_TAG_TPM_PPI_HANDOFF - TPM Physical Presence Interface handoff.
 */
struct cb_tpm_ppi {
	uint32_t	tag;
	uint32_t	size;
	uint32_t	ppi_address;
	uint8_t		tpm_version;	/* 1=TPM1.2, 2=TPM2.0 */
	uint8_t		ppi_version;	/* BCD encoded */
	uint8_t		pad[2];
} __packed;

/*
 * CB_TAG_SMMSTOREV2 - SMM-based variable store configuration.
 *
 * The mmap_addr field was added after the initial implementation.
 * Older coreboot emits a shorter record (32 bytes) without it.
 * Consumers must check rec->size to detect whether mmap_addr is present.
 */
#define CB_SMMSTOREV2_BASE_SIZE	32	/* size without mmap_addr */
struct cb_smmstorev2 {
	uint32_t	tag;
	uint32_t	size;
	uint32_t	num_blocks;
	uint32_t	block_size;
	uint32_t	mmap_addr_lo;	/* deprecated 32-bit address */
	uint32_t	com_buffer;
	uint32_t	com_buffer_size;
	uint8_t		apm_cmd;
	uint8_t		unused[3];
	/* Fields below only present if size > CB_SMMSTOREV2_BASE_SIZE */
	uint64_t	mmap_addr;	/* 64-bit address (preferred) */
} __packed;

/*
 * CB_TAG_ACPI_RSDP - ACPI Root System Description Pointer address.
 */
struct cb_acpi_rsdp {
	uint32_t	tag;
	uint32_t	size;
	uint64_t	rsdp_pointer;
} __packed;

/*
 * CBMEM console ring buffer (in-memory structure at cbmem_addr).
 * Bit 31 of cursor indicates overflow (ring has wrapped).
 */
struct cbmem_console {
	uint32_t	size;
	uint32_t	cursor;
	uint8_t		body[];
} __packed;

#define CBMEM_CONSOLE_CURSOR_MASK	((1 << 28) - 1)
#define CBMEM_CONSOLE_OVERFLOW		(1 << 31)

#define CB_MAX_MAC_ADDRS	8
#define CB_MAX_GPIOS		32

/*
 * Driver softc
 */
struct coreboot_softc {
	device_t		dev;

	vm_paddr_t		table_paddr;
	vm_size_t		table_size;
	void			*table_vaddr;

	char			version[64];
	char			build[64];
	char			compile_time[64];
	char			compiler[128];
	char			extra_version[64];
	char			platform_blob_version[64];
	char			serialno[64];
	uint32_t		version_timestamp;
	int			has_version_timestamp;

	char			mb_vendor[64];
	char			mb_part[64];

	uint32_t		serial_baseaddr;
	uint32_t		serial_baud;
	uint32_t		serial_regwidth;
	int			has_serial;

	uint32_t		tsc_freq_khz;
	int			has_tsc_info;

	uint64_t		pcie_ctrl_base;
	int			has_pcie;

	uint64_t		fmap_offset;
	uint64_t		cbfs_offset;
	uint64_t		cbfs_size;
	uint64_t		boot_media_size;
	int			has_boot_media;

	int32_t			mmc_early_cmd1_status;
	int			has_mmc_info;

	vm_paddr_t		console_paddr;
	vm_size_t		console_size;
	uint32_t		console_data_size;
	struct cbmem_console	*console_vaddr;
	int			has_console;
	struct cdev		*console_cdev;

	uint32_t		cbmem_count;
	struct cbmem_entry_info	cbmem_entries[CB_MAX_CBMEM_ENTRIES];
	struct cdev		*cbmem_cdev;

	uint64_t		fw_config;
	uint32_t		board_id;
	uint32_t		ram_code;
	uint32_t		sku_id;
	int			has_board_config;

	uint32_t		mac_count;
	struct cb_mac_address	macs[CB_MAX_MAC_ADDRS];
	char			mac_strs[CB_MAX_MAC_ADDRS][18];

	uint64_t		acpi_rsdp;
	int			has_acpi_rsdp;

	uint32_t		spi_flash_size;
	uint32_t		spi_sector_size;
	uint8_t			spi_erase_cmd;
	uint8_t			spi_flags;
	int			has_spi_flash;

	uint16_t		console_type;
	int			has_console_type;

	uint64_t		fb_addr;
	uint32_t		fb_x_res;
	uint32_t		fb_y_res;
	uint32_t		fb_stride;
	uint8_t			fb_bpp;
	int			has_framebuffer;

	uint32_t		gpio_count;
	struct cb_gpio		gpios[CB_MAX_GPIOS];

	uint32_t		tpm_ppi_addr;
	uint8_t			tpm_version;
	int			has_tpm;

	vm_paddr_t		tpm_log_paddr;
	int			has_tpm_log;

	vm_paddr_t		acpi_gnvs_paddr;
	int			has_acpi_gnvs;
	vm_paddr_t		acpi_cnvs_paddr;
	int			has_acpi_cnvs;
	vm_paddr_t		vpd_paddr;
	int			has_vpd;
	vm_paddr_t		wifi_cal_paddr;
	int			has_wifi_cal;
	vm_paddr_t		fmap_paddr;
	int			has_fmap;
	vm_paddr_t		vboot_workbuf_paddr;
	int			has_vboot_workbuf;
	vm_paddr_t		type_c_info_paddr;
	int			has_type_c_info;
	vm_paddr_t		root_bridge_info_paddr;
	int			has_root_bridge_info;

	uint32_t		smmstore_num_blocks;
	uint32_t		smmstore_block_size;
	uint64_t		smmstore_mmap_addr;
	uint32_t		smmstore_com_buffer;
	uint8_t			smmstore_apm_cmd;
	int			has_smmstore;

	vm_paddr_t		timestamps_paddr;
	int			has_timestamps;

	struct sysctl_ctx_list	sysctl_ctx;
	struct sysctl_oid	*sysctl_tree;
};

/*
 * Generic ID-to-name linear search.
 * The table must be terminated by an entry with name == NULL.
 * Returns fallback if no match is found.
 */
struct cb_id_name {
	uint32_t	id;
	const char	*name;
};

static __inline const char *
cb_id_lookup(const struct cb_id_name *tbl, uint32_t id, const char *fallback)
{
	int i;

	for (i = 0; tbl[i].name != NULL; i++) {
		if (tbl[i].id == id)
			return (tbl[i].name);
	}
	return (fallback);
}

/*
 * CBMEM ID to human-readable name lookup.
 */
static __inline const char *
cbmem_id_to_name(uint32_t id)
{
	static const struct cb_id_name names[] = {
		{ CBMEM_ID_ACPI,		"ACPI" },
		{ CBMEM_ID_ACPI_GNVS,		"ACPI GNVS" },
		{ CBMEM_ID_AFTER_CAR,		"AFTER CAR" },
		{ CBMEM_ID_CBTABLE,		"COREBOOT" },
		{ CBMEM_ID_CBTABLE_FWD,	"COREBOOT FWD" },
		{ CBMEM_ID_CBFS_RO_MCACHE,	"RO MCACHE" },
		{ CBMEM_ID_CBFS_RW_MCACHE,	"RW MCACHE" },
		{ CBMEM_ID_CONSOLE,		"CONSOLE" },
		{ CBMEM_ID_ELOG,		"ELOG" },
		{ CBMEM_ID_FMAP,		"FMAP" },
		{ CBMEM_ID_FREESPACE,		"FREE SPACE" },
		{ CBMEM_ID_FSP_RESERVED_MEMORY, "FSP MEMORY" },
		{ CBMEM_ID_FSP_RUNTIME,	"FSP RUNTIME" },
		{ CBMEM_ID_FSPM_VERSION,	"FSPM VERSION" },
		{ CBMEM_ID_IGD_OPREGION,	"IGD OPREGION" },
		{ CBMEM_ID_IMD_ROOT,		"IMD ROOT" },
		{ CBMEM_ID_IMD_SMALL,		"IMD SMALL" },
		{ CBMEM_ID_MEMINFO,		"MEM INFO" },
		{ CBMEM_ID_MPTABLE,		"SMP TABLE" },
		{ CBMEM_ID_MRCDATA,		"MRC DATA" },
		{ CBMEM_ID_PIRQ,		"IRQ TABLE" },
		{ CBMEM_ID_POWER_STATE,	"POWER STATE" },
		{ CBMEM_ID_RAM_OOPS,		"RAMOOPS" },
		{ CBMEM_ID_RAMSTAGE,		"RAMSTAGE" },
		{ CBMEM_ID_REFCODE,		"REFCODE" },
		{ CBMEM_ID_RESUME,		"ACPI RESUME" },
		{ CBMEM_ID_ROMSTAGE_INFO,	"ROMSTAGE" },
		{ CBMEM_ID_ROMSTAGE_RAM_STACK,	"ROMSTG STACK" },
		{ CBMEM_ID_ROOT,		"CBMEM ROOT" },
		{ CBMEM_ID_SMBIOS,		"SMBIOS" },
		{ CBMEM_ID_SMM_COMBUFFER,	"SMM COMBUF" },
		{ CBMEM_ID_SMM_SAVE_SPACE,	"SMM BACKUP" },
		{ CBMEM_ID_TIMESTAMP,		"TIMESTAMP" },
		{ CBMEM_ID_VBOOT_WORKBUF,	"VBOOT WORK" },
		{ CBMEM_ID_VPD,		"VPD" },
		{ 0, NULL }
	};

	return (cb_id_lookup(names, id, "UNKNOWN"));
}

/*
 * IP-style 16-bit checksum (RFC 1071) over 16-bit words.
 * When computed over data including its checksum field, result is 0.
 */
static __inline uint16_t
cb_checksum(const void *ptr, size_t len)
{
	const uint8_t *p = (const uint8_t *)ptr;
	uint32_t sum = 0;
	size_t i;

	for (i = 0; i + 1 < len; i += 2)
		sum += (uint32_t)p[i] | ((uint32_t)p[i + 1] << 8);

	if (i < len)
		sum += p[i];

	while (sum >> 16)
		sum = (sum & 0xffff) + (sum >> 16);

	return ((uint16_t)~sum);
}

/*
 * Destroy a character device and clear the pointer.
 * Safe to call with a NULL cdev pointer.
 */
static __inline void
coreboot_cdev_destroy(struct cdev **cdevp)
{

	if (*cdevp != NULL) {
		destroy_dev(*cdevp);
		*cdevp = NULL;
	}
}

/* Functions exported from coreboot.c */
struct coreboot_softc *coreboot_get_softc(void);

/* Functions exported from coreboot_console.c */
int	coreboot_console_create(struct coreboot_softc *sc);
void	coreboot_console_destroy(struct coreboot_softc *sc);

/* Functions exported from coreboot_cbmem.c */
int	coreboot_cbmem_create(struct coreboot_softc *sc);
void	coreboot_cbmem_destroy(struct coreboot_softc *sc);

/* Functions exported from coreboot_timestamps.c */
int	coreboot_timestamps_register(struct coreboot_softc *sc,
	    struct sysctl_oid *parent);

#endif /* _DEV_COREBOOT_COREBOOT_H_ */
