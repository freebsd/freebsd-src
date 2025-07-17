/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2013 EMC Corp.
 * All rights reserved.
 *
 * Copyright (C) 2012-2013 Intel Corporation
 * All rights reserved.
 * Copyright (C) 2016-2023 Warner Losh <imp@FreeBSD.org>
 * Copyright (C) 2018-2019 Alexander Motin <mav@FreeBSD.org>
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
#include <sysexits.h>
#include <unistd.h>
#include <sys/endian.h>

#include "nvmecontrol.h"

/* Tables for command line parsing */

static cmd_fn_t logpage;

#define NONE 0xffffffffu
static struct options {
	bool		binary;
	bool		hex;
	uint32_t	page;
	uint8_t		lsp;
	uint16_t	lsi;
	bool		rae;
	const char	*vendor;
	const char	*dev;
} opt = {
	.binary = false,
	.hex = false,
	.page = NONE,
	.lsp = 0,
	.lsi = 0,
	.rae = false,
	.vendor = NULL,
	.dev = NULL,
};

static const struct opts logpage_opts[] = {
#define OPT(l, s, t, opt, addr, desc) { l, s, t, &opt.addr, desc }
	OPT("binary", 'b', arg_none, opt, binary,
	    "Dump the log page as binary"),
	OPT("hex", 'x', arg_none, opt, hex,
	    "Dump the log page as hex"),
	OPT("page", 'p', arg_uint32, opt, page,
	    "Page to dump"),
	OPT("lsp", 'f', arg_uint8, opt, lsp,
	    "Log Specific Field"),
	OPT("lsi", 'i', arg_uint16, opt, lsi,
	    "Log Specific Identifier"),
	OPT("rae", 'r', arg_none, opt, rae,
	    "Retain Asynchronous Event"),
	OPT("vendor", 'v', arg_string, opt, vendor,
	    "Vendor specific formatting"),
	{ NULL, 0, arg_none, NULL, NULL }
};
#undef OPT

static const struct args logpage_args[] = {
	{ arg_string, &opt.dev, "<controller id|namespace id>" },
	{ arg_none, NULL, NULL },
};

static struct cmd logpage_cmd = {
	.name = "logpage",
	.fn = logpage,
	.descr = "Print logpages in human-readable form",
	.ctx_size = sizeof(opt),
	.opts = logpage_opts,
	.args = logpage_args,
};

CMD_COMMAND(logpage_cmd);

/* End of tables for command line parsing */

#define MAX_FW_SLOTS	(7)

static SLIST_HEAD(,logpage_function) logpages;

static int
logpage_compare(struct logpage_function *a, struct logpage_function *b)
{
	int c;

	if ((a->vendor == NULL) != (b->vendor == NULL))
		return (a->vendor == NULL ? -1 : 1);
	if (a->vendor != NULL) {
		c = strcmp(a->vendor, b->vendor);
		if (c != 0)
			return (c);
	}
	return ((int)a->log_page - (int)b->log_page);
}

void
logpage_register(struct logpage_function *p)
{
	struct logpage_function *l, *a;

	a = NULL;
	l = SLIST_FIRST(&logpages);
	while (l != NULL) {
		if (logpage_compare(l, p) > 0)
			break;
		a = l;
		l = SLIST_NEXT(l, link);
	}
	if (a == NULL)
		SLIST_INSERT_HEAD(&logpages, p, link);
	else
		SLIST_INSERT_AFTER(a, p, link);
}

const char *
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
		errx(EX_OSERR, "unable to malloc %u bytes", size);

	memset(buf, 0, size);
	return (buf);
}

