/*
 * Copyright (c) 2026 Abdelkader Boudih <freebsd@seuros.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * coreboot(4) - FreeBSD driver for coreboot firmware tables
 *
 * Discovers the coreboot table by scanning low memory for the "LBIO"
 * signature, follows CB_TAG_FORWARD to the high-memory table, and
 * exposes firmware information through sysctl(9) and character devices.
 */

#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/sysctl.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/coreboot/coreboot.h>

static struct coreboot_softc *coreboot_sc;

/*
 * Debug verbosity control, non-zero enables extra output.
 * Tunable via loader.conf: hw.coreboot.debug=1
 * Runtime: sysctl hw.coreboot.debug=1
 * Registered dynamically under hw.coreboot in
 * coreboot_register_sysctls().
 */
static int coreboot_debug = 0;
TUNABLE_INT("hw.coreboot.debug", &coreboot_debug);

struct coreboot_softc *
coreboot_get_softc(void)
{
	return (coreboot_sc);
}

static void	coreboot_identify(driver_t *, device_t);
static int	coreboot_probe(device_t);
static int	coreboot_attach(device_t);
static int	coreboot_detach(device_t);
static int	coreboot_modevent(module_t, int, void *);

/*
 * Scan a physical memory region for the "LBIO" signature.
 * Returns the physical address of the header, or 0 if not found.
 */
static vm_paddr_t
coreboot_scan_region(vm_paddr_t start, vm_paddr_t end)
{
	vm_paddr_t addr;
	void *va;
	struct cb_header *hdr;

	for (addr = (start == 0 ? CB_SCAN_LOW_STEP : start); addr < end;
	    addr += CB_SCAN_LOW_STEP) {
		va = pmap_mapbios(addr, sizeof(struct cb_header));
		if (va == NULL)
			continue;

		hdr = (struct cb_header *)va;
		if (memcmp(hdr->signature, CB_HEADER_SIGNATURE,
		    CB_HEADER_SIG_LEN) == 0) {
			pmap_unmapbios(va, sizeof(struct cb_header));
			return (addr);
		}
		pmap_unmapbios(va, sizeof(struct cb_header));
	}
	return (0);
}

/*
 * Validate length fields in the header before using them for mappings
 * and pointer arithmetic.
 */
static int
coreboot_sanitize_header(const struct cb_header *hdr, vm_size_t *map_size)
{
	uint64_t total;

	if (hdr->header_bytes < sizeof(*hdr) ||
	    hdr->header_bytes > CB_MAX_HEADER_BYTES)
		return (EINVAL);
	if ((hdr->header_bytes % CB_TABLE_ALIGN) != 0)
		return (EINVAL);
	if (hdr->table_bytes > CB_MAX_TABLE_BYTES)
		return (EINVAL);
	if ((hdr->table_bytes % CB_TABLE_ALIGN) != 0)
		return (EINVAL);

	total = (uint64_t)hdr->header_bytes + (uint64_t)hdr->table_bytes;
	if (total > CB_MAX_TABLE_MAP_BYTES)
		return (EINVAL);

	*map_size = (vm_size_t)total;
	return (0);
}

/*
 * Validate the coreboot header checksum.
 * Returns 0 on success, non-zero on failure.
 */
static int
coreboot_validate_header(struct cb_header *hdr, vm_size_t mapped_len)
{
	uint16_t cksum;

	if (hdr->header_bytes > mapped_len)
		return (EINVAL);

	cksum = cb_checksum(hdr, hdr->header_bytes);
	if (cksum != 0)
		return (EINVAL);

	return (0);
}

/*
 * Validate checksum for the table payload.
 */
static int
coreboot_validate_table(struct cb_header *hdr, vm_size_t mapped_len)
{
	const uint8_t *table;
	uint16_t cksum;

	if (hdr->table_bytes == 0)
		return (0);

	if ((uint64_t)hdr->header_bytes + (uint64_t)hdr->table_bytes >
	    mapped_len)
		return (EINVAL);
	if (hdr->table_checksum > UINT16_MAX)
		return (EINVAL);

	table = (const uint8_t *)hdr + hdr->header_bytes;
	cksum = cb_checksum(table, hdr->table_bytes);
	if (cksum != (uint16_t)hdr->table_checksum)
		return (EINVAL);

	return (0);
}

static void
coreboot_copy_bounded_string(const char *src, size_t maxlen, char *dst,
    size_t dstlen)
{
	size_t slen;

	if (dstlen == 0)
		return;

	slen = strnlen(src, maxlen);
	if (slen >= dstlen)
		slen = dstlen - 1;
	memcpy(dst, src, slen);
	dst[slen] = '\0';
}

/*
 * Copy a coreboot string record into a destination buffer.
 */
static void
coreboot_copy_string(const struct cb_string *rec, char *dst, size_t dstlen)
{
	size_t slen;

	slen = rec->size - sizeof(struct cb_record);
	if (slen >= dstlen)
		slen = dstlen - 1;
	memcpy(dst, rec->string, slen);
	dst[slen] = '\0';

	/* Strip trailing whitespace/nulls */
	while (slen > 0 && (dst[slen - 1] == '\0' || dst[slen - 1] == ' ' ||
	    dst[slen - 1] == '\n'))
		dst[--slen] = '\0';
}

/*
 * Extract mainboard vendor and part number from the strings field.
 */
static void
coreboot_parse_mainboard(struct coreboot_softc *sc,
    const struct cb_mainboard *mb)
{
	const char *strings = (const char *)mb->strings;
	size_t total = mb->size - offsetof(struct cb_mainboard, strings);
	uint8_t vendor_off, part_off;

	vendor_off = mb->vendor_idx;
	part_off = mb->part_idx;

	if (vendor_off < total)
		coreboot_copy_bounded_string(strings + vendor_off,
		    total - vendor_off, sc->mb_vendor, sizeof(sc->mb_vendor));
	if (part_off < total)
		coreboot_copy_bounded_string(strings + part_off,
		    total - part_off, sc->mb_part, sizeof(sc->mb_part));
}

/*
 * Parse all records in the coreboot table and populate softc.
 */
