/*-
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

#if _BYTE_ORDER != _LITTLE_ENDIAN
#error "Code only works on little endian machines"
#endif

#include "nvmecontrol.h"

#define DEFAULT_SIZE	(4096)
#define MAX_FW_SLOTS	(7)

typedef void (*print_fn_t)(void *buf, uint32_t size);


/*
 * 128-bit integer augments to standard values
 */
#define UINT128_DIG	39
typedef __uint128_t uint128_t;

static inline uint128_t
to128(void *p)
{
	return *(uint128_t *)p;
}

static char *
uint128_to_str(uint128_t u, char *buf, size_t buflen)
{
	char *end = buf + buflen - 1;

	*end-- = '\0';
	if (u == 0)
		*end-- = '0';
	while (u && end >= buf) {
		*end-- = u % 10 + '0';
		u /= 10;
	}
	end++;
	if (u != 0)
		return NULL;

	return end;
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
read_logpage(int fd, uint8_t log_page, int nsid, void *payload, 
    uint32_t payload_size)
{
	struct nvme_pt_command	pt;

	memset(&pt, 0, sizeof(pt));
	pt.cmd.opc = NVME_OPC_GET_LOG_PAGE;
	pt.cmd.nsid = nsid;
	pt.cmd.cdw10 = ((payload_size/sizeof(uint32_t)) - 1) << 16;
	pt.cmd.cdw10 |= log_page;
	pt.buf = payload;
	pt.len = payload_size;
	pt.is_read = 1;

	if (ioctl(fd, NVME_PASSTHROUGH_CMD, &pt) < 0)
		err(1, "get log page request failed");

	if (nvme_completion_is_error(&pt.cpl))
		errx(1, "get log page request returned error");
}

static void
print_log_error(void *buf, uint32_t size)
{
	int					i, nentries;
	struct nvme_error_information_entry	*entry = buf;
	struct nvme_status			*status;

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

		status = &entry->status;
		printf("Entry %02d\n", i + 1);
		printf("=========\n");
		printf(" Error count:          %ju\n", entry->error_count);
		printf(" Submission queue ID:  %u\n", entry->sqid);
		printf(" Command ID:           %u\n", entry->cid);
		/* TODO: Export nvme_status_string structures from kernel? */
		printf(" Status:\n");
		printf("  Phase tag:           %d\n", status->p);
		printf("  Status code:         %d\n", status->sc);
		printf("  Status code type:    %d\n", status->sct);
		printf("  More:                %d\n", status->m);
		printf("  DNR:                 %d\n", status->dnr);
		printf(" Error location:       %u\n", entry->error_location);
		printf(" LBA:                  %ju\n", entry->lba);
		printf(" Namespace ID:         %u\n", entry->nsid);
		printf(" Vendor specific info: %u\n", entry->vendor_specific);
	}
}

static void
print_log_health(void *buf, uint32_t size __unused)
{
	struct nvme_health_information_page *health = buf;
	char cbuf[UINT128_DIG + 1];

	printf("SMART/Health Information Log\n");
	printf("============================\n");

	printf("Critical Warning State:         0x%02x\n",
	    health->critical_warning.raw);
	printf(" Available spare:               %d\n",
	    health->critical_warning.bits.available_spare);
	printf(" Temperature:                   %d\n",
	    health->critical_warning.bits.temperature);
	printf(" Device reliability:            %d\n",
	    health->critical_warning.bits.device_reliability);
	printf(" Read only:                     %d\n",
	    health->critical_warning.bits.read_only);
	printf(" Volatile memory backup:        %d\n",
	    health->critical_warning.bits.volatile_memory_backup);
	printf("Temperature:                    %u K, %2.2f C, %3.2f F\n",
	    health->temperature,
	    (float)health->temperature - (float)273.15,
	    ((float)health->temperature * (float)9/5) - (float)459.67);
	printf("Available spare:                %u\n",
	    health->available_spare);
	printf("Available spare threshold:      %u\n",
	    health->available_spare_threshold);
	printf("Percentage used:                %u\n",
	    health->percentage_used);

	printf("Data units (512,000 byte) read:     %s\n",
	    uint128_to_str(to128(health->data_units_read), cbuf, sizeof(cbuf)));
	printf("Data units (512,000 byte) written:  %s\n",
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
}

static void
print_log_firmware(void *buf, uint32_t size __unused)
{
	int				i;
	const char			*status;
	struct nvme_firmware_page	*fw = buf;

	printf("Firmware Slot Log\n");
	printf("=================\n");

	for (i = 0; i < MAX_FW_SLOTS; i++) {
		printf("Slot %d: ", i + 1);
		if (fw->afi.slot == i + 1)
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

static struct logpage_function {
	uint8_t		log_page;
	print_fn_t	print_fn;
	size_t		size;
} logfuncs[] = {
	{NVME_LOG_ERROR,		print_log_error,
	 0},
	{NVME_LOG_HEALTH_INFORMATION,	print_log_health,
	 sizeof(struct nvme_health_information_page)},
	{NVME_LOG_FIRMWARE_SLOT,	print_log_firmware,
	 sizeof(struct nvme_firmware_page)},
	{0,				NULL,
	 0},
};

static void
logpage_usage(void)
{
	fprintf(stderr, "usage:\n");
	fprintf(stderr, LOGPAGE_USAGE);
	exit(1);
}

void
logpage(int argc, char *argv[])
{
	int				fd, nsid;
	int				log_page = 0, pageflag = false;
	int				hexflag = false, ns_specified;
	char				ch, *p;
	char				cname[64];
	uint32_t			size;
	void				*buf;
	struct logpage_function		*f;
	struct nvme_controller_data	cdata;
	print_fn_t			print_fn;

	while ((ch = getopt(argc, argv, "p:x")) != -1) {
		switch (ch) {
		case 'p':
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

	/*
	 * The log page attribtues indicate whether or not the controller
	 * supports the SMART/Health information log page on a per
	 * namespace basis.
	 */
	if (ns_specified) {
		if (log_page != NVME_LOG_HEALTH_INFORMATION)
			errx(1, "log page %d valid only at controller level",
			    log_page);
		if (cdata.lpa.ns_smart == 0)
			errx(1,
			    "controller does not support per namespace "
			    "smart/health information");
	}

	print_fn = print_hex;
	size = DEFAULT_SIZE;
	if (!hexflag) {
		/*
		 * See if there is a pretty print function for the
		 *  specified log page.  If one isn't found, we
		 *  just revert to the default (print_hex).
		 */
		f = logfuncs;
		while (f->log_page > 0) {
			if (log_page == f->log_page) {
				print_fn = f->print_fn;
				size = f->size;
				break;
			}
			f++;
		}
	}

	if (log_page == NVME_LOG_ERROR) {
		size = sizeof(struct nvme_error_information_entry);
		size *= (cdata.elpe + 1);
	}

	/* Read the log page */
	buf = get_log_buffer(size);
	read_logpage(fd, log_page, nsid, buf, size);
	print_fn(buf, size);

	close(fd);
	exit(0);
}
