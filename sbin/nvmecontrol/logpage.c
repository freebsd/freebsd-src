/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2013 EMC Corp.
 * All rights reserved.
 *
 * Copyright (C) 2012-2013 Intel Corporation
 * All rights reserved.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/ioccom.h>

#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/endian.h>

#include "nvmecontrol.h"

#define DEFAULT_SIZE	(4096)
#define MAX_FW_SLOTS	(7)

typedef void (*print_fn_t)(const struct nvme_controller_data *cdata, void *buf, uint32_t size);

struct kv_name
{
	uint32_t key;
	const char *name;
};

static const char *
kv_lookup(const struct kv_name *kv, size_t kv_count, uint32_t key)
{
	static char bad[32];
	size_t i;

	for (i = 0; i < kv_count; i++, kv++)
		if (kv->key == key)
			return kv->name;
	snprintf(bad, sizeof(bad), "Attribute %#x", key);
	return bad;
}

static void
print_log_hex(const struct nvme_controller_data *cdata __unused, void *data, uint32_t length)
{

	print_hex(data, length);
}

static void
print_bin(const struct nvme_controller_data *cdata __unused, void *data, uint32_t length)
{

	write(STDOUT_FILENO, data, length);
}

static void *
get_log_buffer(uint32_t size)
{
	void	*buf;

	if ((buf = malloc(size)) == NULL)
		errx(1, "unable to malloc %u bytes", size);

	memset(buf, 0, size);
	return (buf);
}

void
read_logpage(int fd, uint8_t log_page, uint32_t nsid, void *payload,
    uint32_t payload_size)
{
	struct nvme_pt_command	pt;
	struct nvme_error_information_entry	*err_entry;
	int i, err_pages;

	memset(&pt, 0, sizeof(pt));
	pt.cmd.opc_fuse = NVME_CMD_SET_OPC(NVME_OPC_GET_LOG_PAGE);
	pt.cmd.nsid = htole32(nsid);
	pt.cmd.cdw10 = ((payload_size/sizeof(uint32_t)) - 1) << 16;
	pt.cmd.cdw10 |= log_page;
	pt.cmd.cdw10 = htole32(pt.cmd.cdw10);
	pt.buf = payload;
	pt.len = payload_size;
	pt.is_read = 1;

	if (ioctl(fd, NVME_PASSTHROUGH_CMD, &pt) < 0)
		err(1, "get log page request failed");

	/* Convert data to host endian */
	switch (log_page) {
	case NVME_LOG_ERROR:
		err_entry = (struct nvme_error_information_entry *)payload;
		err_pages = payload_size / sizeof(struct nvme_error_information_entry);
		for (i = 0; i < err_pages; i++)
			nvme_error_information_entry_swapbytes(err_entry++);
		break;
	case NVME_LOG_HEALTH_INFORMATION:
		nvme_health_information_page_swapbytes(
		    (struct nvme_health_information_page *)payload);
		break;
	case NVME_LOG_FIRMWARE_SLOT:
		nvme_firmware_page_swapbytes(
		    (struct nvme_firmware_page *)payload);
		break;
	case INTEL_LOG_TEMP_STATS:
		intel_log_temp_stats_swapbytes(
		    (struct intel_log_temp_stats *)payload);
		break;
	default:
		break;
	}

	if (nvme_completion_is_error(&pt.cpl))
		errx(1, "get log page request returned error");
}

static void
print_log_error(const struct nvme_controller_data *cdata __unused, void *buf, uint32_t size)
{
	int					i, nentries;
	uint16_t				status;
	uint8_t					p, sc, sct, m, dnr;
	struct nvme_error_information_entry	*entry = buf;

	printf("Error Information Log\n");
	printf("=====================\n");

	if (entry->error_count == 0) {
		printf("No error entries found\n");
		return;
	}

	nentries = size/sizeof(struct nvme_error_information_entry);
	for (i = 0; i < nentries; i++, entry++) {
		if (entry->error_count == 0)
			break;

		status = entry->status;

		p = NVME_STATUS_GET_P(status);
		sc = NVME_STATUS_GET_SC(status);
		sct = NVME_STATUS_GET_SCT(status);
		m = NVME_STATUS_GET_M(status);
		dnr = NVME_STATUS_GET_DNR(status);

		printf("Entry %02d\n", i + 1);
		printf("=========\n");
		printf(" Error count:          %ju\n", entry->error_count);
		printf(" Submission queue ID:  %u\n", entry->sqid);
		printf(" Command ID:           %u\n", entry->cid);
		/* TODO: Export nvme_status_string structures from kernel? */
		printf(" Status:\n");
		printf("  Phase tag:           %d\n", p);
		printf("  Status code:         %d\n", sc);
		printf("  Status code type:    %d\n", sct);
		printf("  More:                %d\n", m);
		printf("  DNR:                 %d\n", dnr);
		printf(" Error location:       %u\n", entry->error_location);
		printf(" LBA:                  %ju\n", entry->lba);
		printf(" Namespace ID:         %u\n", entry->nsid);
		printf(" Vendor specific info: %u\n", entry->vendor_specific);
	}
}

