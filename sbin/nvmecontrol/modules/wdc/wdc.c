/*-
 * Copyright (c) 2017 Netflix, Inc.
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
#include <sys/endian.h>

#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include <stdbool.h>

#include "nvmecontrol.h"

/* Tables for command line parsing */

static cmd_fn_t wdc;
static cmd_fn_t wdc_cap_diag;

#define NONE 0xffffffffu
#define NONE64 0xffffffffffffffffull
#define OPT(l, s, t, opt, addr, desc) { l, s, t, &opt.addr, desc }
#define OPT_END	{ NULL, 0, arg_none, NULL, NULL }

static struct cmd wdc_cmd = {
	.name = "wdc", .fn = wdc, .descr = "wdc vendor specific commands", .ctx_size = 0, .opts = NULL, .args = NULL,
};

CMD_COMMAND(wdc_cmd);

static struct options 
{
	const char *template;
	const char *dev;
	uint8_t data_area;
} opt = {
	.template = NULL,
	.dev = NULL,
	.data_area = 0,
};

static const struct opts opts[] = {
	OPT("template", 'o', arg_string, opt, template,
	    "Template for paths to use for different logs"),
	OPT("data-area", 'd', arg_uint8, opt, data_area,
	    "Data-area to retrieve up to"),
	OPT_END
};

static const struct args args[] = {
	{ arg_string, &opt.dev, "controller-id" },
	{ arg_none, NULL, NULL },
};

static struct cmd cap_diag_cmd = {
	.name = "cap-diag",
	.fn = wdc_cap_diag,
	.descr = "Retrieve the cap-diag logs from the drive",
	.ctx_size = sizeof(struct options),
	.opts = opts,
	.args = args,
};

CMD_SUBCOMMAND(wdc_cmd, cap_diag_cmd);

#define WDC_NVME_VID				0x1c58
#define WDC_NVME_VID_2				0x1b96
#define WDC_NVME_VID_3				0x15b7

#define WDC_NVME_TOC_SIZE			0x8
#define WDC_NVME_LOG_SIZE_HDR_LEN		0x8
#define WDC_NVME_CAP_DIAG_OPCODE_E6		0xe6
#define WDC_NVME_CAP_DIAG_CMD			0x0000
#define WDC_NVME_CAP_DIAG_OPCODE_FA		0xfa
#define WDC_NVME_DUI_MAX_SECTIONS_V0		0x3c
#define WDC_NVME_DUI_MAX_SECTIONS_V1		0x3a
#define WDC_NVME_DUI_MAX_SECTIONS_V2		0x26
#define WDC_NVME_DUI_MAX_SECTIONS_V3		0x23

typedef enum wdc_dui_header {
	WDC_DUI_HEADER_VER_0 = 0,
	WDC_DUI_HEADER_VER_1,
	WDC_DUI_HEADER_VER_2,
	WDC_DUI_HEADER_VER_3,
} wdc_dui_header;

static void
wdc_append_serial_name(int fd, char *buf, size_t len, const char *suffix)
{
	struct nvme_controller_data	cdata;
	char sn[NVME_SERIAL_NUMBER_LENGTH + 1];
	char *walker;

	len -= strlen(buf);
	buf += strlen(buf);
	if (read_controller_data(fd, &cdata))
		errx(EX_IOERR, "Identify request failed");
	memcpy(sn, cdata.sn, NVME_SERIAL_NUMBER_LENGTH);
	walker = sn + NVME_SERIAL_NUMBER_LENGTH - 1;
	while (walker > sn && *walker == ' ')
		walker--;
	*++walker = '\0';
	snprintf(buf, len, "_%s_%s.bin", sn, suffix);
}

static void
wdc_get_data(int fd, uint32_t opcode, uint32_t len, uint32_t off, uint32_t cmd,
    uint8_t *buffer, size_t buflen, bool e6lg_flag)
{
	struct nvme_pt_command	pt;

	memset(&pt, 0, sizeof(pt));
	pt.cmd.opc = opcode;
	pt.cmd.cdw10 = htole32(len / sizeof(uint32_t));
	pt.cmd.cdw12 = htole32(cmd);
	if (e6lg_flag)
		pt.cmd.cdw11 = htole32(off / sizeof(uint32_t));
	else
		pt.cmd.cdw13 = htole32(off / sizeof(uint32_t));
	pt.buf = buffer;
	pt.len = buflen;
	pt.is_read = 1;

	if (ioctl(fd, NVME_PASSTHROUGH_CMD, &pt) < 0)
		err(EX_IOERR, "wdc_get_data request failed");
	if (nvme_completion_is_error(&pt.cpl))
		errx(EX_IOERR, "wdc_get_data request returned error");
}