void
read_logpage(int fd, uint8_t log_page, uint32_t nsid, uint8_t lsp,
    uint16_t lsi, uint8_t rae, uint64_t lpo, uint8_t csi, uint8_t ot,
    uint16_t uuid_index, void *payload, uint32_t payload_size)
{
	struct nvme_pt_command	pt;
	u_int numd;

	numd = payload_size / sizeof(uint32_t) - 1;
	memset(&pt, 0, sizeof(pt));
	pt.cmd.opc = NVME_OPC_GET_LOG_PAGE;
	pt.cmd.nsid = htole32(nsid);
	pt.cmd.cdw10 = htole32(
	    (numd << 16) |			/* NUMDL */
	    (rae << 15) |			/* RAE */
	    (lsp << 8) |			/* LSP */
	    log_page);				/* LID */
	pt.cmd.cdw11 = htole32(
	    ((uint32_t)lsi << 16) |		/* LSI */
	    (numd >> 16));			/* NUMDU */
	pt.cmd.cdw12 = htole32(lpo & 0xffffffff); /* LPOL */
	pt.cmd.cdw13 = htole32(lpo >> 32);	/* LPOU */
	pt.cmd.cdw14 = htole32(
	    (csi << 24) | 			/* CSI */
	    (ot << 23) |			/* OT */
	    uuid_index);			/* UUID Index */
	pt.buf = payload;
	pt.len = payload_size;
	pt.is_read = 1;

	if (ioctl(fd, NVME_PASSTHROUGH_CMD, &pt) < 0)
		err(EX_IOERR, "get log page request failed");

	if (nvme_completion_is_error(&pt.cpl))
		errx(EX_IOERR, "get log page request returned error");
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

	if (letoh(entry->error_count) == 0) {
		printf("No error entries found\n");
		return;
	}

	nentries = size / sizeof(struct nvme_error_information_entry);
	for (i = 0; i < nentries; i++, entry++) {
		if (letoh(entry->error_count) == 0)
			break;

		status = letoh(entry->status);

		p = NVME_STATUS_GET_P(status);
		sc = NVME_STATUS_GET_SC(status);
		sct = NVME_STATUS_GET_SCT(status);
		m = NVME_STATUS_GET_M(status);
		dnr = NVME_STATUS_GET_DNR(status);

		printf("Entry %02d\n", i + 1);
		printf("=========\n");
		printf(" Error count:          %ju\n", letoh(entry->error_count));
		printf(" Submission queue ID:  %u\n", letoh(entry->sqid));
		printf(" Command ID:           %u\n", letoh(entry->cid));
		/* TODO: Export nvme_status_string structures from kernel? */
		printf(" Status:\n");
		printf("  Phase tag:           %d\n", p);
		printf("  Status code:         %d\n", sc);
		printf("  Status code type:    %d\n", sct);
		printf("  More:                %d\n", m);
		printf("  DNR:                 %d\n", dnr);
		printf(" Error location:       %u\n", letoh(entry->error_location));
		printf(" LBA:                  %ju\n", letoh(entry->lba));
		printf(" Namespace ID:         %u\n", letoh(entry->nsid));
		printf(" Vendor specific info: %u\n", letoh(entry->vendor_specific));
		printf(" Transport type:       %u\n", letoh(entry->trtype));
		printf(" Command specific info:%ju\n", letoh(entry->csi));
		printf(" Transport specific:   %u\n", letoh(entry->ttsi));
	}
}

void
print_temp_K(uint16_t t)
{
	printf("%u K, %2.2f C, %3.2f F\n", t, (float)t - 273.15, (float)t * 9 / 5 - 459.67);
}

void
print_temp_C(uint16_t t)
{
	printf("%2.2f K, %u C, %3.2f F\n", (float)t + 273.15, t, (float)t * 9 / 5 + 32);
}

static void
print_log_health(const struct nvme_controller_data *cdata __unused, void *buf, uint32_t size __unused)
{
	struct nvme_health_information_page *health = buf;
	char cbuf[UINT128_DIG + 1];
	uint8_t	warning;
	int i;

	warning = letoh(health->critical_warning);

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
	print_temp_K(letoh(health->temperature));
	printf("Available spare:                %u\n",
	    letoh(health->available_spare));
	printf("Available spare threshold:      %u\n",
	    letoh(health->available_spare_threshold));
	printf("Percentage used:                %u\n",
	    letoh(health->percentage_used));

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

	printf("Warning Temp Composite Time:    %d\n", letoh(health->warning_temp_time));
	printf("Error Temp Composite Time:      %d\n", letoh(health->error_temp_time));
	for (i = 0; i < 8; i++) {
		if (letoh(health->temp_sensor[i]) == 0)
			continue;
		printf("Temperature Sensor %d:           ", i + 1);
		print_temp_K(letoh(health->temp_sensor[i]));
	}
	printf("Temperature 1 Transition Count: %d\n", letoh(health->tmt1tc));
	printf("Temperature 2 Transition Count: %d\n", letoh(health->tmt2tc));
	printf("Total Time For Temperature 1:   %d\n", letoh(health->ttftmt1));
	printf("Total Time For Temperature 2:   %d\n", letoh(health->ttftmt2));
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

	afi_slot = NVMEV(NVME_FIRMWARE_PAGE_AFI_SLOT, fw->afi);

	oacs_fw = NVMEV(NVME_CTRLR_DATA_OACS_FIRMWARE, cdata->oacs);
	fw_num_slots = NVMEV(NVME_CTRLR_DATA_FRMW_NUM_SLOTS, cdata->frmw);

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

		if (fw->revision[i][0] == '\0')
			printf("Empty\n");
		else
			printf("[%s] %.8s\n", status, fw->revision[i]);
	}
}