static void
coreboot_parse_table(struct coreboot_softc *sc, struct cb_header *hdr)
{
	uint8_t *entry;
	uint8_t *table_end;
	struct cb_record *rec;

	entry = (uint8_t *)hdr + hdr->header_bytes;
	table_end = entry + hdr->table_bytes;

	while ((size_t)(table_end - entry) >= sizeof(struct cb_record)) {
		size_t rec_size;

		rec = (struct cb_record *)entry;
		rec_size = rec->size;

		if (rec_size < sizeof(struct cb_record))
			break;
		if (rec_size > (size_t)(table_end - entry))
			break;

		switch (rec->tag) {
		case CB_TAG_VERSION:
			coreboot_copy_string((struct cb_string *)rec,
			    sc->version, sizeof(sc->version));
			break;

		case CB_TAG_EXTRA_VERSION:
			coreboot_copy_string((struct cb_string *)rec,
			    sc->extra_version, sizeof(sc->extra_version));
			break;

		case CB_TAG_BUILD:
			coreboot_copy_string((struct cb_string *)rec,
			    sc->build, sizeof(sc->build));
			break;

		case CB_TAG_COMPILE_TIME:
			coreboot_copy_string((struct cb_string *)rec,
			    sc->compile_time, sizeof(sc->compile_time));
			break;

		case CB_TAG_COMPILER:
			coreboot_copy_string((struct cb_string *)rec,
			    sc->compiler, sizeof(sc->compiler));
			break;

		case CB_TAG_PLATFORM_BLOB_VERSION:
			coreboot_copy_string((struct cb_string *)rec,
			    sc->platform_blob_version,
			    sizeof(sc->platform_blob_version));
			break;

		case CB_TAG_SERIALNO:
			coreboot_copy_string((struct cb_string *)rec,
			    sc->serialno, sizeof(sc->serialno));
			break;

		case CB_TAG_VERSION_TIMESTAMP: {
			struct cb_version_timestamp *ts =
			    (struct cb_version_timestamp *)rec;

			if (rec_size < sizeof(*ts))
				break;
			sc->version_timestamp = ts->timestamp;
			sc->has_version_timestamp = 1;
			break;
		}

		case CB_TAG_MAINBOARD:
			if (rec_size < offsetof(struct cb_mainboard, strings))
				break;
			coreboot_parse_mainboard(sc,
			    (struct cb_mainboard *)rec);
			break;

		case CB_TAG_SERIAL: {
			struct cb_serial *ser = (struct cb_serial *)rec;

			if (rec_size < sizeof(*ser))
				break;
			sc->serial_baseaddr = ser->baseaddr;
			sc->serial_baud = ser->baud;
			sc->serial_regwidth = ser->regwidth;
			sc->has_serial = 1;
			break;
		}

		case CB_TAG_TSC_INFO: {
			struct cb_tsc_info *tsc = (struct cb_tsc_info *)rec;

			if (rec_size < sizeof(*tsc))
				break;
			sc->tsc_freq_khz = tsc->freq_khz;
			sc->has_tsc_info = 1;
			break;
		}

		case CB_TAG_PCIE: {
			struct cb_pcie *pcie = (struct cb_pcie *)rec;

			if (rec_size < sizeof(*pcie))
				break;
			sc->pcie_ctrl_base = pcie->ctrl_base;
			sc->has_pcie = 1;
			break;
		}

		case CB_TAG_BOOT_MEDIA_PARAMS: {
			struct cb_boot_media_params *bmp =
			    (struct cb_boot_media_params *)rec;

			if (rec_size < sizeof(*bmp))
				break;
			sc->fmap_offset = bmp->fmap_offset;
			sc->cbfs_offset = bmp->cbfs_offset;
			sc->cbfs_size = bmp->cbfs_size;
			sc->boot_media_size = bmp->boot_media_size;
			sc->has_boot_media = 1;
			break;
		}

		case CB_TAG_MMC_INFO: {
			struct cb_mmc_info *mmc = (struct cb_mmc_info *)rec;

			if (rec_size < sizeof(*mmc))
				break;
			sc->mmc_early_cmd1_status = mmc->early_cmd1_status;
			sc->has_mmc_info = 1;
			break;
		}

		case CB_TAG_CBMEM_CONSOLE: {
			struct cb_cbmem_ref *ref = (struct cb_cbmem_ref *)rec;

			if (rec_size < sizeof(*ref))
				break;
			sc->console_paddr = (vm_paddr_t)ref->cbmem_addr;
			sc->has_console = 1;
			break;
		}

		case CB_TAG_CBMEM_ENTRY: {
			struct cb_cbmem_entry *ent =
			    (struct cb_cbmem_entry *)rec;

			if (rec_size < sizeof(*ent))
				break;
			if (sc->cbmem_count < CB_MAX_CBMEM_ENTRIES) {
				struct cbmem_entry_info *info =
				    &sc->cbmem_entries[sc->cbmem_count];
				info->id = ent->id;
				info->address = ent->address;
				info->size = ent->entry_size;
				strlcpy(info->name, cbmem_id_to_name(ent->id),
				    sizeof(info->name));
				sc->cbmem_count++;
			}
			break;
		}

		case CB_TAG_BOARD_CONFIG: {
			struct cb_board_config *bc =
			    (struct cb_board_config *)rec;

			if (rec_size < sizeof(*bc))
				break;
			sc->fw_config = bc->fw_config;
			sc->board_id = bc->board_id;
			sc->ram_code = bc->ram_code;
			sc->sku_id = bc->sku_id;
			sc->has_board_config = 1;
			break;
		}

		case CB_TAG_MAC_ADDRS: {
			struct cb_macs *macs = (struct cb_macs *)rec;
			uint32_t i, count;

			if (rec_size < sizeof(*macs))
				break;
			count = macs->count;
			if (count > CB_MAX_MAC_ADDRS)
				count = CB_MAX_MAC_ADDRS;
			if (rec_size < sizeof(*macs) +
			    count * sizeof(struct cb_mac_address))
				break;
			for (i = 0; i < count; i++)
				sc->macs[i] = macs->entries[i];
			sc->mac_count = count;
			break;
		}

		case CB_TAG_ACPI_RSDP: {
			struct cb_acpi_rsdp *rsdp =
			    (struct cb_acpi_rsdp *)rec;

			if (rec_size < sizeof(*rsdp))
				break;
			sc->acpi_rsdp = rsdp->rsdp_pointer;
			sc->has_acpi_rsdp = 1;
			break;
		}

		case CB_TAG_SPI_FLASH: {
			struct cb_spi_flash *spi =
			    (struct cb_spi_flash *)rec;

			if (rec_size < sizeof(*spi))
				break;
			sc->spi_flash_size = spi->flash_size;
			sc->spi_sector_size = spi->sector_size;
			sc->spi_erase_cmd = spi->erase_cmd;
			sc->spi_flags = spi->flags;
			sc->has_spi_flash = 1;
			break;
		}

		case CB_TAG_CONSOLE: {
			struct cb_console *con = (struct cb_console *)rec;

			if (rec_size < sizeof(*con))
				break;
			sc->console_type = con->type;
			sc->has_console_type = 1;
			break;
		}

		case CB_TAG_FRAMEBUFFER: {
			struct cb_framebuffer *fb =
			    (struct cb_framebuffer *)rec;

			if (rec_size < CB_FRAMEBUFFER_MIN_SIZE)
				break;
			sc->fb_addr = fb->physical_address;
			sc->fb_x_res = fb->x_resolution;
			sc->fb_y_res = fb->y_resolution;
			sc->fb_stride = fb->bytes_per_line;
			sc->fb_bpp = fb->bits_per_pixel;
			sc->has_framebuffer = 1;
			break;
		}

		case CB_TAG_GPIO: {
			struct cb_gpios *gpios = (struct cb_gpios *)rec;
			uint32_t i, count;

			if (rec_size < sizeof(*gpios))
				break;
			count = gpios->count;
			if (count > CB_MAX_GPIOS)
				count = CB_MAX_GPIOS;
			if (rec_size < sizeof(*gpios) +
			    count * sizeof(struct cb_gpio))
				break;
			for (i = 0; i < count; i++)
				sc->gpios[i] = gpios->entries[i];
			sc->gpio_count = count;
			break;
		}

		case CB_TAG_TPM_PPI_HANDOFF: {
			struct cb_tpm_ppi *tpm = (struct cb_tpm_ppi *)rec;

			if (rec_size < sizeof(*tpm))
				break;
			sc->tpm_ppi_addr = tpm->ppi_address;
			sc->tpm_version = tpm->tpm_version;
			sc->has_tpm = 1;
			break;
		}

		case CB_TAG_TIMESTAMPS: {
			struct cb_cbmem_ref *ref = (struct cb_cbmem_ref *)rec;

			if (rec_size < sizeof(*ref))
				break;
			sc->timestamps_paddr = (vm_paddr_t)ref->cbmem_addr;
			sc->has_timestamps = 1;
			break;
		}

		case CB_TAG_ACPI_GNVS: {
			struct cb_cbmem_ref *ref = (struct cb_cbmem_ref *)rec;

			if (rec_size < sizeof(*ref))
				break;
			sc->acpi_gnvs_paddr = (vm_paddr_t)ref->cbmem_addr;
			sc->has_acpi_gnvs = 1;
			break;
		}

		case CB_TAG_ACPI_CNVS: {
			struct cb_cbmem_ref *ref = (struct cb_cbmem_ref *)rec;

			if (rec_size < sizeof(*ref))
				break;
			sc->acpi_cnvs_paddr = (vm_paddr_t)ref->cbmem_addr;
			sc->has_acpi_cnvs = 1;
			break;
		}

		case CB_TAG_VPD: {
			struct cb_cbmem_ref *ref = (struct cb_cbmem_ref *)rec;

			if (rec_size < sizeof(*ref))
				break;
			sc->vpd_paddr = (vm_paddr_t)ref->cbmem_addr;
			sc->has_vpd = 1;
			break;
		}

		case CB_TAG_WIFI_CALIBRATION: {
			struct cb_cbmem_ref *ref = (struct cb_cbmem_ref *)rec;

			if (rec_size < sizeof(*ref))
				break;
			sc->wifi_cal_paddr = (vm_paddr_t)ref->cbmem_addr;
			sc->has_wifi_cal = 1;
			break;
		}

		case CB_TAG_FMAP: {
			struct cb_cbmem_ref *ref = (struct cb_cbmem_ref *)rec;

			if (rec_size < sizeof(*ref))
				break;
			sc->fmap_paddr = (vm_paddr_t)ref->cbmem_addr;
			sc->has_fmap = 1;
			break;
		}

		case CB_TAG_VBOOT_WORKBUF: {
			struct cb_cbmem_ref *ref = (struct cb_cbmem_ref *)rec;

			if (rec_size < sizeof(*ref))
				break;
			sc->vboot_workbuf_paddr = (vm_paddr_t)ref->cbmem_addr;
			sc->has_vboot_workbuf = 1;
			break;
		}

		case CB_TAG_TYPE_C_INFO: {
			struct cb_cbmem_ref *ref = (struct cb_cbmem_ref *)rec;

			if (rec_size < sizeof(*ref))
				break;
			sc->type_c_info_paddr = (vm_paddr_t)ref->cbmem_addr;
			sc->has_type_c_info = 1;
			break;
		}

		case CB_TAG_ROOT_BRIDGE_INFO: {
			struct cb_cbmem_ref *ref = (struct cb_cbmem_ref *)rec;

			if (rec_size < sizeof(*ref))
				break;
			sc->root_bridge_info_paddr =
			    (vm_paddr_t)ref->cbmem_addr;
			sc->has_root_bridge_info = 1;
			break;
		}

		case CB_TAG_TPM_CB_LOG: {
			struct cb_cbmem_ref *ref = (struct cb_cbmem_ref *)rec;

			if (rec_size < sizeof(*ref))
				break;
			sc->tpm_log_paddr = (vm_paddr_t)ref->cbmem_addr;
			sc->has_tpm_log = 1;
			break;
		}

		case CB_TAG_SMMSTOREV2: {
			struct cb_smmstorev2 *smm =
			    (struct cb_smmstorev2 *)rec;

			if (rec_size < CB_SMMSTOREV2_BASE_SIZE)
				break;
			sc->smmstore_num_blocks = smm->num_blocks;
			sc->smmstore_block_size = smm->block_size;
			sc->smmstore_com_buffer = smm->com_buffer;
			sc->smmstore_apm_cmd = smm->apm_cmd;
			/* 64-bit mmap_addr only present in newer coreboot */
			if (rec_size >= sizeof(*smm))
				sc->smmstore_mmap_addr = smm->mmap_addr;
			else
				sc->smmstore_mmap_addr =
				    (uint64_t)smm->mmap_addr_lo;
			sc->has_smmstore = 1;
			break;
		}

		default:
			break;
		}

		entry += rec_size;
	}
}