static void
print_temp(uint16_t t)
{
	printf("%u K, %2.2f C, %3.2f F\n", t, (float)t - 273.15, (float)t * 9 / 5 - 459.67);
}


static void
print_log_health(const struct nvme_controller_data *cdata __unused, void *buf, uint32_t size __unused)
{
	struct nvme_health_information_page *health = buf;
	char cbuf[UINT128_DIG + 1];
	uint8_t	warning;
	int i;

	warning = health->critical_warning;

	printf("SMART/Health Information Log\n");
	printf("============================\n");

	printf("Critical Warning State:         0x%02x\n", warning);
	printf(" Available spare:               %d\n",
	    !!(warning & NVME_CRIT_WARN_ST_AVAILABLE_SPARE));
	printf(" Temperature:                   %d\n",
	    !!(warning & NVME_CRIT_WARN_ST_TEMPERATURE));
	printf(" Device reliability:            %d\n",
	    !!(warning & NVME_CRIT_WARN_ST_DEVICE_RELIABILITY));
	printf(" Read only:                     %d\n",
	    !!(warning & NVME_CRIT_WARN_ST_READ_ONLY));
	printf(" Volatile memory backup:        %d\n",
	    !!(warning & NVME_CRIT_WARN_ST_VOLATILE_MEMORY_BACKUP));
	printf("Temperature:                    ");
	print_temp(health->temperature);
	printf("Available spare:                %u\n",
	    health->available_spare);
	printf("Available spare threshold:      %u\n",
	    health->available_spare_threshold);
	printf("Percentage used:                %u\n",
	    health->percentage_used);

	printf("Data units (512,000 byte) read: %s\n",
	    uint128_to_str(to128(health->data_units_read), cbuf, sizeof(cbuf)));
	printf("Data units written:             %s\n",
	    uint128_to_str(to128(health->data_units_written), cbuf, sizeof(cbuf)));
	printf("Host read commands:             %s\n",
	    uint128_to_str(to128(health->host_read_commands), cbuf, sizeof(cbuf)));
	printf("Host write commands:            %s\n",
	    uint128_to_str(to128(health->host_write_commands), cbuf, sizeof(cbuf)));
	printf("Controller busy time (minutes): %s\n",
	    uint128_to_str(to128(health->controller_busy_time), cbuf, sizeof(cbuf)));
	printf("Power cycles:                   %s\n",
	    uint128_to_str(to128(health->power_cycles), cbuf, sizeof(cbuf)));
	printf("Power on hours:                 %s\n",
	    uint128_to_str(to128(health->power_on_hours), cbuf, sizeof(cbuf)));
	printf("Unsafe shutdowns:               %s\n",
	    uint128_to_str(to128(health->unsafe_shutdowns), cbuf, sizeof(cbuf)));
	printf("Media errors:                   %s\n",
	    uint128_to_str(to128(health->media_errors), cbuf, sizeof(cbuf)));
	printf("No. error info log entries:     %s\n",
	    uint128_to_str(to128(health->num_error_info_log_entries), cbuf, sizeof(cbuf)));

	printf("Warning Temp Composite Time:    %d\n", health->warning_temp_time);
	printf("Error Temp Composite Time:      %d\n", health->error_temp_time);
	for (i = 0; i < 8; i++) {
		if (health->temp_sensor[i] == 0)
			continue;
		printf("Temperature Sensor %d:           ", i + 1);
		print_temp(health->temp_sensor[i]);
	}
}