static void
print_log_ns(const struct nvme_controller_data *cdata __unused, void *buf,
    uint32_t size __unused)
{
	struct nvme_ns_list *nsl;
	u_int i;

	nsl = (struct nvme_ns_list *)buf;
	printf("Changed Namespace List\n");
	printf("======================\n");

	for (i = 0; i < nitems(nsl->ns) && letoh(nsl->ns[i]) != 0; i++) {
		printf("%08x\n", letoh(nsl->ns[i]));
	}
}

static void
print_log_command_effects(const struct nvme_controller_data *cdata __unused,
    void *buf, uint32_t size __unused)
{
	struct nvme_command_effects_page *ce;
	u_int i;
	uint32_t s;

	ce = (struct nvme_command_effects_page *)buf;
	printf("Commands Supported and Effects\n");
	printf("==============================\n");
	printf("  Command\tLBCC\tNCC\tNIC\tCCC\tCSE\tUUID\n");

	for (i = 0; i < 255; i++) {
		s = letoh(ce->acs[i]);
		if (NVMEV(NVME_CE_PAGE_CSUP, s) == 0)
			continue;
		printf("Admin\t%02x\t%s\t%s\t%s\t%s\t%u\t%s\n", i,
		    NVMEV(NVME_CE_PAGE_LBCC, s) != 0 ? "Yes" : "No",
		    NVMEV(NVME_CE_PAGE_NCC, s) != 0 ? "Yes" : "No",
		    NVMEV(NVME_CE_PAGE_NIC, s) != 0 ? "Yes" : "No",
		    NVMEV(NVME_CE_PAGE_CCC, s) != 0 ? "Yes" : "No",
		    NVMEV(NVME_CE_PAGE_CSE, s),
		    NVMEV(NVME_CE_PAGE_UUID, s) != 0 ? "Yes" : "No");
	}
	for (i = 0; i < 255; i++) {
		s = letoh(ce->iocs[i]);
		if (NVMEV(NVME_CE_PAGE_CSUP, s) == 0)
			continue;
		printf("I/O\t%02x\t%s\t%s\t%s\t%s\t%u\t%s\n", i,
		    NVMEV(NVME_CE_PAGE_LBCC, s) != 0 ? "Yes" : "No",
		    NVMEV(NVME_CE_PAGE_NCC, s) != 0 ? "Yes" : "No",
		    NVMEV(NVME_CE_PAGE_NIC, s) != 0 ? "Yes" : "No",
		    NVMEV(NVME_CE_PAGE_CCC, s) != 0 ? "Yes" : "No",
		    NVMEV(NVME_CE_PAGE_CSE, s),
		    NVMEV(NVME_CE_PAGE_UUID, s) != 0 ? "Yes" : "No");
	}
}

static void
print_log_res_notification(const struct nvme_controller_data *cdata __unused,
    void *buf, uint32_t size __unused)
{
	struct nvme_res_notification_page *rn;

	rn = (struct nvme_res_notification_page *)buf;
	printf("Reservation Notification\n");
	printf("========================\n");

	printf("Log Page Count:                %ju\n",
	    (uintmax_t)letoh(rn->log_page_count));
	printf("Log Page Type:                 ");
	switch (letoh(rn->log_page_type)) {
	case 0:
		printf("Empty Log Page\n");
		break;
	case 1:
		printf("Registration Preempted\n");
		break;
	case 2:
		printf("Reservation Released\n");
		break;
	case 3:
		printf("Reservation Preempted\n");
		break;
	default:
		printf("Unknown %x\n", letoh(rn->log_page_type));
		break;
	};
	printf("Number of Available Log Pages: %d\n", letoh(rn->available_log_pages));
	printf("Namespace ID:                  0x%x\n", letoh(rn->nsid));
}