static void
wdc_do_dump_e6(int fd, char *tmpl, const char *suffix, uint32_t opcode,
    uint32_t cmd, int len_off)
{
	int first;
	int fd2;
	uint8_t *buf, *hdr;
	uint64_t max_xfer_size;
	uint32_t len, offset;
	size_t resid;
	bool e6lg_flag = false;

	wdc_append_serial_name(fd, tmpl, MAXPATHLEN, suffix);

	/* Read Log Dump header */
	len = WDC_NVME_LOG_SIZE_HDR_LEN;
	offset = 0;
	hdr = malloc(len);
	if (hdr == NULL)
		errx(EX_OSERR, "Can't get buffer to read dump");
	wdc_get_data(fd, opcode, len, offset, cmd, hdr, len, false);
	if (memcmp("E6LG", hdr, 4) == 0) {
		e6lg_flag = true;
	}

	/* XXX overwrite protection? */
	fd2 = open(tmpl, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd2 < 0)
		err(EX_CANTCREAT, "open %s", tmpl);
	if (ioctl(fd, NVME_GET_MAX_XFER_SIZE, &max_xfer_size) < 0)
		err(EX_IOERR, "query max transfer size failed");
	buf = aligned_alloc(PAGE_SIZE, max_xfer_size);
	if (buf == NULL)
		errx(EX_OSERR, "Can't get buffer to read dump");
	offset = 0;
	len = max_xfer_size;
	first = 1;

	do {
		resid = MIN(len, max_xfer_size);
		wdc_get_data(fd, opcode, resid, offset, cmd, buf, resid, e6lg_flag);

		if (first) {
			len = be32dec(buf + len_off);
			if (len == 0)
				errx(EX_PROTOCOL, "No data for %s", suffix);

			printf("Dumping %d bytes of version %d.%d log to %s\n", len,
			    buf[8], buf[9], tmpl);
			/*
			 * Adjust amount to dump if total dump < 1MB,
			 * though it likely doesn't matter to the WDC
			 * analysis tools.
			 */
			if (resid > len)
				resid = len;
			first = 0;
		}
		if (write(fd2, buf, resid) != (ssize_t)resid)
			err(EX_IOERR, "write");
		offset += resid;
		len -= resid;
	} while (len > 0);
	free(hdr);
	free(buf);
	close(fd2);
}

static void
wdc_get_data_dui(int fd, uint32_t opcode, uint32_t len, uint64_t off,
    uint8_t *buffer, size_t buflen)
{
	struct nvme_pt_command	pt;

	memset(&pt, 0, sizeof(pt));
	pt.cmd.opc = opcode;
	pt.cmd.nsid = NONE;
	pt.cmd.cdw10 = htole32((len / sizeof(uint32_t)) - 1) ;
	pt.cmd.cdw12 = htole32(off & 0xFFFFFFFFu);
	pt.cmd.cdw13 = htole32(off >> 32);
	pt.buf = buffer;
	pt.len = buflen;
	pt.is_read = 1;

	if (ioctl(fd, NVME_PASSTHROUGH_CMD, &pt) < 0)
		err(EX_IOERR, "wdc_get_data_dui request failed");
	if (nvme_completion_is_error(&pt.cpl))
		errx(EX_IOERR, "wdc_get_data_dui request returned error");
}

static uint8_t
wdc_get_dui_max_sections(uint16_t header_ver)
{
	switch (header_ver) {
	case WDC_DUI_HEADER_VER_0:
		return WDC_NVME_DUI_MAX_SECTIONS_V0;
	case WDC_DUI_HEADER_VER_1:
		return WDC_NVME_DUI_MAX_SECTIONS_V1;
	case WDC_DUI_HEADER_VER_2:
		return WDC_NVME_DUI_MAX_SECTIONS_V2;
	case WDC_DUI_HEADER_VER_3:
		return WDC_NVME_DUI_MAX_SECTIONS_V3;
	}
	return 0;
}

static void
wdc_get_dui_log_size(int fd, uint32_t opcode, uint8_t data_area,
	uint64_t *log_size, int len_off)
{
	uint8_t *hdr, *tofree;
	uint8_t max_sections;
	int i, j;
	uint16_t hdr_ver;
	uint16_t len;
	uint64_t dui_size;

	dui_size = 0;
	len = 1024;
	tofree = hdr = (uint8_t*)malloc(len);
	if (hdr == NULL)
		errx(EX_OSERR, "Can't get buffer to read header");
	wdc_get_data_dui(fd, opcode, len, 0, hdr, len);

	hdr += len_off;
	hdr_ver = ((*hdr & 0xF) != 0)? *hdr : le16dec(hdr);
	max_sections = wdc_get_dui_max_sections(hdr_ver);

	if (hdr_ver == 0 || hdr_ver == 1) {
		dui_size = (uint64_t)le32dec(hdr + 4);
		if (dui_size == 0) {
			hdr += 8;
			for (i = 0, j = 0; i < (int)max_sections; i++, j+=8)
				dui_size += (uint64_t)le32dec(hdr + j + 4);
		}
	} else if (hdr_ver == 2 || hdr_ver == 3) {
		if (data_area == 0) {
			dui_size = le64dec(hdr + 4);
			if (dui_size == 0) {
				hdr += 12;
				for (i = 0, j = 0 ; i < (int)max_sections; i++, j+=12)
					dui_size += le64dec(hdr + j + 4);
			}
		} else {
			hdr += 12;
			for (i = 0, j = 0; i < (int)max_sections; i++, j+=12) {
				if (le16dec(hdr + j + 2) <= data_area)
					dui_size += le64dec(hdr + j + 4);
				else
					break;
			}
		}
	}
	else
		errx(EX_PROTOCOL, "ERROR : No valid header ");

	*log_size = dui_size;
	free(tofree);
}