/*
 * Table-driven sysctl registration.
 *
 * Each leaf descriptor specifies the parent node, name, type, data offset
 * into softc, a guard flag offset (or -1 for unconditional), and flags.
 * Nodes that group related leaves are indexed by cb_sysctl_node.
 */

/* Node indices for parent selection */
enum cb_sysctl_node {
	CB_NODE_ROOT = 0,
	CB_NODE_MAINBOARD,
	CB_NODE_SERIAL,
	CB_NODE_BOARD,
	CB_NODE_BOOT_MEDIA,
	CB_NODE_SPI_FLASH,
	CB_NODE_FRAMEBUFFER,
	CB_NODE_TPM,
	CB_NODE_SMMSTORE,
	CB_NODE_CBMEM_REFS,
	CB_NODE_COUNT
};

/* Sysctl value type discriminator */
enum cb_sysctl_type {
	CB_SYSCTL_U8,
	CB_SYSCTL_U16,
	CB_SYSCTL_U32,
	CB_SYSCTL_S32,
	CB_SYSCTL_U64,
	CB_SYSCTL_ULONG,
	CB_SYSCTL_STRING,
};

/*
 * Guard mode: how to decide whether a leaf should be registered.
 *   STR_NONEMPTY: check that the char[] at guard_off is non-empty
 *   FLAG_SET:     check that the int at guard_off is non-zero
 *   ALWAYS:       unconditional (guard_off ignored)
 */
enum cb_sysctl_guard {
	CB_GUARD_ALWAYS,
	CB_GUARD_FLAG_SET,
	CB_GUARD_STR_NONEMPTY,
};

struct cb_sysctl_node_desc {
	enum cb_sysctl_node	id;
	enum cb_sysctl_node	parent;
	const char		*name;
	const char		*desc;
};

struct cb_sysctl_leaf {
	enum cb_sysctl_node	parent;
	const char		*name;
	enum cb_sysctl_type	type;
	size_t			data_off;
	enum cb_sysctl_guard	guard;
	size_t			guard_off;
	int			flags;
	const char		*desc;
};

/* Helper macros for field offset within coreboot_softc */
#define SC_OFF(field)	offsetof(struct coreboot_softc, field)

static const struct cb_sysctl_node_desc cb_nodes[] = {
	{ CB_NODE_MAINBOARD,	CB_NODE_ROOT,	"mainboard",
	    "Mainboard information" },
	{ CB_NODE_SERIAL,	CB_NODE_ROOT,	"serial",
	    "Serial port" },
	{ CB_NODE_BOARD,	CB_NODE_ROOT,	"board",
	    "Board identification" },
	{ CB_NODE_BOOT_MEDIA,	CB_NODE_ROOT,	"boot_media",
	    "Boot media parameters" },
	{ CB_NODE_SPI_FLASH,	CB_NODE_ROOT,	"spi_flash",
	    "SPI flash parameters" },
	{ CB_NODE_FRAMEBUFFER,	CB_NODE_ROOT,	"framebuffer",
	    "Framebuffer information" },
	{ CB_NODE_TPM,		CB_NODE_ROOT,	"tpm",
	    "TPM information" },
	{ CB_NODE_SMMSTORE,	CB_NODE_ROOT,	"smmstore",
	    "SMMSTORE v2 configuration" },
	{ CB_NODE_CBMEM_REFS,	CB_NODE_ROOT,	"cbmem_refs",
	    "Additional CBMEM reference addresses" },
};