static void
print_log_firmware(const struct nvme_controller_data *cdata, void *buf, uint32_t size __unused)
{
	int				i, slots;
	const char			*status;
	struct nvme_firmware_page	*fw = buf;
	uint8_t				afi_slot;
	uint16_t			oacs_fw;
	uint8_t				fw_num_slots;

	afi_slot = fw->afi >> NVME_FIRMWARE_PAGE_AFI_SLOT_SHIFT;
	afi_slot &= NVME_FIRMWARE_PAGE_AFI_SLOT_MASK;

	oacs_fw = (cdata->oacs >> NVME_CTRLR_DATA_OACS_FIRMWARE_SHIFT) &
		NVME_CTRLR_DATA_OACS_FIRMWARE_MASK;
	fw_num_slots = (cdata->frmw >> NVME_CTRLR_DATA_FRMW_NUM_SLOTS_SHIFT) &
		NVME_CTRLR_DATA_FRMW_NUM_SLOTS_MASK;

	printf("Firmware Slot Log\n");
	printf("=================\n");

	if (oacs_fw == 0)
		slots = 1;
	else
		slots = MIN(fw_num_slots, MAX_FW_SLOTS);

	for (i = 0; i < slots; i++) {
		printf("Slot %d: ", i + 1);
		if (afi_slot == i + 1)
			status = "  Active";
		else
			status = "Inactive";

		if (fw->revision[i] == 0LLU)
			printf("Empty\n");
		else
			if (isprint(*(char *)&fw->revision[i]))
				printf("[%s] %.8s\n", status,
				    (char *)&fw->revision[i]);
			else
				printf("[%s] %016jx\n", status,
				    fw->revision[i]);
	}
}

/*
 * Intel specific log pages from
 * http://www.intel.com/content/dam/www/public/us/en/documents/product-specifications/ssd-dc-p3700-spec.pdf
 *
 * Though the version as of this date has a typo for the size of log page 0xca,
 * offset 147: it is only 1 byte, not 6.
 */
static void
print_intel_temp_stats(const struct nvme_controller_data *cdata __unused, void *buf, uint32_t size __unused)
{
	struct intel_log_temp_stats	*temp = buf;

	printf("Intel Temperature Log\n");
	printf("=====================\n");

	printf("Current:                        ");
	print_temp(temp->current);
	printf("Overtemp Last Flags             %#jx\n", (uintmax_t)temp->overtemp_flag_last);
	printf("Overtemp Lifetime Flags         %#jx\n", (uintmax_t)temp->overtemp_flag_life);
	printf("Max Temperature                 ");
	print_temp(temp->max_temp);
	printf("Min Temperature                 ");
	print_temp(temp->min_temp);
	printf("Max Operating Temperature       ");
	print_temp(temp->max_oper_temp);
	printf("Min Operating Temperature       ");
	print_temp(temp->min_oper_temp);
	printf("Estimated Temperature Offset:   %ju C/K\n", (uintmax_t)temp->est_offset);
}

/*
 * Format from Table 22, section 5.7 IO Command Latency Statistics.
 * Read and write stats pages have identical encoding.
 */
static void
print_intel_read_write_lat_log(const struct nvme_controller_data *cdata __unused, void *buf, uint32_t size __unused)
{
	const char *walker = buf;
	int i;

	printf("Major:                         %d\n", le16dec(walker + 0));
	printf("Minor:                         %d\n", le16dec(walker + 2));
	for (i = 0; i < 32; i++)
		printf("%4dus-%4dus:                 %ju\n", i * 32, (i + 1) * 32, (uintmax_t)le32dec(walker + 4 + i * 4));
	for (i = 1; i < 32; i++)
		printf("%4dms-%4dms:                 %ju\n", i, i + 1, (uintmax_t)le32dec(walker + 132 + i * 4));
	for (i = 1; i < 32; i++)
		printf("%4dms-%4dms:                 %ju\n", i * 32, (i + 1) * 32, (uintmax_t)le32dec(walker + 256 + i * 4));
}

static void
print_intel_read_lat_log(const struct nvme_controller_data *cdata __unused, void *buf, uint32_t size)
{

	printf("Intel Read Latency Log\n");
	printf("======================\n");
	print_intel_read_write_lat_log(cdata, buf, size);
}

static void
print_intel_write_lat_log(const struct nvme_controller_data *cdata __unused, void *buf, uint32_t size)
{

	printf("Intel Write Latency Log\n");
	printf("=======================\n");
	print_intel_read_write_lat_log(cdata, buf, size);
}

/*
 * Table 19. 5.4 SMART Attributes. Samsung also implements this and some extra data not documented.
 */