static void
wdc_do_dump_dui(int fd, char *tmpl, uint8_t data_area,
	const char *suffix, uint32_t opcode, int len_off)
{
	int fd2, first;
	uint8_t *buf;
	uint64_t max_xfer_size;
	uint16_t hdr_ver;
	uint64_t log_len, offset;
	size_t resid;

	wdc_append_serial_name(fd, tmpl, MAXPATHLEN, suffix);
	wdc_get_dui_log_size(fd, opcode, data_area, &log_len, len_off);
	if (log_len == 0)
		errx(EX_PROTOCOL, "No data for %s", suffix);
	fd2 = open(tmpl, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd2 < 0)
		err(EX_CANTCREAT, "open %s", tmpl);
	if (ioctl(fd, NVME_GET_MAX_XFER_SIZE, &max_xfer_size) < 0)
		err(EX_IOERR, "query max transfer size failed");
	buf = aligned_alloc(PAGE_SIZE, max_xfer_size);
	if (buf == NULL)
		errx(EX_OSERR, "Can't get buffer to read dump");
	offset = 0;
	first = 1;

	while (log_len > 0) {
		resid = MIN(log_len, max_xfer_size);
		wdc_get_data_dui(fd, opcode, resid, offset, buf, resid);
		if (first) {
			hdr_ver = ((buf[len_off] & 0xF) != 0) ?
			    (buf[len_off]) : (le16dec(buf + len_off));
			printf("Dumping %jd bytes of version %d log to %s\n",
			    (uintmax_t)log_len, hdr_ver, tmpl);
			first = 0;
		}
		if (write(fd2, buf, resid) != (ssize_t)resid)
			err(EX_IOERR, "write");
		offset += resid;
		log_len -= resid;
	}

	free(buf);
	close(fd2);
}

static void
wdc_cap_diag(const struct cmd *f, int argc, char *argv[])
{
	char tmpl[MAXPATHLEN];
 	int fd;
	struct nvme_controller_data	cdata;
	uint32_t vid;

	if (arg_parse(argc, argv, f))
		return;
	if (opt.template == NULL) {
		fprintf(stderr, "Missing template arg.\n");
		arg_help(argc, argv, f);
	}
	if (opt.data_area > 4) {
		fprintf(stderr, "Data area range 1-4, supplied %d.\n", opt.data_area);
		arg_help(argc, argv, f);
	}
	strlcpy(tmpl, opt.template, sizeof(tmpl));
	open_dev(opt.dev, &fd, 1, 1);
	if (read_controller_data(fd, &cdata))
		errx(EX_IOERR, "Identify request failed");
	vid = cdata.vid;

	switch (vid) {
	case WDC_NVME_VID :
	case WDC_NVME_VID_2 :
		wdc_do_dump_e6(fd, tmpl, "cap_diag", WDC_NVME_CAP_DIAG_OPCODE_E6,
		    WDC_NVME_CAP_DIAG_CMD, 4);
		break;
	case WDC_NVME_VID_3 :
		wdc_do_dump_dui(fd, tmpl, opt.data_area, "cap_diag",
		    WDC_NVME_CAP_DIAG_OPCODE_FA, 512);
		break;
	default:
		errx(EX_UNAVAILABLE, "ERROR : WDC: unsupported device (%#x) for this command", vid);
	}
	close(fd);
	exit(0);
}

static void
wdc(const struct cmd *nf __unused, int argc, char *argv[])
{

	cmd_dispatch(argc, argv, &wdc_cmd);
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
		for (i = 0; i < plen && wsp < esp; i++)
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
	uint16_t len;
	uint8_t subtype, res;

	printf("HGST Extra Info Log\n");
	printf("===================\n");

	walker = buf;
	walker += 2;			/* Page count */
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

NVME_LOGPAGE(hgst_info,
    HGST_INFO_LOG,			"hgst",	"Detailed Health/SMART",
    print_hgst_info_log,		DEFAULT_SIZE);
NVME_LOGPAGE(wdc_info,
    HGST_INFO_LOG,			"wdc",	"Detailed Health/SMART",
    print_hgst_info_log,		DEFAULT_SIZE);
