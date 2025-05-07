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
#include <libxo/xo.h>
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

	xo_emit("{T:Error Information Log}\n");
	xo_emit("{T:=====================}\n");

	if (letoh(entry->error_count) == 0) {
		xo_emit("{:error/No error entries found}\n");
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

		xo_emit("{T:Entry }{T:entry/%02d}\n", i + 1);
		xo_emit("{T:=========}\n");
		xo_emit("{P: }{Lc:Error count}{P:          }{:error-count/%ju}\n", letoh(entry->error_count));
		xo_emit("{P: }{Lc:Submission queue ID}{P:  }{:submission-queue-id/%u}\n", letoh(entry->sqid));
		xo_emit("{P: }{Lc:Command ID}{P:           }{:command-id/%u}\n", letoh(entry->cid));
		/* TODO: Export nvme_status_string structures from kernel? */
		xo_emit("{P: }{Lc:Status}{P:}\n");
		xo_emit("{P:  }{Lc:Phase tag}{P:           }{:phase-tag/%d}\n", p);
		xo_emit("{P:  }{Lc:Status code}{P:         }{:status-code/%d}\n", sc);
		xo_emit("{P:  }{Lc:Status code type}{P:    }{:status-code-type/%d}\n", sct);
		xo_emit("{P:  }{Lc:More}{P:                }{:more/%d}\n", m);
		xo_emit("{P:  }{Lc:DNR}{P:                 }{:do-not-retry/%d}\n", dnr);
		xo_emit("{P: }{Lc:Error location}{P:       }{:error-location/%u}\n", letoh(entry->error_location));
		xo_emit("{P: }{Lc:LBA}{P:                  }{:logical-block/%ju}\n", letoh(entry->lba));
		xo_emit("{P: }{Lc:Namespace ID}{P:         }{:namespace-id/%u}\n", letoh(entry->nsid));
		xo_emit("{P: }{Lc:Vendor specific info}{P: }{:vendor-specific/%u}\n", letoh(entry->vendor_specific));
		xo_emit("{P: }{Lc:Transport type}{P:       }{:transport-type/%u}\n", letoh(entry->trtype));
		xo_emit("{P: }{Lc:Command specific info}{P:}{:command-specific-info/%ju}\n", letoh(entry->csi));
		xo_emit("{P: }{Lc:Transport specific}{P:   }{:transport-specific/%u}\n", letoh(entry->ttsi));
	}
}

void
print_temp_K(uint16_t t)
{
	xo_emit("{:kelvin/%u} {U:K}, {:celius/%2.2f} {U:C}, {:fahrenheit/%3.2f} {U:F}\n", t, (float)t - 273.15, (float)t * 9 / 5 - 459.67);
}

void
print_temp_C(uint16_t t)
{
	xo_emit("{:kelvin/%2.2f} {U:K}, {:celius/%u} {U:C}, {:fahrenheit/%3.2f} {U:F}\n", (float)t + 273.15, t, (float)t * 9 / 5 + 32);
}