static void
print_log_sanitize_status(const struct nvme_controller_data *cdata __unused,
    void *buf, uint32_t size __unused)
{
	struct nvme_sanitize_status_page *ss;
	u_int p;
	uint16_t sprog, sstat;

	ss = (struct nvme_sanitize_status_page *)buf;
	printf("Sanitize Status\n");
	printf("===============\n");

	sprog = letoh(ss->sprog);
	printf("Sanitize Progress:                   %u%% (%u/65535)\n",
	    (sprog * 100 + 32768) / 65536, sprog);
	printf("Sanitize Status:                     ");
	sstat = letoh(ss->sstat);
	switch (NVMEV(NVME_SS_PAGE_SSTAT_STATUS, sstat)) {
	case NVME_SS_PAGE_SSTAT_STATUS_NEVER:
		printf("Never sanitized");
		break;
	case NVME_SS_PAGE_SSTAT_STATUS_COMPLETED:
		printf("Completed");
		break;
	case NVME_SS_PAGE_SSTAT_STATUS_INPROG:
		printf("In Progress");
		break;
	case NVME_SS_PAGE_SSTAT_STATUS_FAILED:
		printf("Failed");
		break;
	case NVME_SS_PAGE_SSTAT_STATUS_COMPLETEDWD:
		printf("Completed with deallocation");
		break;
	default:
		printf("Unknown 0x%x", sstat);
		break;
	}
	p = NVMEV(NVME_SS_PAGE_SSTAT_PASSES, sstat);
	if (p > 0)
		printf(", %d passes", p);
	if (NVMEV(NVME_SS_PAGE_SSTAT_GDE, sstat) != 0)
		printf(", Global Data Erased");
	printf("\n");
	printf("Sanitize Command Dword 10:           0x%x\n", letoh(ss->scdw10));
	printf("Time For Overwrite:                  %u sec\n", letoh(ss->etfo));
	printf("Time For Block Erase:                %u sec\n", letoh(ss->etfbe));
	printf("Time For Crypto Erase:               %u sec\n", letoh(ss->etfce));
	printf("Time For Overwrite No-Deallocate:    %u sec\n", letoh(ss->etfownd));
	printf("Time For Block Erase No-Deallocate:  %u sec\n", letoh(ss->etfbewnd));
	printf("Time For Crypto Erase No-Deallocate: %u sec\n", letoh(ss->etfcewnd));
}

static const char *
self_test_res[] = {
	[0] = "completed without error",
	[1] = "aborted by a Device Self-test command",
	[2] = "aborted by a Controller Level Reset",
	[3] = "aborted due to namespace removal",
	[4] = "aborted due to Format NVM command",
	[5] = "failed due to fatal or unknown test error",
	[6] = "completed with an unknown segment that failed",
	[7] = "completed with one or more failed segments",
	[8] = "aborted for unknown reason",
	[9] = "aborted due to a sanitize operation",
};
static uint32_t self_test_res_max = nitems(self_test_res);

static void
print_log_self_test_status(const struct nvme_controller_data *cdata __unused,
    void *buf, uint32_t size __unused)
{
	struct nvme_device_self_test_page *dst;
	uint32_t r;
	uint16_t vs;

	dst = buf;
	printf("Device Self-test Status\n");
	printf("=======================\n");

	printf("Current Operation: ");
	switch (letoh(dst->curr_operation)) {
	case 0x0:
		printf("No device self-test operation in progress\n");
		break;
	case 0x1:
		printf("Short device self-test operation in progress\n");
		break;
	case 0x2:
		printf("Extended device self-test operation in progress\n");
		break;
	case 0xe:
		printf("Vendor specific\n");
		break;
	default:
		printf("Reserved (0x%x)\n", letoh(dst->curr_operation));
	}

	if (letoh(dst->curr_operation) != 0)
		printf("Current Completion: %u%%\n", letoh(dst->curr_compl) & 0x7f);

	printf("Results\n");
	for (r = 0; r < 20; r++) {
		uint64_t failing_lba;
		uint8_t code, res, status;

		status = letoh(dst->result[r].status);
		code = (status >> 4) & 0xf;
		res  = status & 0xf;

		if (res == 0xf)
			continue;

		printf("[%2u] ", r);
		switch (code) {
		case 0x1:
			printf("Short device self-test");
			break;
		case 0x2:
			printf("Extended device self-test");
			break;
		case 0xe:
			printf("Vendor specific");
			break;
		default:
			printf("Reserved (0x%x)", code);
		}
		if (res < self_test_res_max)
			printf(" %s", self_test_res[res]);
		else
			printf(" Reserved status 0x%x", res);

		if (res == 7)
			printf(" starting in segment %u",
			    letoh(dst->result[r].segment_num));

#define BIT(b) (1 << (b))
		if (letoh(dst->result[r].valid_diag_info) & BIT(0))
			printf(" NSID=0x%x", letoh(dst->result[r].nsid));
		if (letoh(dst->result[r].valid_diag_info) & BIT(1)) {
			memcpy(&failing_lba, dst->result[r].failing_lba,
			    sizeof(failing_lba));
			printf(" FLBA=0x%jx", (uintmax_t)letoh(failing_lba));
		}
		if (letoh(dst->result[r].valid_diag_info) & BIT(2))
			printf(" SCT=0x%x", letoh(dst->result[r].status_code_type));
		if (letoh(dst->result[r].valid_diag_info) & BIT(3))
			printf(" SC=0x%x", letoh(dst->result[r].status_code));
#undef BIT
		memcpy(&vs, dst->result[r].vendor_specific, sizeof(vs));
		printf(" VENDOR_SPECIFIC=0x%x", letoh(vs));
		printf("\n");
	}
}