static const struct cb_sysctl_leaf cb_leaves[] = {
	/* Root-level strings (guarded by non-empty string) */
	{ CB_NODE_ROOT, "version", CB_SYSCTL_STRING,
	    SC_OFF(version), CB_GUARD_STR_NONEMPTY, SC_OFF(version),
	    CTLFLAG_RD, "Firmware version" },
	{ CB_NODE_ROOT, "build", CB_SYSCTL_STRING,
	    SC_OFF(build), CB_GUARD_STR_NONEMPTY, SC_OFF(build),
	    CTLFLAG_RD, "Build date" },
	{ CB_NODE_ROOT, "compile_time", CB_SYSCTL_STRING,
	    SC_OFF(compile_time), CB_GUARD_STR_NONEMPTY, SC_OFF(compile_time),
	    CTLFLAG_RD, "Firmware compile time" },
	{ CB_NODE_ROOT, "compiler", CB_SYSCTL_STRING,
	    SC_OFF(compiler), CB_GUARD_STR_NONEMPTY, SC_OFF(compiler),
	    CTLFLAG_RD, "Compiler info" },
	{ CB_NODE_ROOT, "extra_version", CB_SYSCTL_STRING,
	    SC_OFF(extra_version), CB_GUARD_STR_NONEMPTY, SC_OFF(extra_version),
	    CTLFLAG_RD, "Extra version info" },
	{ CB_NODE_ROOT, "serialno", CB_SYSCTL_STRING,
	    SC_OFF(serialno), CB_GUARD_STR_NONEMPTY, SC_OFF(serialno),
	    CTLFLAG_RD, "Serial number" },
	{ CB_NODE_ROOT, "platform_blob_version", CB_SYSCTL_STRING,
	    SC_OFF(platform_blob_version), CB_GUARD_STR_NONEMPTY,
	    SC_OFF(platform_blob_version),
	    CTLFLAG_RD, "Platform blob version" },

	/* Root-level scalars */
	{ CB_NODE_ROOT, "version_timestamp", CB_SYSCTL_U32,
	    SC_OFF(version_timestamp), CB_GUARD_FLAG_SET,
	    SC_OFF(has_version_timestamp),
	    CTLFLAG_RD, "Firmware version timestamp" },
	{ CB_NODE_ROOT, "table_addr", CB_SYSCTL_U64,
	    SC_OFF(table_paddr), CB_GUARD_ALWAYS, 0,
	    CTLFLAG_RD, "Physical address of coreboot table" },
	{ CB_NODE_ROOT, "table_size", CB_SYSCTL_ULONG,
	    SC_OFF(table_size), CB_GUARD_ALWAYS, 0,
	    CTLFLAG_RD, "Total coreboot table size" },
	{ CB_NODE_ROOT, "tsc_freq_khz", CB_SYSCTL_U32,
	    SC_OFF(tsc_freq_khz), CB_GUARD_FLAG_SET, SC_OFF(has_tsc_info),
	    CTLFLAG_RD, "TSC frequency in kHz" },
	{ CB_NODE_ROOT, "pcie_ctrl_base", CB_SYSCTL_U64,
	    SC_OFF(pcie_ctrl_base), CB_GUARD_FLAG_SET, SC_OFF(has_pcie),
	    CTLFLAG_RD, "PCIe controller base address" },
	{ CB_NODE_ROOT, "acpi_rsdp", CB_SYSCTL_U64,
	    SC_OFF(acpi_rsdp), CB_GUARD_FLAG_SET, SC_OFF(has_acpi_rsdp),
	    CTLFLAG_RD, "ACPI RSDP physical address" },
	{ CB_NODE_ROOT, "mmc_early_cmd1_status", CB_SYSCTL_S32,
	    SC_OFF(mmc_early_cmd1_status), CB_GUARD_FLAG_SET,
	    SC_OFF(has_mmc_info),
	    CTLFLAG_RD, "Early eMMC CMD1 status" },
	{ CB_NODE_ROOT, "console_type", CB_SYSCTL_U16,
	    SC_OFF(console_type), CB_GUARD_FLAG_SET, SC_OFF(has_console_type),
	    CTLFLAG_RD, "Firmware console type" },
	{ CB_NODE_ROOT, "timestamps_addr", CB_SYSCTL_U64,
	    SC_OFF(timestamps_paddr), CB_GUARD_FLAG_SET, SC_OFF(has_timestamps),
	    CTLFLAG_RD, "Timestamps CBMEM physical address" },

	/* Mainboard children */
	{ CB_NODE_MAINBOARD, "vendor", CB_SYSCTL_STRING,
	    SC_OFF(mb_vendor), CB_GUARD_STR_NONEMPTY, SC_OFF(mb_vendor),
	    CTLFLAG_RD, "Board vendor" },
	{ CB_NODE_MAINBOARD, "part", CB_SYSCTL_STRING,
	    SC_OFF(mb_part), CB_GUARD_STR_NONEMPTY, SC_OFF(mb_part),
	    CTLFLAG_RD, "Board part number" },

	/* Serial children */
	{ CB_NODE_SERIAL, "baseaddr", CB_SYSCTL_U32,
	    SC_OFF(serial_baseaddr), CB_GUARD_FLAG_SET, SC_OFF(has_serial),
	    CTLFLAG_RD, "Base address" },
	{ CB_NODE_SERIAL, "baud", CB_SYSCTL_U32,
	    SC_OFF(serial_baud), CB_GUARD_FLAG_SET, SC_OFF(has_serial),
	    CTLFLAG_RD, "Baud rate" },
	{ CB_NODE_SERIAL, "regwidth", CB_SYSCTL_U32,
	    SC_OFF(serial_regwidth), CB_GUARD_FLAG_SET, SC_OFF(has_serial),
	    CTLFLAG_RD, "Register width" },

	/* Board config children */
	{ CB_NODE_BOARD, "fw_config", CB_SYSCTL_U64,
	    SC_OFF(fw_config), CB_GUARD_FLAG_SET, SC_OFF(has_board_config),
	    CTLFLAG_RD, "Firmware configuration bitmask" },
	{ CB_NODE_BOARD, "board_id", CB_SYSCTL_U32,
	    SC_OFF(board_id), CB_GUARD_FLAG_SET, SC_OFF(has_board_config),
	    CTLFLAG_RD, "Board ID" },
	{ CB_NODE_BOARD, "ram_code", CB_SYSCTL_U32,
	    SC_OFF(ram_code), CB_GUARD_FLAG_SET, SC_OFF(has_board_config),
	    CTLFLAG_RD, "RAM code" },
	{ CB_NODE_BOARD, "sku_id", CB_SYSCTL_U32,
	    SC_OFF(sku_id), CB_GUARD_FLAG_SET, SC_OFF(has_board_config),
	    CTLFLAG_RD, "SKU ID" },

	/* Boot media children */
	{ CB_NODE_BOOT_MEDIA, "fmap_offset", CB_SYSCTL_U64,
	    SC_OFF(fmap_offset), CB_GUARD_FLAG_SET, SC_OFF(has_boot_media),
	    CTLFLAG_RD, "FMAP offset from boot media start" },
	{ CB_NODE_BOOT_MEDIA, "cbfs_offset", CB_SYSCTL_U64,
	    SC_OFF(cbfs_offset), CB_GUARD_FLAG_SET, SC_OFF(has_boot_media),
	    CTLFLAG_RD, "CBFS offset from boot media start" },
	{ CB_NODE_BOOT_MEDIA, "cbfs_size", CB_SYSCTL_U64,
	    SC_OFF(cbfs_size), CB_GUARD_FLAG_SET, SC_OFF(has_boot_media),
	    CTLFLAG_RD, "CBFS size in bytes" },
	{ CB_NODE_BOOT_MEDIA, "size", CB_SYSCTL_U64,
	    SC_OFF(boot_media_size), CB_GUARD_FLAG_SET, SC_OFF(has_boot_media),
	    CTLFLAG_RD, "Boot media size in bytes" },

	/* SPI flash children */
	{ CB_NODE_SPI_FLASH, "size", CB_SYSCTL_U32,
	    SC_OFF(spi_flash_size), CB_GUARD_FLAG_SET, SC_OFF(has_spi_flash),
	    CTLFLAG_RD, "Flash size in bytes" },
	{ CB_NODE_SPI_FLASH, "sector_size", CB_SYSCTL_U32,
	    SC_OFF(spi_sector_size), CB_GUARD_FLAG_SET, SC_OFF(has_spi_flash),
	    CTLFLAG_RD, "Sector size in bytes" },
	{ CB_NODE_SPI_FLASH, "erase_cmd", CB_SYSCTL_U8,
	    SC_OFF(spi_erase_cmd), CB_GUARD_FLAG_SET, SC_OFF(has_spi_flash),
	    CTLFLAG_RD, "Erase command byte" },

	/* Framebuffer children */
	{ CB_NODE_FRAMEBUFFER, "addr", CB_SYSCTL_U64,
	    SC_OFF(fb_addr), CB_GUARD_FLAG_SET, SC_OFF(has_framebuffer),
	    CTLFLAG_RD, "Physical address" },
	{ CB_NODE_FRAMEBUFFER, "x_res", CB_SYSCTL_U32,
	    SC_OFF(fb_x_res), CB_GUARD_FLAG_SET, SC_OFF(has_framebuffer),
	    CTLFLAG_RD, "Horizontal resolution" },
	{ CB_NODE_FRAMEBUFFER, "y_res", CB_SYSCTL_U32,
	    SC_OFF(fb_y_res), CB_GUARD_FLAG_SET, SC_OFF(has_framebuffer),
	    CTLFLAG_RD, "Vertical resolution" },
	{ CB_NODE_FRAMEBUFFER, "bpp", CB_SYSCTL_U8,
	    SC_OFF(fb_bpp), CB_GUARD_FLAG_SET, SC_OFF(has_framebuffer),
	    CTLFLAG_RD, "Bits per pixel" },

	/* TPM children */
	{ CB_NODE_TPM, "version", CB_SYSCTL_U8,
	    SC_OFF(tpm_version), CB_GUARD_FLAG_SET, SC_OFF(has_tpm),
	    CTLFLAG_RD, "TPM version (1=1.2, 2=2.0)" },
	{ CB_NODE_TPM, "ppi_addr", CB_SYSCTL_U32,
	    SC_OFF(tpm_ppi_addr), CB_GUARD_FLAG_SET, SC_OFF(has_tpm),
	    CTLFLAG_RD, "PPI address" },
	{ CB_NODE_TPM, "cblog_addr", CB_SYSCTL_U64,
	    SC_OFF(tpm_log_paddr), CB_GUARD_FLAG_SET, SC_OFF(has_tpm_log),
	    CTLFLAG_RD, "TPM event log physical address" },

	/* SMMSTORE children */
	{ CB_NODE_SMMSTORE, "num_blocks", CB_SYSCTL_U32,
	    SC_OFF(smmstore_num_blocks), CB_GUARD_FLAG_SET,
	    SC_OFF(has_smmstore),
	    CTLFLAG_RD, "Number of blocks" },
	{ CB_NODE_SMMSTORE, "block_size", CB_SYSCTL_U32,
	    SC_OFF(smmstore_block_size), CB_GUARD_FLAG_SET,
	    SC_OFF(has_smmstore),
	    CTLFLAG_RD, "Block size in bytes" },
	{ CB_NODE_SMMSTORE, "mmap_addr", CB_SYSCTL_U64,
	    SC_OFF(smmstore_mmap_addr), CB_GUARD_FLAG_SET,
	    SC_OFF(has_smmstore),
	    CTLFLAG_RD, "Memory-mapped address" },
	{ CB_NODE_SMMSTORE, "com_buffer", CB_SYSCTL_U32,
	    SC_OFF(smmstore_com_buffer), CB_GUARD_FLAG_SET,
	    SC_OFF(has_smmstore),
	    CTLFLAG_RD, "Communication buffer address" },
	{ CB_NODE_SMMSTORE, "apm_cmd", CB_SYSCTL_U8,
	    SC_OFF(smmstore_apm_cmd), CB_GUARD_FLAG_SET,
	    SC_OFF(has_smmstore),
	    CTLFLAG_RD, "APM command byte" },

	/* CBMEM reference addresses */
	{ CB_NODE_CBMEM_REFS, "acpi_gnvs", CB_SYSCTL_U64,
	    SC_OFF(acpi_gnvs_paddr), CB_GUARD_FLAG_SET, SC_OFF(has_acpi_gnvs),
	    CTLFLAG_RD, "ACPI GNVS CBMEM physical address" },
	{ CB_NODE_CBMEM_REFS, "acpi_cnvs", CB_SYSCTL_U64,
	    SC_OFF(acpi_cnvs_paddr), CB_GUARD_FLAG_SET, SC_OFF(has_acpi_cnvs),
	    CTLFLAG_RD, "ACPI CNVS CBMEM physical address" },
	{ CB_NODE_CBMEM_REFS, "vpd", CB_SYSCTL_U64,
	    SC_OFF(vpd_paddr), CB_GUARD_FLAG_SET, SC_OFF(has_vpd),
	    CTLFLAG_RD, "VPD CBMEM physical address" },
	{ CB_NODE_CBMEM_REFS, "wifi_calibration", CB_SYSCTL_U64,
	    SC_OFF(wifi_cal_paddr), CB_GUARD_FLAG_SET, SC_OFF(has_wifi_cal),
	    CTLFLAG_RD, "WiFi calibration CBMEM physical address" },
	{ CB_NODE_CBMEM_REFS, "fmap", CB_SYSCTL_U64,
	    SC_OFF(fmap_paddr), CB_GUARD_FLAG_SET, SC_OFF(has_fmap),
	    CTLFLAG_RD, "FMAP CBMEM physical address" },
	{ CB_NODE_CBMEM_REFS, "vboot_workbuf", CB_SYSCTL_U64,
	    SC_OFF(vboot_workbuf_paddr), CB_GUARD_FLAG_SET,
	    SC_OFF(has_vboot_workbuf),
	    CTLFLAG_RD, "Vboot work buffer CBMEM physical address" },
	{ CB_NODE_CBMEM_REFS, "type_c_info", CB_SYSCTL_U64,
	    SC_OFF(type_c_info_paddr), CB_GUARD_FLAG_SET,
	    SC_OFF(has_type_c_info),
	    CTLFLAG_RD, "Type-C info CBMEM physical address" },
	{ CB_NODE_CBMEM_REFS, "root_bridge_info", CB_SYSCTL_U64,
	    SC_OFF(root_bridge_info_paddr), CB_GUARD_FLAG_SET,
	    SC_OFF(has_root_bridge_info),
	    CTLFLAG_RD, "Root bridge info CBMEM physical address" },
};