static void
print_log_health(const struct nvme_controller_data *cdata __unused, void *buf, uint32_t size __unused)
{
	struct nvme_health_information_page *health = buf;
	char cbuf[UINT128_DIG + 1];
	uint8_t	warning;
	int i;

	warning = letoh(health->critical_warning);

	xo_emit("{T:SMART}/{T:Health Information Log}\n");
	xo_emit("{T:============================}\n");

	xo_emit("{Lc:Critical Warning State}{P:         }0x{:critical-warning/%02x}\n", warning);
	xo_emit("{P: }{Lc:Available spare}{P:               }{:warn-spare/%d}\n",
	    !!(warning & NVME_CRIT_WARN_ST_AVAILABLE_SPARE));
	xo_emit("{P: }{Lc:Temperature}{P:                   }{:warn-temp/%d}\n",
	    !!(warning & NVME_CRIT_WARN_ST_TEMPERATURE));
	xo_emit("{P: }{Lc:Device reliability}{P:            }{:warn-reliability/%d}\n",
	    !!(warning & NVME_CRIT_WARN_ST_DEVICE_RELIABILITY));
	xo_emit("{P: }{Lc:Read only}{P:                     }{:warn-read-only/%d}\n",
	    !!(warning & NVME_CRIT_WARN_ST_READ_ONLY));
	xo_emit("{P: }{Lc:Volatile memory backup}{P:        }{:warn-backup/%d}\n",
	    !!(warning & NVME_CRIT_WARN_ST_VOLATILE_MEMORY_BACKUP));
	xo_emit("{Lc:Temperature}{P:                    }");
	print_temp_K(letoh(health->temperature));
	xo_emit("{Lc:Available spare}{P:                }{:available-spare/%u}\n",
	    letoh(health->available_spare));
	xo_emit("{Lc:Available spare threshold}{P:      }{:available-spare-threshold/%u}\n",
	    letoh(health->available_spare_threshold));
	xo_emit("{Lc:Percentage used}{P:                }{:percentage-used/%u}\n",
	    letoh(health->percentage_used));

	xo_emit("{Lc:Data units (512,000 byte) read}{P: }{:data-units-read/%s}\n",
	    uint128_to_str(to128(health->data_units_read), cbuf, sizeof(cbuf)));
	xo_emit("{Lc:Data units written}{P:             }{:data-units-written/%s}\n",
	    uint128_to_str(to128(health->data_units_written), cbuf, sizeof(cbuf)));
	xo_emit("{Lc:Host read commands}{P:             }{:host-read-commands/%s}\n",
	    uint128_to_str(to128(health->host_read_commands), cbuf, sizeof(cbuf)));
	xo_emit("{Lc:Host write commands}{P:            }{:host-write-commands/%s}\n",
	    uint128_to_str(to128(health->host_write_commands), cbuf, sizeof(cbuf)));
	xo_emit("{Lc:Controller busy time (minutes)}{P: }{:controller-busy-time/%s}\n",
	    uint128_to_str(to128(health->controller_busy_time), cbuf, sizeof(cbuf)));
	xo_emit("{Lc:Power cycles}{P:                   }{:power-cycles/%s}\n",
	    uint128_to_str(to128(health->power_cycles), cbuf, sizeof(cbuf)));
	xo_emit("{Lc:Power on hours}{P:                 }{:power-on-hours/%s}\n",
	    uint128_to_str(to128(health->power_on_hours), cbuf, sizeof(cbuf)));
	xo_emit("{Lc:Unsafe shutdowns}{P:               }{:unsafe-shutdowns/%s}\n",
	    uint128_to_str(to128(health->unsafe_shutdowns), cbuf, sizeof(cbuf)));
	xo_emit("{Lc:Media errors}{P:                   }{:media-errors/%s}\n",
	    uint128_to_str(to128(health->media_errors), cbuf, sizeof(cbuf)));
	xo_emit("{Lc:No. error info log entries}{P:     }{:num-error-info-log-entries/%s}\n",
	    uint128_to_str(to128(health->num_error_info_log_entries), cbuf, sizeof(cbuf)));

	xo_emit("{Lc:Warning Temp Composite Time}{P:    }{:warning-temp-time/%d}\n", letoh(health->warning_temp_time));
	xo_emit("{Lc:Error Temp Composite Time}{P:      }{:error-temp-time/%d}\n", letoh(health->error_temp_time));
	for (i = 0; i < 8; i++) {
		if (letoh(health->temp_sensor[i]) == 0)
			continue;
		xo_emit("{L:Temperature Sensor} {c:temp-sensor/%d}{P:           }", i + 1);
		print_temp_K(letoh(health->temp_sensor[i]));
	}
	xo_emit("{Lc:Temperature 1 Transition Count}{P: }{:temp-1/%d}\n", letoh(health->tmt1tc));
	xo_emit("{Lc:Temperature 2 Transition Count}{P: }{:temp-2/%d}\n", letoh(health->tmt2tc));
	xo_emit("{Lc:Total Time For Temperature 1}{P:   }{:total-time-temp-1/%d}\n", letoh(health->ttftmt1));
	xo_emit("{Lc:Total Time For Temperature 2}{P:   }{:total-time-temp-2/%d}\n", letoh(health->ttftmt2));
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

	xo_emit("{T:Firmware Slot Log}\n");
	xo_emit("{T:=================}\n");

	if (oacs_fw == 0)
		slots = 1;
	else
		slots = MIN(fw_num_slots, MAX_FW_SLOTS);

	for (i = 0; i < slots; i++) {
		xo_emit("{L:Slot }{c:slot/%d}{P: }", i + 1);
		if (afi_slot == i + 1)
			status = "  Active";
		else
			status = "Inactive";

		if (fw->revision[i][0] == '\0')
			xo_emit("Empty\n");
		else
			xo_emit("[{:status/%s}] {:fw-revision/%.8s}\n", status, fw->revision[i]);
	}
}