/*
 * Table of log page printer / sizing.
 *
 * Make sure you keep all the pages of one vendor together so -v help
 * lists all the vendors pages.
 */
NVME_LOGPAGE(error,
    NVME_LOG_ERROR,			NULL,	"Drive Error Log",
    print_log_error, 			0);
NVME_LOGPAGE(health,
    NVME_LOG_HEALTH_INFORMATION,	NULL,	"Health/SMART Data",
    print_log_health, 			sizeof(struct nvme_health_information_page));
NVME_LOGPAGE(fw,
    NVME_LOG_FIRMWARE_SLOT,		NULL,	"Firmware Information",
    print_log_firmware,			sizeof(struct nvme_firmware_page));
NVME_LOGPAGE(ns,
    NVME_LOG_CHANGED_NAMESPACE,		NULL,	"Changed Namespace List",
    print_log_ns,			sizeof(struct nvme_ns_list));
NVME_LOGPAGE(ce,
    NVME_LOG_COMMAND_EFFECT,		NULL,	"Commands Supported and Effects",
    print_log_command_effects,		sizeof(struct nvme_command_effects_page));
NVME_LOGPAGE(dst,
    NVME_LOG_DEVICE_SELF_TEST,		NULL,	"Device Self-test",
    print_log_self_test_status,		sizeof(struct nvme_device_self_test_page));
NVME_LOGPAGE(thi,
    NVME_LOG_TELEMETRY_HOST_INITIATED,	NULL,	"Telemetry Host-Initiated",
    NULL,				DEFAULT_SIZE);
NVME_LOGPAGE(tci,
    NVME_LOG_TELEMETRY_CONTROLLER_INITIATED,	NULL,	"Telemetry Controller-Initiated",
    NULL,				DEFAULT_SIZE);
NVME_LOGPAGE(egi,
    NVME_LOG_ENDURANCE_GROUP_INFORMATION,	NULL,	"Endurance Group Information",
    NULL,				DEFAULT_SIZE);
NVME_LOGPAGE(plpns,
    NVME_LOG_PREDICTABLE_LATENCY_PER_NVM_SET,	NULL,	"Predictable Latency Per NVM Set",
    NULL,				DEFAULT_SIZE);
NVME_LOGPAGE(ple,
    NVME_LOG_PREDICTABLE_LATENCY_EVENT_AGGREGATE,	NULL,	"Predictable Latency Event Aggregate",
    NULL,				DEFAULT_SIZE);
NVME_LOGPAGE(ana,
    NVME_LOG_ASYMMETRIC_NAMESPACE_ACCESS,	NULL,	"Asymmetric Namespace Access",
    NULL,				DEFAULT_SIZE);
NVME_LOGPAGE(pel,
    NVME_LOG_PERSISTENT_EVENT_LOG,	NULL,	"Persistent Event Log",
    NULL,				DEFAULT_SIZE);
NVME_LOGPAGE(lbasi,
    NVME_LOG_LBA_STATUS_INFORMATION,	NULL,	"LBA Status Information",
    NULL,				DEFAULT_SIZE);