/*
 * Check whether a leaf's guard condition is satisfied.
 */
static int
cb_sysctl_guard_check(const struct cb_sysctl_leaf *leaf,
    const struct coreboot_softc *sc)
{
	const char *base;

	base = (const char *)sc;
	switch (leaf->guard) {
	case CB_GUARD_ALWAYS:
		return (1);
	case CB_GUARD_FLAG_SET:
		return (*(const int *)(base + leaf->guard_off) != 0);
	case CB_GUARD_STR_NONEMPTY:
		return (*(base + leaf->guard_off) != '\0');
	}
	return (0);
}

/*
 * Type-to-handler mapping for sysctl_add_oid().
 * Mirrors the SYSCTL_ADD_* macros but avoids their CTASSERT on flags.
 */
static const struct {
	int		ctltype;
	int		(*handler)(SYSCTL_HANDLER_ARGS);
	const char	*fmt;
} cb_sysctl_types[] = {
	[CB_SYSCTL_U8]     = { CTLTYPE_U8,	sysctl_handle_8,	"CU" },
	[CB_SYSCTL_U16]    = { CTLTYPE_U16,	sysctl_handle_16,	"SU" },
	[CB_SYSCTL_U32]    = { CTLTYPE_U32,	sysctl_handle_32,	"IU" },
	[CB_SYSCTL_S32]    = { CTLTYPE_S32,	sysctl_handle_32,	"I" },
	[CB_SYSCTL_U64]    = { CTLTYPE_U64,	sysctl_handle_64,	"QU" },
	[CB_SYSCTL_ULONG]  = { CTLTYPE_ULONG,	sysctl_handle_long,	"LU" },
	[CB_SYSCTL_STRING] = { CTLTYPE_STRING,	sysctl_handle_string,	"A" },
};