static void
print_log_ns(const struct nvme_controller_data *cdata __unused, void *buf,
    uint32_t size __unused)
{
	struct nvme_ns_list *nsl;
	u_int i;

	nsl = (struct nvme_ns_list *)buf;
	xo_emit("{T:Changed Namespace List}\n");
	xo_emit("{T:======================}\n");

	for (i = 0; i < nitems(nsl->ns) && letoh(nsl->ns[i]) != 0; i++) {
		xo_emit("{:namespace/%08x}\n", letoh(nsl->ns[i]));
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
	xo_emit("{T:Commands Supported and Effects}\n");
	xo_emit("{T:==============================}\n");
	xo_emit("  Command\tLBCC\tNCC\tNIC\tCCC\tCSE\tUUID\n");

	for (i = 0; i < 255; i++) {
		s = letoh(ce->acs[i]);
		if (NVMEV(NVME_CE_PAGE_CSUP, s) == 0)
			continue;
		xo_emit("Admin\t{:admin-command-set/%02x}\t{:lbcc/%s}\t{:namespace-capability/%s}\t{:namespace-inventory/%s}\t{:controller-capability/%s}\t{:command-submit-exec/%u}\t{:unique-id/%s}\n", i,
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
		xo_emit("I/O\t{:id-io-command-set/%02x}\t{:lbcc/%s}\t{:namespace-capability/%s}\t{:namespace-inventory/%s}\t{:controller-capability/%s}\t{:command-submit-exec/%u}\t{:unique-id/%s}\n", i,
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
	xo_emit("{T:Reservation Notification}\n");
	xo_emit("{T:========================}\n");

	xo_emit("{Lc:Log Page Count}{P:                }{:log-page-count/%ju}\n",
	    (uintmax_t)letoh(rn->log_page_count));
	xo_emit("{Lc:Log Page Type}{P:                 }");
	switch (letoh(rn->log_page_type)) {
	case 0:
		xo_emit("{d:page-type-name/Empty Log Page}{e:page-type/empty-log}\n");
		break;
	case 1:
		xo_emit("{d:page-type-name/Registration Preempted}{e:page-type/registration-preempted}\n");
		break;
	case 2:
		xo_emit("{d:page-type-name/Reservation Released}{e:page-type/reservation-released}\n");
		break;
	case 3:
		xo_emit("{d:page-type-name/Reservation Preempted}{e:page-type/reservation-preempted}\n");
		break;
	default:
		xo_emit("Unknown {:log-page-type/%x}\n", letoh(rn->log_page_type));
		break;
	};
	xo_emit("{Lc:Number of Available Log Pages}{P: }{:available-log-pages/%d}\n", letoh(rn->available_log_pages));
	xo_emit("{Lc:Namespace ID}{P:                 }0x{:namespace-id/%x}\n", letoh(rn->nsid));
}

static void
print_log_sanitize_status(const struct nvme_controller_data *cdata __unused,
    void *buf, uint32_t size __unused)
{
	struct nvme_sanitize_status_page *ss;
	u_int p;
	uint16_t sprog, sstat;

	ss = (struct nvme_sanitize_status_page *)buf;
	xo_emit("{T:Sanitize Status}\n");
	xo_emit("{T:===============}\n");

	sprog = letoh(ss->sprog);
	xo_emit("{Lc:Sanitize Progress}{P:                   }{:progress-percent/%u}%% ({:progress/%u}/65535)\n",
	    (sprog * 100 + 32768) / 65536, sprog);
	xo_emit("{Lc:Sanitize Status}{P:                     }");
	sstat = letoh(ss->sstat);
	switch (NVMEV(NVME_SS_PAGE_SSTAT_STATUS, sstat)) {
	case NVME_SS_PAGE_SSTAT_STATUS_NEVER:
		xo_emit("{d:page-status-name/Never sanitized}{e:page-status/never-sanitized}\n");
		break;
	case NVME_SS_PAGE_SSTAT_STATUS_COMPLETED:
		xo_emit("{d:page-status-name/Completed}{e:page-status/completed}\n");
		break;
	case NVME_SS_PAGE_SSTAT_STATUS_INPROG:
		xo_emit("{d:page-status-name/In Progress}{e:page-status/in-progress}\n");
		break;
	case NVME_SS_PAGE_SSTAT_STATUS_FAILED:
		xo_emit("{d:page-status-name/Failed}{e:page-status/failed}\n");
		break;
	case NVME_SS_PAGE_SSTAT_STATUS_COMPLETEDWD:
		xo_emit("{d:page-status-name/Completed with deallocation}{e:page-status/completed-dealloc}\n");
		break;
	default:
		xo_emit("Unknown 0x{:sstat/%x}", sstat);
		break;
	}
	p = NVMEV(NVME_SS_PAGE_SSTAT_PASSES, sstat);
	if (p > 0)
		xo_emit(", {:passes/%d}{N:passes}", p);
	if (NVMEV(NVME_SS_PAGE_SSTAT_GDE, sstat) != 0)
		xo_emit(", Global Data Erased");
	xo_emit("\n");
	xo_emit("{Lc:Sanitize Command Dword 10}{P:           0x%x}\n", letoh(ss->scdw10));
	xo_emit("{Lc:Time For Overwrite}{P:                  %u} sec\n", letoh(ss->etfo));
	xo_emit("{Lc:Time For Block Erase}{P:                %u} sec\n", letoh(ss->etfbe));
	xo_emit("{Lc:Time For Crypto Erase}{P:               %u} sec\n", letoh(ss->etfce));
	xo_emit("{Lc:Time For Overwrite No-Deallocate}{P:    %u} sec\n", letoh(ss->etfownd));
	xo_emit("{Lc:Time For Block Erase No-Deallocate}{P:  %u} sec\n", letoh(ss->etfbewnd));
	xo_emit("{Lc:Time For Crypto Erase No-Deallocate}{P: %u} sec\n", letoh(ss->etfcewnd));
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
	xo_emit("{T:Device Self-test Status}\n");
	xo_emit("{T:=======================}\n");

	xo_emit("Current Operation: ");
	switch (letoh(dst->curr_operation)) {
	case 0x0:
		xo_emit("{d:page-test-name/No device self-test operation in progress}{e:page-test/no-device}\n");
		break;
	case 0x1:
		xo_emit("{d:page-test-name/Short device self-test operation in progress}{e:page-test/short-device}\n");
		break;
	case 0x2:
		xo_emit("{d:page-test-name/Extended device self-test operation in progress}{e:page-test/extended-device}\n");
		break;
	case 0xe:
		xo_emit("{d:page-test-name/Vendor specific}{e:page-test/vendor-specific}\n");
		break;
	default:
		xo_emit("Reserved (0x{:curr-operation/%x})\n", letoh(dst->curr_operation));
	}

	if (letoh(dst->curr_operation) != 0)
		xo_emit("Current Completion: {:current-completion/%u}%\n", letoh(dst->curr_compl) & 0x7f);

	xo_emit("{T:Results}\n");
	for (r = 0; r < 20; r++) {
		uint64_t failing_lba;
		uint8_t code, res, status;

		status = letoh(dst->result[r].status);
		code = (status >> 4) & 0xf;
		res  = status & 0xf;

		if (res == 0xf)
			continue;

		xo_emit("[{:result/%2u}] ", r);
		switch (code) {
		case 0x1:
			xo_emit("Short device self-test");
			break;
		case 0x2:
			xo_emit("Extended device self-test");
			break;
		case 0xe:
			xo_emit("Vendor specific");
			break;
		default:
			xo_emit("Reserved (0x{:code/%x})", code);
		}
		if (res < self_test_res_max)
			xo_emit("{P: }{:result-status/%s}", self_test_res[res]);
		else
			xo_emit("{P: }Reserved status 0x{:result/%x}", res);

		if (res == 7)
			xo_emit("{P: }starting in segment {:segment-number/%u}",
			    letoh(dst->result[r].segment_num));

#define BIT(b) (1 << (b))
		if (letoh(dst->result[r].valid_diag_info) & BIT(0))
			xo_emit("{P: }NSID=0x{:namespace-id/%x}", letoh(dst->result[r].nsid));
		if (letoh(dst->result[r].valid_diag_info) & BIT(1)) {
			memcpy(&failing_lba, dst->result[r].failing_lba,
			    sizeof(failing_lba));
			xo_emit("{P: }FLBA=0x{:failing-logical-block/%jx}", (uintmax_t)letoh(failing_lba));
		}
		if (letoh(dst->result[r].valid_diag_info) & BIT(2))
			xo_emit("{P: }SCT=0x{:status-code-type/%x}", letoh(dst->result[r].status_code_type));
		if (letoh(dst->result[r].valid_diag_info) & BIT(3))
			xo_emit("{P: }SC=0x{:status-code/%x}", letoh(dst->result[r].status_code));
#undef BIT
		memcpy(&vs, dst->result[r].vendor_specific, sizeof(vs));
		xo_emit("{P: }VENDOR_SPECIFIC=0x{:vs/%x}", letoh(vs));
		xo_emit("\n");
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