NVME_LOGPAGE(egea,
    NVME_LOG_ENDURANCE_GROUP_EVENT_AGGREGATE,	NULL,	"Endurance Group Event Aggregate",
    NULL,				DEFAULT_SIZE);
NVME_LOGPAGE(res_notification,
    NVME_LOG_RES_NOTIFICATION,		NULL,	"Reservation Notification",
    print_log_res_notification,		sizeof(struct nvme_res_notification_page));
NVME_LOGPAGE(sanitize_status,
    NVME_LOG_SANITIZE_STATUS,		NULL,	"Sanitize Status",
    print_log_sanitize_status,		sizeof(struct nvme_sanitize_status_page));

static void
logpage_help(void)
{
	const struct logpage_function	*f;
	const char 			*v;

	fprintf(stderr, "\n");
	fprintf(stderr, "%-8s %-10s %s\n", "Page", "Vendor","Page Name");
	fprintf(stderr, "-------- ---------- ----------\n");
	SLIST_FOREACH(f, &logpages, link) {
		v = f->vendor == NULL ? "-" : f->vendor;
		fprintf(stderr, "0x%02x     %-10s %s\n", f->log_page, v, f->name);
	}

	exit(EX_USAGE);
}

static void
logpage(const struct cmd *f, int argc, char *argv[])
{
	int				fd;
	char				*path;
	uint32_t			nsid, size;
	void				*buf;
	const struct logpage_function	*lpf;
	struct nvme_controller_data	cdata;
	print_fn_t			print_fn;
	uint8_t				ns_smart;

	if (arg_parse(argc, argv, f))
		return;
	if (opt.hex && opt.binary) {
		fprintf(stderr,
		    "Can't specify both binary and hex\n");
		arg_help(argc, argv, f);
	}
	if (opt.vendor != NULL && strcmp(opt.vendor, "help") == 0)
		logpage_help();
	if (opt.page == NONE) {
		fprintf(stderr, "Missing page_id (-p).\n");
		arg_help(argc, argv, f);
	}
	open_dev(opt.dev, &fd, 0, 1);
	get_nsid(fd, &path, &nsid);
	if (nsid == 0) {
		nsid = NVME_GLOBAL_NAMESPACE_TAG;
	} else {
		close(fd);
		open_dev(path, &fd, 0, 1);
	}
	free(path);

	if (read_controller_data(fd, &cdata))
		errx(EX_IOERR, "Identify request failed");

	ns_smart = NVMEV(NVME_CTRLR_DATA_LPA_NS_SMART, cdata.lpa);

	/*
	 * The log page attributes indicate whether or not the controller
	 * supports the SMART/Health information log page on a per
	 * namespace basis.
	 */
	if (nsid != NVME_GLOBAL_NAMESPACE_TAG) {
		if (opt.page != NVME_LOG_HEALTH_INFORMATION)
			errx(EX_USAGE, "log page %d valid only at controller level",
			    opt.page);
		if (ns_smart == 0)
			errx(EX_UNAVAILABLE,
			    "controller does not support per namespace "
			    "smart/health information");
	}

	print_fn = print_log_hex;
	size = DEFAULT_SIZE;
	if (opt.binary)
		print_fn = print_bin;
	if (!opt.binary && !opt.hex) {
		/*
		 * See if there is a pretty print function for the specified log
		 * page.  If one isn't found, we just revert to the default
		 * (print_hex). If there was a vendor specified by the user, and
		 * the page is vendor specific, don't match the print function
		 * unless the vendors match.
		 */
		SLIST_FOREACH(lpf, &logpages, link) {
			if (lpf->vendor != NULL && opt.vendor != NULL &&
			    strcmp(lpf->vendor, opt.vendor) != 0)
				continue;
			if (opt.page != lpf->log_page)
				continue;
			if (lpf->print_fn != NULL)
				print_fn = lpf->print_fn;
			size = lpf->size;
			break;
		}
	}

	if (opt.page == NVME_LOG_ERROR) {
		size = sizeof(struct nvme_error_information_entry);
		size *= (cdata.elpe + 1);
	}

	/* Read the log page */
	buf = get_log_buffer(size);
	read_logpage(fd, opt.page, nsid, opt.lsp, opt.lsi, opt.rae,
	    0, 0, 0, 0, buf, size);
	print_fn(&cdata, buf, size);

	close(fd);
	exit(0);
}