/*
 * Add a single sysctl leaf under the given parent OID.
 */
static void
cb_sysctl_add_leaf(struct sysctl_ctx_list *ctx, struct sysctl_oid *parent,
    const struct cb_sysctl_leaf *leaf, struct coreboot_softc *sc)
{
	void *ptr;

	ptr = (char *)sc + leaf->data_off;

	sysctl_add_oid(ctx, SYSCTL_CHILDREN(parent), OID_AUTO,
	    leaf->name,
	    cb_sysctl_types[leaf->type].ctltype | CTLFLAG_MPSAFE | leaf->flags,
	    ptr, 0,
	    cb_sysctl_types[leaf->type].handler,
	    cb_sysctl_types[leaf->type].fmt,
	    __DESCR(leaf->desc), NULL);
}

/*
 * Register the sysctl tree under hw.coreboot.*
 *
 * Static leaves and nodes are driven by the cb_leaves[] and cb_nodes[]
 * tables.  Dynamic entries (CBMEM, MAC, GPIO) that require loops over
 * runtime-determined counts are handled explicitly below the table loop.
 */
static void
coreboot_register_sysctls(struct coreboot_softc *sc)
{
	struct sysctl_oid *nodes[CB_NODE_COUNT];
	struct sysctl_oid *oid_cbmem, *oid_entry;
	struct sysctl_oid *oid_mac, *oid_gpio, *oid_pin;
	char numstr[8];
	uint32_t i;
	int any_cbref;

	sysctl_ctx_init(&sc->sysctl_ctx);
	sc->sysctl_tree = SYSCTL_ADD_NODE(&sc->sysctl_ctx,
	    SYSCTL_STATIC_CHILDREN(_hw), OID_AUTO, "coreboot",
	    CTLFLAG_RD | CTLFLAG_MPSAFE, 0, "coreboot firmware information");

	if (sc->sysctl_tree == NULL)
		return;

	memset(nodes, 0, sizeof(nodes));
	nodes[CB_NODE_ROOT] = sc->sysctl_tree;

	SYSCTL_ADD_INT(&sc->sysctl_ctx,
	    SYSCTL_CHILDREN(sc->sysctl_tree), OID_AUTO, "debug",
	    CTLFLAG_RW, &coreboot_debug, 0,
	    "Enable verbose coreboot diagnostics");

	/*
	 * Create intermediate nodes on demand.
	 *
	 * The mainboard node is special: it appears when either vendor or
	 * part is present.  The TPM node appears when has_tpm or has_tpm_log
	 * is set.  The cbmem_refs node appears when any of its children
	 * would be registered.  All other nodes are gated by the guard
	 * flags on their children (a node is created the first time a child
	 * needs it).
	 */

	/* Pre-create mainboard node if either string is populated */
	if (sc->mb_vendor[0] != '\0' || sc->mb_part[0] != '\0')
		nodes[CB_NODE_MAINBOARD] = SYSCTL_ADD_NODE(&sc->sysctl_ctx,
		    SYSCTL_CHILDREN(sc->sysctl_tree), OID_AUTO, "mainboard",
		    CTLFLAG_RD | CTLFLAG_MPSAFE, 0, "Mainboard information");

	/* TPM node appears for has_tpm OR has_tpm_log */
	if (sc->has_tpm || sc->has_tpm_log)
		nodes[CB_NODE_TPM] = SYSCTL_ADD_NODE(&sc->sysctl_ctx,
		    SYSCTL_CHILDREN(sc->sysctl_tree), OID_AUTO, "tpm",
		    CTLFLAG_RD | CTLFLAG_MPSAFE, 0, "TPM information");

	/* cbmem_refs node: created if any ref address is present */
	any_cbref = sc->has_acpi_gnvs || sc->has_acpi_cnvs || sc->has_vpd ||
	    sc->has_wifi_cal || sc->has_fmap || sc->has_vboot_workbuf ||
	    sc->has_type_c_info || sc->has_root_bridge_info;
	if (any_cbref)
		nodes[CB_NODE_CBMEM_REFS] = SYSCTL_ADD_NODE(&sc->sysctl_ctx,
		    SYSCTL_CHILDREN(sc->sysctl_tree), OID_AUTO, "cbmem_refs",
		    CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
		    "Additional CBMEM reference addresses");

	/* Walk the leaf table and register matching entries */
	for (i = 0; i < nitems(cb_leaves); i++) {
		const struct cb_sysctl_leaf *leaf = &cb_leaves[i];
		enum cb_sysctl_node nid = leaf->parent;

		if (!cb_sysctl_guard_check(leaf, sc))
			continue;

		/* Lazily create the parent node if not yet instantiated */
		if (nodes[nid] == NULL) {
			const struct cb_sysctl_node_desc *nd;
			uint32_t j;

			for (j = 0; j < nitems(cb_nodes); j++) {
				if (cb_nodes[j].id == nid)
					break;
			}
			if (j >= nitems(cb_nodes))
				continue;
			nd = &cb_nodes[j];
			nodes[nid] = SYSCTL_ADD_NODE(&sc->sysctl_ctx,
			    SYSCTL_CHILDREN(nodes[nd->parent]), OID_AUTO,
			    nd->name, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
			    nd->desc);
			if (nodes[nid] == NULL)
				continue;
		}

		cb_sysctl_add_leaf(&sc->sysctl_ctx, nodes[nid], leaf, sc);
	}

	/* --- Dynamic entries that don't fit the static table --- */

	/* CBMEM entry enumeration */
	if (sc->cbmem_count > 0) {
		oid_cbmem = SYSCTL_ADD_NODE(&sc->sysctl_ctx,
		    SYSCTL_CHILDREN(sc->sysctl_tree), OID_AUTO, "cbmem",
		    CTLFLAG_RD | CTLFLAG_MPSAFE, 0, "CBMEM entries");

		for (i = 0; i < sc->cbmem_count; i++) {
			struct cbmem_entry_info *info = &sc->cbmem_entries[i];

			snprintf(numstr, sizeof(numstr), "%u", i);
			oid_entry = SYSCTL_ADD_NODE(&sc->sysctl_ctx,
			    SYSCTL_CHILDREN(oid_cbmem), OID_AUTO, numstr,
			    CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
			    "CBMEM entry");

			SYSCTL_ADD_STRING(&sc->sysctl_ctx,
			    SYSCTL_CHILDREN(oid_entry), OID_AUTO, "name",
			    CTLFLAG_RD, info->name, 0, "Entry name");

			SYSCTL_ADD_U32(&sc->sysctl_ctx,
			    SYSCTL_CHILDREN(oid_entry), OID_AUTO, "id",
			    CTLFLAG_RD, &info->id, 0, "Entry ID (hex)");

			SYSCTL_ADD_U64(&sc->sysctl_ctx,
			    SYSCTL_CHILDREN(oid_entry), OID_AUTO, "address",
			    CTLFLAG_RD, (uint64_t *)&info->address, 0,
			    "Physical address");

			SYSCTL_ADD_U32(&sc->sysctl_ctx,
			    SYSCTL_CHILDREN(oid_entry), OID_AUTO, "size",
			    CTLFLAG_RD, &info->size, 0, "Entry size");
		}
	}

	/* Factory MAC addresses */
	if (sc->mac_count > 0) {
		oid_mac = SYSCTL_ADD_NODE(&sc->sysctl_ctx,
		    SYSCTL_CHILDREN(sc->sysctl_tree), OID_AUTO, "mac",
		    CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
		    "Factory MAC addresses");

		for (i = 0; i < sc->mac_count; i++) {
			uint8_t *m = sc->macs[i].mac_addr;

			snprintf(numstr, sizeof(numstr), "%u", i);
			snprintf(sc->mac_strs[i], sizeof(sc->mac_strs[i]),
			    "%02x:%02x:%02x:%02x:%02x:%02x",
			    m[0], m[1], m[2], m[3], m[4], m[5]);

			SYSCTL_ADD_STRING(&sc->sysctl_ctx,
			    SYSCTL_CHILDREN(oid_mac), OID_AUTO, numstr,
			    CTLFLAG_RD, sc->mac_strs[i], 0,
			    "MAC address");
		}
	}

	/* GPIO pins */
	if (sc->gpio_count > 0) {
		oid_gpio = SYSCTL_ADD_NODE(&sc->sysctl_ctx,
		    SYSCTL_CHILDREN(sc->sysctl_tree), OID_AUTO, "gpio",
		    CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
		    "GPIO pin states");

		for (i = 0; i < sc->gpio_count; i++) {
			struct cb_gpio *g = &sc->gpios[i];

			snprintf(numstr, sizeof(numstr), "%u", i);
			oid_pin = SYSCTL_ADD_NODE(&sc->sysctl_ctx,
			    SYSCTL_CHILDREN(oid_gpio), OID_AUTO, numstr,
			    CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
			    "GPIO pin");

			/* Ensure name is NUL-terminated */
			g->name[sizeof(g->name) - 1] = '\0';
			SYSCTL_ADD_STRING(&sc->sysctl_ctx,
			    SYSCTL_CHILDREN(oid_pin), OID_AUTO, "name",
			    CTLFLAG_RD, g->name, 0, "Pin name");

			SYSCTL_ADD_U32(&sc->sysctl_ctx,
			    SYSCTL_CHILDREN(oid_pin), OID_AUTO, "port",
			    CTLFLAG_RD, &g->port, 0, "Port number");

			SYSCTL_ADD_U32(&sc->sysctl_ctx,
			    SYSCTL_CHILDREN(oid_pin), OID_AUTO, "value",
			    CTLFLAG_RD, &g->value, 0, "Pin value");

			SYSCTL_ADD_U32(&sc->sysctl_ctx,
			    SYSCTL_CHILDREN(oid_pin), OID_AUTO, "polarity",
			    CTLFLAG_RD, &g->polarity, 0, "Pin polarity");
		}
	}

	/* Timestamp PROC sysctl */
	if (sc->has_timestamps)
		coreboot_timestamps_register(sc, sc->sysctl_tree);
}