static void
print_intel_add_smart(const struct nvme_controller_data *cdata __unused, void *buf, uint32_t size __unused)
{
	uint8_t *walker = buf;
	uint8_t *end = walker + 150;
	const char *name;
	uint64_t raw;
	uint8_t normalized;

	static struct kv_name kv[] =
	{
		{ 0xab, "Program Fail Count" },
		{ 0xac, "Erase Fail Count" },
		{ 0xad, "Wear Leveling Count" },
		{ 0xb8, "End to End Error Count" },
		{ 0xc7, "CRC Error Count" },
		{ 0xe2, "Timed: Media Wear" },
		{ 0xe3, "Timed: Host Read %" },
		{ 0xe4, "Timed: Elapsed Time" },
		{ 0xea, "Thermal Throttle Status" },
		{ 0xf0, "Retry Buffer Overflows" },
		{ 0xf3, "PLL Lock Loss Count" },
		{ 0xf4, "NAND Bytes Written" },
		{ 0xf5, "Host Bytes Written" },
	};

	printf("Additional SMART Data Log\n");
	printf("=========================\n");
	/*
	 * walker[0] = Key
	 * walker[1,2] = reserved
	 * walker[3] = Normalized Value
	 * walker[4] = reserved
	 * walker[5..10] = Little Endian Raw value
	 *	(or other represenations)
	 * walker[11] = reserved
	 */
	while (walker < end) {
		name = kv_lookup(kv, nitems(kv), *walker);
		normalized = walker[3];
		raw = le48dec(walker + 5);
		switch (*walker){
		case 0:
			break;
		case 0xad:
			printf("%-32s: %3d min: %u max: %u ave: %u\n", name, normalized,
			    le16dec(walker + 5), le16dec(walker + 7), le16dec(walker + 9));
			break;
		case 0xe2:
			printf("%-32s: %3d %.3f%%\n", name, normalized, raw / 1024.0);
			break;
		case 0xea:
			printf("%-32s: %3d %d%% %d times\n", name, normalized, walker[5], le32dec(walker+6));
			break;
		default:
			printf("%-32s: %3d %ju\n", name, normalized, (uintmax_t)raw);
			break;
		}
		walker += 12;
	}
}

/*
 * HGST's 0xc1 page. This is a grab bag of additional data. Please see
 * https://www.hgst.com/sites/default/files/resources/US_SN150_ProdManual.pdf
 * https://www.hgst.com/sites/default/files/resources/US_SN100_ProdManual.pdf
 * Appendix A for details
 */

typedef void (*subprint_fn_t)(void *buf, uint16_t subtype, uint8_t res, uint32_t size);

struct subpage_print
{
	uint16_t key;
	subprint_fn_t fn;
};

static void print_hgst_info_write_errors(void *buf, uint16_t subtype, uint8_t res, uint32_t size);
static void print_hgst_info_read_errors(void *buf, uint16_t subtype, uint8_t res, uint32_t size);
static void print_hgst_info_verify_errors(void *buf, uint16_t subtype, uint8_t res, uint32_t size);
static void print_hgst_info_self_test(void *buf, uint16_t subtype, uint8_t res, uint32_t size);
static void print_hgst_info_background_scan(void *buf, uint16_t subtype, uint8_t res, uint32_t size);
static void print_hgst_info_erase_errors(void *buf, uint16_t subtype, uint8_t res, uint32_t size);
static void print_hgst_info_erase_counts(void *buf, uint16_t subtype, uint8_t res, uint32_t size);
static void print_hgst_info_temp_history(void *buf, uint16_t subtype, uint8_t res, uint32_t size);
static void print_hgst_info_ssd_perf(void *buf, uint16_t subtype, uint8_t res, uint32_t size);
static void print_hgst_info_firmware_load(void *buf, uint16_t subtype, uint8_t res, uint32_t size);

static struct subpage_print hgst_subpage[] = {
	{ 0x02, print_hgst_info_write_errors },
	{ 0x03, print_hgst_info_read_errors },
	{ 0x05, print_hgst_info_verify_errors },
	{ 0x10, print_hgst_info_self_test },
	{ 0x15, print_hgst_info_background_scan },
	{ 0x30, print_hgst_info_erase_errors },
	{ 0x31, print_hgst_info_erase_counts },
	{ 0x32, print_hgst_info_temp_history },
	{ 0x37, print_hgst_info_ssd_perf },
	{ 0x38, print_hgst_info_firmware_load },
};

/* Print a subpage that is basically just key value pairs */
static void
print_hgst_info_subpage_gen(void *buf, uint16_t subtype __unused, uint32_t size,
    const struct kv_name *kv, size_t kv_count)
{
	uint8_t *wsp, *esp;
	uint16_t ptype;
	uint8_t plen;
	uint64_t param;
	int i;

	wsp = buf;
	esp = wsp + size;
	while (wsp < esp) {
		ptype = le16dec(wsp);
		wsp += 2;
		wsp++;			/* Flags, just ignore */
		plen = *wsp++;
		param = 0;
		for (i = 0; i < plen; i++)
			param |= (uint64_t)*wsp++ << (i * 8);
		printf("  %-30s: %jd\n", kv_lookup(kv, kv_count, ptype), (uintmax_t)param);
	}
}

