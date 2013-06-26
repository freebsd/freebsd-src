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
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "nvmecontrol.h"

#define DEFAULT_SIZE	(4096)
#define MAX_FW_SLOTS	(7)

typedef void (*print_fn_t)(void *buf, uint32_t size);

static void *
get_log_buffer(size_t size)
{
	void	*buf;

	if ((buf = malloc(size)) == NULL) {
		fprintf(stderr, "Unable to malloc %zd bytes\n", size);
		exit(EX_IOERR);
	}
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

	if (ioctl(fd, NVME_PASSTHROUGH_CMD, &pt) < 0) {
		printf("Get log page request failed. errno=%d (%s)\n",
		    errno, strerror(errno));
		exit(EX_IOERR);
	}

	if (nvme_completion_is_error(&pt.cpl)) {
		printf("Passthrough command returned error.\n");
		exit(EX_IOERR);
	}
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

	/* 
	 * TODO: These are pretty ugly in hex. Is there a library that 
	 *	 will convert 128-bit unsigned values to decimal?
	 */
	printf("Data units (512 byte) read:     0x%016jx%016jx\n",
	    health->data_units_read[1],
	    health->data_units_read[0]);
	printf("Data units (512 byte) written:  0x%016jx%016jx\n",
	    health->data_units_written[1],
	    health->data_units_written[0]);
	printf("Host read commands:             0x%016jx%016jx\n",
	    health->host_read_commands[1],
	    health->host_read_commands[0]);
	printf("Host write commands:            0x%016jx%016jx\n",
	    health->host_write_commands[1],
	    health->host_write_commands[0]);
	printf("Controller busy time (minutes): 0x%016jx%016jx\n",
	    health->controller_busy_time[1],
	    health->controller_busy_time[0]);
	printf("Power cycles:                   0x%016jx%016jx\n",
	    health->power_cycles[1],
	    health->power_cycles[0]);
	printf("Power on hours:                 0x%016jx%016jx\n",
	    health->power_on_hours[1],
	    health->power_on_hours[0]);
	printf("Unsafe shutdowns:               0x%016jx%016jx\n",
	    health->unsafe_shutdowns[1],
	    health->unsafe_shutdowns[0]);
	printf("Media errors:                   0x%016jx%016jx\n",
	    health->media_errors[1],
	    health->media_errors[0]);
	printf("No. error info log entries:     0x%016jx%016jx\n",
	    health->num_error_info_log_entries[1],
	    health->num_error_info_log_entries[0]);
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

struct logpage_function {
	uint8_t		log_page;
	print_fn_t	fn;
} logfuncs[] = {
	{NVME_LOG_ERROR,		print_log_error		},
	{NVME_LOG_HEALTH_INFORMATION,	print_log_health	},
	{NVME_LOG_FIRMWARE_SLOT,	print_log_firmware	},
	{0,				NULL			},
};

static void
logpage_usage(void)
{
	fprintf(stderr, "usage:\n");
	fprintf(stderr, LOGPAGE_USAGE);
	exit(EX_USAGE);
}

void
logpage(int argc, char *argv[])
{
	int				fd, nsid, len;
	int				log_page = 0, pageflag = false;
	int				hexflag = false;
	int				allow_ns = false;
	char				ch, *p, *nsloc = NULL;
	char				*cname = NULL;
	size_t				size;
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
			/* TODO: Define valid log page id ranges in nvme.h? */
			} else if (log_page == 0 ||
				   (log_page >= 0x04 && log_page <= 0x7F) ||
				   (log_page >= 0x80 && log_page <= 0xBF)) {
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

	/*
	 * The log page attribtues indicate whether or not the controller
	 * supports the SMART/Health information log page on a per
	 * namespace basis.
	 */
	cname = malloc(strlen(NVME_CTRLR_PREFIX) + 2);
	len = strlen(NVME_CTRLR_PREFIX) + 1;
	cname = strncpy(cname, argv[optind], len);
	open_dev(cname, &fd, 1, 1);
	read_controller_data(fd, &cdata);

	if (log_page == NVME_LOG_HEALTH_INFORMATION && cdata.lpa.ns_smart != 0)
		allow_ns = true;

	/* If a namespace id was specified, validate it's use */
	if (strstr(argv[optind], NVME_NS_PREFIX) != NULL) {
		if (!allow_ns) {
			if (log_page != NVME_LOG_HEALTH_INFORMATION) {
				fprintf(stderr,
				    "Namespace ID not valid for log page %d.\n",
				    log_page);
			} else if (cdata.lpa.ns_smart == 0) {
				fprintf(stderr,
				    "Controller does not support per "
				    "namespace SMART/Health information.\n");
			}
			close(fd);
			exit(EX_IOERR);
		}
		nsloc = strnstr(argv[optind], NVME_NS_PREFIX, 10);
		if (nsloc != NULL)
			nsid = strtol(nsloc + 2, NULL, 10);
		if (nsloc == NULL || (nsid == 0 && errno != 0)) {
			fprintf(stderr,
			    "Invalid namespace ID %s.\n",
			    argv[optind]);
			close(fd);
			exit(EX_IOERR);
		}

		/*
		 * User is asking for per namespace log page information
		 * so close the controller and open up the namespace.
		 */
		close(fd);
		open_dev(argv[optind], &fd, 1, 1);
	} else
		nsid = NVME_GLOBAL_NAMESPACE_TAG;

	print_fn = print_hex;
	if (!hexflag) {
		/*
		 * See if there is a pretty print function for the
		 *  specified log page.  If one isn't found, we
		 *  just revert to the default (print_hex).
		 */
		f = logfuncs;
		while (f->log_page > 0) {
			if (log_page == f->log_page) {
				print_fn = f->fn;
				break;
			}
			f++;
		}
	}

	/* Read the log page */
	switch (log_page) {
	case NVME_LOG_ERROR:
		size = sizeof(struct nvme_error_information_entry);
		size *= (cdata.elpe + 1);
		break;
	case NVME_LOG_HEALTH_INFORMATION:
		size = sizeof(struct nvme_health_information_page);
		break;
	case NVME_LOG_FIRMWARE_SLOT:
		size = sizeof(struct nvme_firmware_page);
		break;
	default:
		size = DEFAULT_SIZE;
		break;
	}

	buf = get_log_buffer(size);
	read_logpage(fd, log_page, nsid, buf, size);
	print_fn(buf, size);

	close(fd);
	exit(EX_OK);
}