/*
 * Map and validate a coreboot table at physical address pa.
 * On success, *vap points to the mapped table and *sizep is the total size.
 * The caller must pmap_unmapbios(*vap, *sizep) when done.
 */
static int
coreboot_map_table(vm_paddr_t pa, void **vap, vm_size_t *sizep)
{
	struct cb_header *hdr;
	void *va;
	vm_size_t map_size;
	int error;

	va = pmap_mapbios(pa, sizeof(struct cb_header));
	if (va == NULL)
		return (ENOMEM);

	hdr = (struct cb_header *)va;
	if (memcmp(hdr->signature, CB_HEADER_SIGNATURE,
	    CB_HEADER_SIG_LEN) != 0) {
		pmap_unmapbios(va, sizeof(struct cb_header));
		return (ENXIO);
	}

	error = coreboot_sanitize_header(hdr, &map_size);
	pmap_unmapbios(va, sizeof(struct cb_header));
	if (error != 0)
		return (ENXIO);

	va = pmap_mapbios(pa, map_size);
	if (va == NULL)
		return (ENOMEM);

	hdr = (struct cb_header *)va;
	if (coreboot_validate_header(hdr, map_size) != 0 ||
	    coreboot_validate_table(hdr, map_size) != 0) {
		pmap_unmapbios(va, map_size);
		return (ENXIO);
	}

	*vap = va;
	*sizep = map_size;
	return (0);
}

/*
 * Identify: scan low memory for "LBIO" signature and register a child
 */
static void
coreboot_identify(driver_t *driver, device_t parent)
{
	vm_paddr_t low_addr, real_addr;
	struct cb_header *hdr;
	uint8_t *entry, *table_end;
	struct cb_record *rec;
	device_t child;
	void *va;
	vm_size_t map_size;
	int error;

	if (!device_is_alive(parent))
		return;

	if (device_find_child(parent, "coreboot", -1) != NULL)
		return;

	low_addr = coreboot_scan_region(CB_SCAN_LOW_START, CB_SCAN_LOW_END);
	if (low_addr == 0)
		return;

	va = pmap_mapbios(low_addr, sizeof(struct cb_header));
	if (va == NULL)
		return;

	/* Scan already verified the signature; re-read to get sizes. */
	hdr = (struct cb_header *)va;
	error = coreboot_sanitize_header(hdr, &map_size);
	pmap_unmapbios(va, sizeof(struct cb_header));
	if (error != 0)
		return;

	va = pmap_mapbios(low_addr, map_size);
	if (va == NULL)
		return;

	hdr = (struct cb_header *)va;
	if (coreboot_validate_header(hdr, map_size) != 0 ||
	    coreboot_validate_table(hdr, map_size) != 0) {
		pmap_unmapbios(va, map_size);
		return;
	}

	/* Look for CB_TAG_FORWARD to find the real table in high memory */
	real_addr = low_addr;
	entry = (uint8_t *)hdr + hdr->header_bytes;
	table_end = entry + hdr->table_bytes;
	while ((size_t)(table_end - entry) >= sizeof(struct cb_record)) {
		size_t rec_size;

		rec = (struct cb_record *)entry;
		rec_size = rec->size;
		if (rec_size < sizeof(struct cb_record))
			break;
		if (rec_size > (size_t)(table_end - entry))
			break;
		if (rec->tag == CB_TAG_FORWARD) {
			if (rec_size >= sizeof(struct cb_forward)) {
				struct cb_forward *fwd;

				fwd = (struct cb_forward *)entry;
				real_addr = (vm_paddr_t)fwd->forward;
			}
			break;
		}
		entry += rec_size;
	}
	pmap_unmapbios(va, map_size);

	child = BUS_ADD_CHILD(parent, 5, "coreboot", DEVICE_UNIT_ANY);
	if (child == NULL)
		return;
	device_set_driver(child, driver);

	bus_set_resource(child, SYS_RES_MEMORY, 0, real_addr, PAGE_SIZE);
	device_set_desc(child, "coreboot firmware table");
}