static void
print_hgst_info_write_errors(void *buf, uint16_t subtype, uint8_t res __unused, uint32_t size)
{
	static struct kv_name kv[] =
	{
		{ 0x0000, "Corrected Without Delay" },
		{ 0x0001, "Corrected Maybe Delayed" },
		{ 0x0002, "Re-Writes" },
		{ 0x0003, "Errors Corrected" },
		{ 0x0004, "Correct Algorithm Used" },
		{ 0x0005, "Bytes Processed" },
		{ 0x0006, "Uncorrected Errors" },
		{ 0x8000, "Flash Write Commands" },
		{ 0x8001, "HGST Special" },
	};

	printf("Write Errors Subpage:\n");
	print_hgst_info_subpage_gen(buf, subtype, size, kv, nitems(kv));
}

static void
print_hgst_info_read_errors(void *buf, uint16_t subtype, uint8_t res __unused, uint32_t size)
{
	static struct kv_name kv[] =
	{
		{ 0x0000, "Corrected Without Delay" },
		{ 0x0001, "Corrected Maybe Delayed" },
		{ 0x0002, "Re-Reads" },
		{ 0x0003, "Errors Corrected" },
		{ 0x0004, "Correct Algorithm Used" },
		{ 0x0005, "Bytes Processed" },
		{ 0x0006, "Uncorrected Errors" },
		{ 0x8000, "Flash Read Commands" },
		{ 0x8001, "XOR Recovered" },
		{ 0x8002, "Total Corrected Bits" },
	};

	printf("Read Errors Subpage:\n");
	print_hgst_info_subpage_gen(buf, subtype, size, kv, nitems(kv));
}

static void
print_hgst_info_verify_errors(void *buf, uint16_t subtype, uint8_t res __unused, uint32_t size)
{
	static struct kv_name kv[] =
	{
		{ 0x0000, "Corrected Without Delay" },
		{ 0x0001, "Corrected Maybe Delayed" },
		{ 0x0002, "Re-Reads" },
		{ 0x0003, "Errors Corrected" },
		{ 0x0004, "Correct Algorithm Used" },
		{ 0x0005, "Bytes Processed" },
		{ 0x0006, "Uncorrected Errors" },
		{ 0x8000, "Commands Processed" },
	};

	printf("Verify Errors Subpage:\n");
	print_hgst_info_subpage_gen(buf, subtype, size, kv, nitems(kv));
}

static void
print_hgst_info_self_test(void *buf, uint16_t subtype __unused, uint8_t res __unused, uint32_t size)
{
	size_t i;
	uint8_t *walker = buf;
	uint16_t code, hrs;
	uint32_t lba;

	printf("Self Test Subpage:\n");
	for (i = 0; i < size / 20; i++) {	/* Each entry is 20 bytes */
		code = le16dec(walker);
		walker += 2;
		walker++;			/* Ignore fixed flags */
		if (*walker == 0)		/* Last entry is zero length */
			break;
		if (*walker++ != 0x10) {
			printf("Bad length for self test report\n");
			return;
		}
		printf("  %-30s: %d\n", "Recent Test", code);
		printf("    %-28s: %#x\n", "Self-Test Results", *walker & 0xf);
		printf("    %-28s: %#x\n", "Self-Test Code", (*walker >> 5) & 0x7);
		walker++;
		printf("    %-28s: %#x\n", "Self-Test Number", *walker++);
		hrs = le16dec(walker);
		walker += 2;
		lba = le32dec(walker);
		walker += 4;
		printf("    %-28s: %u\n", "Total Power On Hrs", hrs);
		printf("    %-28s: %#jx (%jd)\n", "LBA", (uintmax_t)lba, (uintmax_t)lba);
		printf("    %-28s: %#x\n", "Sense Key", *walker++ & 0xf);
		printf("    %-28s: %#x\n", "Additional Sense Code", *walker++);
		printf("    %-28s: %#x\n", "Additional Sense Qualifier", *walker++);
		printf("    %-28s: %#x\n", "Vendor Specific Detail", *walker++);
	}
}

static void
print_hgst_info_background_scan(void *buf, uint16_t subtype __unused, uint8_t res __unused, uint32_t size)
{
	uint8_t *walker = buf;
	uint8_t status;
	uint16_t code, nscan, progress;
	uint32_t pom, nand;

	printf("Background Media Scan Subpage:\n");
	/* Decode the header */
	code = le16dec(walker);
	walker += 2;
	walker++;			/* Ignore fixed flags */
	if (*walker++ != 0x10) {
		printf("Bad length for background scan header\n");
		return;
	}
	if (code != 0) {
		printf("Expceted code 0, found code %#x\n", code);
		return;
	}
	pom = le32dec(walker);
	walker += 4;
	walker++;			/* Reserved */
	status = *walker++;
	nscan = le16dec(walker);
	walker += 2;
	progress = le16dec(walker);
	walker += 2;
	walker += 6;			/* Reserved */
	printf("  %-30s: %d\n", "Power On Minutes", pom);
	printf("  %-30s: %x (%s)\n", "BMS Status", status,
	    status == 0 ? "idle" : (status == 1 ? "active" : (status == 8 ? "suspended" : "unknown")));
	printf("  %-30s: %d\n", "Number of BMS", nscan);
	printf("  %-30s: %d\n", "Progress Current BMS", progress);
	/* Report retirements */
	if (walker - (uint8_t *)buf != 20) {
		printf("Coding error, offset not 20\n");
		return;
	}
	size -= 20;
	printf("  %-30s: %d\n", "BMS retirements", size / 0x18);
	while (size > 0) {
		code = le16dec(walker);
		walker += 2;
		walker++;
		if (*walker++ != 0x14) {
			printf("Bad length parameter\n");
			return;
		}
		pom = le32dec(walker);
		walker += 4;
		/*
		 * Spec sheet says the following are hard coded, if true, just
		 * print the NAND retirement.
		 */
		if (walker[0] == 0x41 &&
		    walker[1] == 0x0b &&
		    walker[2] == 0x01 &&
		    walker[3] == 0x00 &&
		    walker[4] == 0x00 &&
		    walker[5] == 0x00 &&
		    walker[6] == 0x00 &&
		    walker[7] == 0x00) {
			walker += 8;
			walker += 4;	/* Skip reserved */
			nand = le32dec(walker);
			walker += 4;
			printf("  %-30s: %d\n", "Retirement number", code);
			printf("    %-28s: %#x\n", "NAND (C/T)BBBPPP", nand);
		} else {
			printf("Parameter %#x entry corrupt\n", code);
			walker += 16;
		}
	}
}

static void
print_hgst_info_erase_errors(void *buf, uint16_t subtype __unused, uint8_t res __unused, uint32_t size)
{
	static struct kv_name kv[] =
	{
		{ 0x0000, "Corrected Without Delay" },
		{ 0x0001, "Corrected Maybe Delayed" },
		{ 0x0002, "Re-Erase" },
		{ 0x0003, "Errors Corrected" },
		{ 0x0004, "Correct Algorithm Used" },
		{ 0x0005, "Bytes Processed" },
		{ 0x0006, "Uncorrected Errors" },
		{ 0x8000, "Flash Erase Commands" },
		{ 0x8001, "Mfg Defect Count" },
		{ 0x8002, "Grown Defect Count" },
		{ 0x8003, "Erase Count -- User" },
		{ 0x8004, "Erase Count -- System" },
	};

	printf("Erase Errors Subpage:\n");
	print_hgst_info_subpage_gen(buf, subtype, size, kv, nitems(kv));
}

static void
print_hgst_info_erase_counts(void *buf, uint16_t subtype, uint8_t res __unused, uint32_t size)
{
	/* My drive doesn't export this -- so not coding up */
	printf("XXX: Erase counts subpage: %p, %#x %d\n", buf, subtype, size);
}

static void
print_hgst_info_temp_history(void *buf, uint16_t subtype __unused, uint8_t res __unused, uint32_t size __unused)
{
	uint8_t *walker = buf;
	uint32_t min;

	printf("Temperature History:\n");
	printf("  %-30s: %d C\n", "Current Temperature", *walker++);
	printf("  %-30s: %d C\n", "Reference Temperature", *walker++);
	printf("  %-30s: %d C\n", "Maximum Temperature", *walker++);
	printf("  %-30s: %d C\n", "Minimum Temperature", *walker++);
	min = le32dec(walker);
	walker += 4;
	printf("  %-30s: %d:%02d:00\n", "Max Temperature Time", min / 60, min % 60);
	min = le32dec(walker);
	walker += 4;
	printf("  %-30s: %d:%02d:00\n", "Over Temperature Duration", min / 60, min % 60);
	min = le32dec(walker);
	walker += 4;
	printf("  %-30s: %d:%02d:00\n", "Min Temperature Time", min / 60, min % 60);
}