/*
 * Probe: validate the coreboot header at the discovered address
 */
static int
coreboot_probe(device_t dev)
{
	vm_paddr_t pa;
	void *va;
	vm_size_t map_size;
	int error;

	pa = bus_get_resource_start(dev, SYS_RES_MEMORY, 0);
	if (pa == 0)
		return (ENXIO);

	error = coreboot_map_table(pa, &va, &map_size);
	if (error != 0)
		return (error);

	pmap_unmapbios(va, map_size);
	return (BUS_PROBE_SPECIFIC);
}

/*
 * Attach: map the full table, parse records, register sysctls and cdevs
 */
static int
coreboot_attach(device_t dev)
{
	struct coreboot_softc *sc;
	struct cb_header *hdr;
	vm_paddr_t pa;
	void *va;
	vm_size_t map_size;
	int error;

	sc = device_get_softc(dev);
	sc->dev = dev;

	pa = bus_get_resource_start(dev, SYS_RES_MEMORY, 0);

	error = coreboot_map_table(pa, &va, &map_size);
	if (error != 0) {
		device_printf(dev, "coreboot table validation failed at %#jx\n",
		    (uintmax_t)pa);
		return (error);
	}

	sc->table_paddr = pa;
	sc->table_size = map_size;
	sc->table_vaddr = va;

	hdr = (struct cb_header *)va;
	device_printf(dev, "coreboot table at %#jx (%u entries, %u bytes)\n",
	    (uintmax_t)pa, hdr->table_entries, hdr->table_bytes);

	coreboot_parse_table(sc, hdr);

	if (sc->version[0] != '\0')
		device_printf(dev, "firmware: %s\n", sc->version);
	if (sc->mb_vendor[0] != '\0')
		device_printf(dev, "mainboard: %s %s\n", sc->mb_vendor,
		    sc->mb_part);
	if (sc->has_console)
		device_printf(dev, "CBMEM console at %#jx\n",
		    (uintmax_t)sc->console_paddr);
	device_printf(dev, "CBMEM entries: %u\n", sc->cbmem_count);
	if (sc->has_board_config)
		device_printf(dev,
		    "board: id=%u sku=%u fw_config=%#jx\n",
		    sc->board_id, sc->sku_id,
		    (uintmax_t)sc->fw_config);
	if (sc->mac_count > 0)
		device_printf(dev, "factory MAC addresses: %u\n",
		    sc->mac_count);
	if (sc->has_acpi_rsdp)
		device_printf(dev, "ACPI RSDP at %#jx\n",
		    (uintmax_t)sc->acpi_rsdp);
	if (sc->has_pcie)
		device_printf(dev, "PCIe controller at %#jx\n",
		    (uintmax_t)sc->pcie_ctrl_base);
	if (sc->has_boot_media)
		device_printf(dev, "boot media: %#jx bytes, CBFS %#jx+%#jx\n",
		    (uintmax_t)sc->boot_media_size,
		    (uintmax_t)sc->cbfs_offset, (uintmax_t)sc->cbfs_size);
	if (sc->has_mmc_info)
		device_printf(dev, "MMC early CMD1 status: %d\n",
		    sc->mmc_early_cmd1_status);

	if (bootverbose) {
		if (sc->has_spi_flash)
			device_printf(dev,
			    "SPI flash: %u bytes, sector %u, erase %#x\n",
			    sc->spi_flash_size, sc->spi_sector_size,
			    sc->spi_erase_cmd);
		if (sc->has_console_type)
			device_printf(dev, "console type: %u\n",
			    sc->console_type);
		if (sc->has_framebuffer)
			device_printf(dev,
			    "framebuffer: %ux%u@%ubpp at %#jx\n",
			    sc->fb_x_res, sc->fb_y_res, sc->fb_bpp,
			    (uintmax_t)sc->fb_addr);
		if (sc->gpio_count > 0)
			device_printf(dev, "GPIO pins: %u\n",
			    sc->gpio_count);
		if (sc->has_tpm)
			device_printf(dev, "TPM %u.%u PPI at %#x\n",
			    sc->tpm_version == 2 ? 2 : 1,
			    sc->tpm_version == 2 ? 0 : 2,
			    sc->tpm_ppi_addr);
	}

	if (coreboot_debug) {
		if (sc->has_smmstore)
			device_printf(dev,
			    "SMMSTORE v2: %u blocks x %u bytes, "
			    "apm_cmd=%#x\n",
			    sc->smmstore_num_blocks,
			    sc->smmstore_block_size,
			    sc->smmstore_apm_cmd);
		if (sc->has_timestamps)
			device_printf(dev, "timestamps at %#jx\n",
			    (uintmax_t)sc->timestamps_paddr);
		if (sc->has_tpm_log)
			device_printf(dev, "TPM CB log at %#jx\n",
			    (uintmax_t)sc->tpm_log_paddr);
		if (sc->has_fmap)
			device_printf(dev, "FMAP at %#jx\n",
			    (uintmax_t)sc->fmap_paddr);
	}

	coreboot_register_sysctls(sc);

	coreboot_sc = sc;

	if (sc->has_console) {
		error = coreboot_console_create(sc);
		if (error != 0)
			device_printf(dev,
			    "failed to create /dev/coreboot_console (%d)\n",
			    error);
	}
	if (sc->cbmem_count > 0) {
		error = coreboot_cbmem_create(sc);
		if (error != 0)
			device_printf(dev, "failed to create /dev/cbmem (%d)\n",
			    error);
	}

	return (0);
}

/*
 * Detach: unmap table, destroy cdevs and sysctls
 */
static int
coreboot_detach(device_t dev)
{
	struct coreboot_softc *sc;

	sc = device_get_softc(dev);

	coreboot_cbmem_destroy(sc);
	coreboot_console_destroy(sc);

	coreboot_sc = NULL;

	sysctl_ctx_free(&sc->sysctl_ctx);

	if (sc->table_vaddr != NULL) {
		pmap_unmapbios(sc->table_vaddr, sc->table_size);
		sc->table_vaddr = NULL;
	}

	if (sc->console_vaddr != NULL) {
		pmap_unmapbios(sc->console_vaddr, sc->console_size);
		sc->console_vaddr = NULL;
		sc->console_size = 0;
		sc->console_data_size = 0;
	}

	return (0);
}

static int
coreboot_modevent(module_t mod, int what, void *arg)
{
	device_t *devs;
	int count, i;

	switch (what) {
	case MOD_LOAD:
		break;
	case MOD_UNLOAD:
		devclass_get_devices(devclass_find("coreboot"), &devs, &count);
		for (i = 0; i < count; i++)
			device_delete_child(device_get_parent(devs[i]),
			    devs[i]);
		free(devs, M_TEMP);
		break;
	default:
		break;
	}

	return (0);
}

static device_method_t coreboot_methods[] = {
	DEVMETHOD(device_identify,	coreboot_identify),
	DEVMETHOD(device_probe,		coreboot_probe),
	DEVMETHOD(device_attach,	coreboot_attach),
	DEVMETHOD(device_detach,	coreboot_detach),
	DEVMETHOD_END
};

static driver_t coreboot_driver = {
	"coreboot",
	coreboot_methods,
	sizeof(struct coreboot_softc),
};

DRIVER_MODULE(coreboot, nexus, coreboot_driver, coreboot_modevent, NULL);
MODULE_VERSION(coreboot, 1);