static void
print_hgst_info_ssd_perf(void *buf, uint16_t subtype __unused, uint8_t res, uint32_t size __unused)
{
	uint8_t *walker = buf;
	uint64_t val;

	printf("SSD Performance Subpage Type %d:\n", res);
	val = le64dec(walker);
	walker += 8;
	printf("  %-30s: %ju\n", "Host Read Commands", val);
	val = le64dec(walker);
	walker += 8;
	printf("  %-30s: %ju\n", "Host Read Blocks", val);
	val = le64dec(walker);
	walker += 8;
	printf("  %-30s: %ju\n", "Host Cache Read Hits Commands", val);
	val = le64dec(walker);
	walker += 8;
	printf("  %-30s: %ju\n", "Host Cache Read Hits Blocks", val);
	val = le64dec(walker);
	walker += 8;
	printf("  %-30s: %ju\n", "Host Read Commands Stalled", val);
	val = le64dec(walker);
	walker += 8;
	printf("  %-30s: %ju\n", "Host Write Commands", val);
	val = le64dec(walker);
	walker += 8;
	printf("  %-30s: %ju\n", "Host Write Blocks", val);
	val = le64dec(walker);
	walker += 8;
	printf("  %-30s: %ju\n", "Host Write Odd Start Commands", val);
	val = le64dec(walker);
	walker += 8;
	printf("  %-30s: %ju\n", "Host Write Odd End Commands", val);
	val = le64dec(walker);
	walker += 8;
	printf("  %-30s: %ju\n", "Host Write Commands Stalled", val);
	val = le64dec(walker);
	walker += 8;
	printf("  %-30s: %ju\n", "NAND Read Commands", val);
	val = le64dec(walker);
	walker += 8;
	printf("  %-30s: %ju\n", "NAND Read Blocks", val);
	val = le64dec(walker);
	walker += 8;
	printf("  %-30s: %ju\n", "NAND Write Commands", val);
	val = le64dec(walker);
	walker += 8;
	printf("  %-30s: %ju\n", "NAND Write Blocks", val);
	val = le64dec(walker);
	walker += 8;
	printf("  %-30s: %ju\n", "NAND Read Before Writes", val);
}

static void
print_hgst_info_firmware_load(void *buf, uint16_t subtype __unused, uint8_t res __unused, uint32_t size __unused)
{
	uint8_t *walker = buf;

	printf("Firmware Load Subpage:\n");
	printf("  %-30s: %d\n", "Firmware Downloads", le32dec(walker));
}

static void
kv_indirect(void *buf, uint32_t subtype, uint8_t res, uint32_t size, struct subpage_print *sp, size_t nsp)
{
	size_t i;

	for (i = 0; i < nsp; i++, sp++) {
		if (sp->key == subtype) {
			sp->fn(buf, subtype, res, size);
			return;
		}
	}
	printf("No handler for page type %x\n", subtype);
}

static void
print_hgst_info_log(const struct nvme_controller_data *cdata __unused, void *buf, uint32_t size __unused)
{
	uint8_t	*walker, *end, *subpage;
	int pages;
	uint16_t len;
	uint8_t subtype, res;

	printf("HGST Extra Info Log\n");
	printf("===================\n");

	walker = buf;
	pages = *walker++;
	walker++;
	len = le16dec(walker);
	walker += 2;
	end = walker + len;		/* Length is exclusive of this header */
	
	while (walker < end) {
		subpage = walker + 4;
		subtype = *walker++ & 0x3f;	/* subtype */
		res = *walker++;		/* Reserved */
		len = le16dec(walker);
		walker += len + 2;		/* Length, not incl header */
		if (walker > end) {
			printf("Ooops! Off the end of the list\n");
			break;
		}
		kv_indirect(subpage, subtype, res, len, hgst_subpage, nitems(hgst_subpage));
	}
}

/*
 * Table of log page printer / sizing.
 *
 * This includes Intel specific pages that are widely implemented.
 * Make sure you keep all the pages of one vendor together so -v help
 * lists all the vendors pages.
 */
static struct logpage_function {
	uint8_t		log_page;
	const char     *vendor;
	const char     *name;
	print_fn_t	print_fn;
	size_t		size;
} logfuncs[] = {
	{NVME_LOG_ERROR,		NULL,	"Drive Error Log",
	 print_log_error,		0},
	{NVME_LOG_HEALTH_INFORMATION,	NULL,	"Health/SMART Data",
	 print_log_health,		sizeof(struct nvme_health_information_page)},
	{NVME_LOG_FIRMWARE_SLOT,	NULL,	"Firmware Information",
	 print_log_firmware,		sizeof(struct nvme_firmware_page)},
	{HGST_INFO_LOG,			"hgst",	"Detailed Health/SMART",
	 print_hgst_info_log,		DEFAULT_SIZE},
	{HGST_INFO_LOG,			"wds",	"Detailed Health/SMART",
	 print_hgst_info_log,		DEFAULT_SIZE},
	{INTEL_LOG_TEMP_STATS,		"intel", "Temperature Stats",
	 print_intel_temp_stats,	sizeof(struct intel_log_temp_stats)},
	{INTEL_LOG_READ_LAT_LOG,	"intel", "Read Latencies",
	 print_intel_read_lat_log,	DEFAULT_SIZE},
	{INTEL_LOG_WRITE_LAT_LOG,	"intel", "Write Latencies",
	 print_intel_write_lat_log,	DEFAULT_SIZE},
	{INTEL_LOG_ADD_SMART,		"intel", "Extra Health/SMART Data",
	 print_intel_add_smart,		DEFAULT_SIZE},
	{INTEL_LOG_ADD_SMART,		"samsung", "Extra Health/SMART Data",
	 print_intel_add_smart,		DEFAULT_SIZE},

	{0, NULL, NULL, NULL, 0},
};

static void
logpage_usage(void)
{
	fprintf(stderr, "usage:\n");
	fprintf(stderr, LOGPAGE_USAGE);
	exit(1);
}

static void
logpage_help(void)
{
	struct logpage_function		*f;
	const char 			*v;

	fprintf(stderr, "\n");
	fprintf(stderr, "%-8s %-10s %s\n", "Page", "Vendor","Page Name");
	fprintf(stderr, "-------- ---------- ----------\n");
	for (f = logfuncs; f->log_page > 0; f++) {
		v = f->vendor == NULL ? "-" : f->vendor;
		fprintf(stderr, "0x%02x     %-10s %s\n", f->log_page, v, f->name);
	}

	exit(1);
}

void
logpage(int argc, char *argv[])
{
	int				fd;
	int				log_page = 0, pageflag = false;
	int				binflag = false, hexflag = false, ns_specified;
	int				opt;
	char				*p;
	char				cname[64];
	uint32_t			nsid, size;
	void				*buf;
	const char			*vendor = NULL;
	struct logpage_function		*f;
	struct nvme_controller_data	cdata;
	print_fn_t			print_fn;
	uint8_t				ns_smart;

	while ((opt = getopt(argc, argv, "bp:xv:")) != -1) {
		switch (opt) {
		case 'b':
			binflag = true;
			break;
		case 'p':
			if (strcmp(optarg, "help") == 0)
				logpage_help();

			/* TODO: Add human-readable ASCII page IDs */
			log_page = strtol(optarg, &p, 0);
			if (p != NULL && *p != '\0') {
				fprintf(stderr,
				    "\"%s\" not valid log page id.\n",
				    optarg);
				logpage_usage();
			}
			pageflag = true;
			break;
		case 'x':
			hexflag = true;
			break;
		case 'v':
			if (strcmp(optarg, "help") == 0)
				logpage_help();
			vendor = optarg;
			break;
		}
	}

	if (!pageflag) {
		printf("Missing page_id (-p).\n");
		logpage_usage();
	}

	/* Check that a controller and/or namespace was specified. */
	if (optind >= argc)
		logpage_usage();

	if (strstr(argv[optind], NVME_NS_PREFIX) != NULL) {
		ns_specified = true;
		parse_ns_str(argv[optind], cname, &nsid);
		open_dev(cname, &fd, 1, 1);
	} else {
		ns_specified = false;
		nsid = NVME_GLOBAL_NAMESPACE_TAG;
		open_dev(argv[optind], &fd, 1, 1);
	}

	read_controller_data(fd, &cdata);

	ns_smart = (cdata.lpa >> NVME_CTRLR_DATA_LPA_NS_SMART_SHIFT) &
		NVME_CTRLR_DATA_LPA_NS_SMART_MASK;

	/*
	 * The log page attribtues indicate whether or not the controller
	 * supports the SMART/Health information log page on a per
	 * namespace basis.
	 */
	if (ns_specified) {
		if (log_page != NVME_LOG_HEALTH_INFORMATION)
			errx(1, "log page %d valid only at controller level",
			    log_page);
		if (ns_smart == 0)
			errx(1,
			    "controller does not support per namespace "
			    "smart/health information");
	}

	print_fn = print_log_hex;
	size = DEFAULT_SIZE;
	if (binflag)
		print_fn = print_bin;
	if (!binflag && !hexflag) {
		/*
		 * See if there is a pretty print function for the specified log
		 * page.  If one isn't found, we just revert to the default
		 * (print_hex). If there was a vendor specified bt the user, and
		 * the page is vendor specific, don't match the print function
		 * unless the vendors match.
		 */
		for (f = logfuncs; f->log_page > 0; f++) {
			if (f->vendor != NULL && vendor != NULL &&
			    strcmp(f->vendor, vendor) != 0)
				continue;
			if (log_page != f->log_page)
				continue;
			print_fn = f->print_fn;
			size = f->size;
			break;
		}
	}

	if (log_page == NVME_LOG_ERROR) {
		size = sizeof(struct nvme_error_information_entry);
		size *= (cdata.elpe + 1);
	}

	/* Read the log page */
	buf = get_log_buffer(size);
	read_logpage(fd, log_page, nsid, buf, size);
	print_fn(&cdata, buf, size);

	close(fd);
	exit(0);
}
