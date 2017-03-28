/*-
 * Copyright (c) 2017 Netflix, Inc
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
#include <sys/endian.h>

#include <ctype.h>
#include <err.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "nvmecontrol.h"

#define WDC_NVME_TOC_SIZE	8

#define WDC_NVME_CAP_DIAG_OPCODE	0xe6
#define WDC_NVME_CAP_DIAG_CMD		0x0000

#define WDC_NVME_DIAG_OPCODE		0xc6
#define WDC_NVME_DRIVE_LOG_SIZE_CMD	0x0120
#define WDC_NVME_DRIVE_LOG_CMD		0x0020
#define WDC_NVME_CRASH_DUMP_SIZE_CMD	0x0320
#define WDC_NVME_CRASH_DUMP_CMD		0x0420
#define WDC_NVME_PFAIL_DUMP_SIZE_CMD	0x0520
#define WDC_NVME_PFAIL_DUMP_CMD		0x0620

#define WDC_NVME_CLEAR_DUMP_OPCODE	0xff
#define WDC_NVME_CLEAR_CRASH_DUMP_CMD	0x0503
#define WDC_NVME_CLEAR_PFAIL_DUMP_CMD	0x0603

static void wdc_cap_diag(int argc, char *argv[]);
static void wdc_drive_log(int argc, char *argv[]);
static void wdc_get_crash_dump(int argc, char *argv[]);
static void wdc_purge(int argc, char *argv[]);
static void wdc_purge_monitor(int argc, char *argv[]);

#define WDC_CAP_DIAG_USAGE	"\tnvmecontrol wdc cap-diag [-o path-template]\n"
#define WDC_DRIVE_LOG_USAGE	"\tnvmecontrol wdc drive-log [-o path-template]\n"
#define WDC_GET_CRASH_DUMP_USAGE "\tnvmecontrol wdc get-crash-dump [-o path-template]\n"
#define WDC_PURGE_USAGE		"\tnvmecontrol wdc purge [-o path-template]\n"
#define WDC_PURGE_MONITOR_USAGE	"\tnvmecontrol wdc purge-monitor\n"

static struct nvme_function wdc_funcs[] = {
	{"cap-diag",		wdc_cap_diag,		WDC_CAP_DIAG_USAGE},
	{"drive-log",		wdc_drive_log,		WDC_DRIVE_LOG_USAGE},
	{"get-crash-dump",	wdc_get_crash_dump,	WDC_GET_CRASH_DUMP_USAGE},
	{"purge",		wdc_purge,		WDC_PURGE_USAGE},
	{"purge_monitor",	wdc_purge_monitor,	WDC_PURGE_MONITOR_USAGE},
	{NULL,			NULL,			NULL},
};

static void
wdc_append_serial_name(int fd, char *buf, size_t len, const char *suffix)
{
	struct nvme_controller_data	cdata;
	char sn[NVME_SERIAL_NUMBER_LENGTH + 1];
	char *walker;

	len -= strlen(buf);
	buf += strlen(buf);
	read_controller_data(fd, &cdata);
	memcpy(sn, cdata.sn, NVME_SERIAL_NUMBER_LENGTH);
	walker = sn + NVME_SERIAL_NUMBER_LENGTH - 1;
	while (walker > sn && *walker == ' ')
		walker--;
	*++walker = '\0';
	snprintf(buf, len, "%s%s.bin", sn, suffix);
}

static void
wdc_get_data(int fd, uint32_t opcode, uint32_t len, uint32_t off, uint32_t cmd,
    uint8_t *buffer, size_t buflen)
{
	struct nvme_pt_command	pt;

	memset(&pt, 0, sizeof(pt));
	pt.cmd.opc = opcode;
	pt.cmd.cdw10 = len / sizeof(uint32_t);	/* - 1 like all the others ??? */
	pt.cmd.cdw11 = off / sizeof(uint32_t);
	pt.cmd.cdw12 = cmd;
	pt.buf = buffer;
	pt.len = buflen;
	pt.is_read = 1;
//	printf("opcode %#x cdw10(len) %#x cdw11(offset?) %#x cdw12(cmd/sub) %#x buflen %zd\n",
//	    (int)opcode, (int)cdw10, (int)cdw11, (int)cdw12, buflen);

	if (ioctl(fd, NVME_PASSTHROUGH_CMD, &pt) < 0)
		err(1, "wdc_get_data request failed");
	if (nvme_completion_is_error(&pt.cpl))
		errx(1, "wdc_get_data request returned error");
}

static void
wdc_do_dump(int fd, char *tmpl, const char *suffix, uint32_t opcode,
    uint32_t size_cmd, uint32_t cmd, int len_off)
{
	int fd2;
	uint8_t *buf;
	uint32_t len, offset;
	ssize_t resid;

	wdc_append_serial_name(fd, tmpl, MAXPATHLEN, suffix);

	buf = aligned_alloc(PAGE_SIZE, WDC_NVME_TOC_SIZE);
	if (buf == NULL)
		errx(1, "Can't get buffer to get size");
	wdc_get_data(fd, opcode, WDC_NVME_TOC_SIZE,
	    0, size_cmd, buf, WDC_NVME_TOC_SIZE);
	len = be32dec(buf + len_off);

	if (len == 0)
		errx(1, "No data for %s", suffix);

	printf("Dumping %d bytes to %s\n", len, tmpl);
	/* XXX overwrite protection? */
	fd2 = open(tmpl, O_WRONLY | O_CREAT | O_TRUNC);
	if (fd2 < 0)
		err(1, "open %s", tmpl);
	offset = 0;
	buf = aligned_alloc(PAGE_SIZE, NVME_MAX_XFER_SIZE);
	if (buf == NULL)
		errx(1, "Can't get buffer to read dump");
	while (len > 0) {
		resid = len > NVME_MAX_XFER_SIZE ? NVME_MAX_XFER_SIZE : len;
		wdc_get_data(fd, opcode, resid, offset, cmd, buf, resid);
		if (write(fd2, buf, resid) != resid)
			err(1, "write");
		offset += resid;
		len -= resid;
	}
	free(buf);
	close(fd2);
}

static void
wdc_do_clear_dump(int fd, uint32_t opcode, uint32_t cmd)
{
	struct nvme_pt_command	pt;

	memset(&pt, 0, sizeof(pt));
	pt.cmd.opc = opcode;
	pt.cmd.cdw12 = cmd;
	if (ioctl(fd, NVME_PASSTHROUGH_CMD, &pt) < 0)
		err(1, "wdc_do_clear_dump request failed");
	if (nvme_completion_is_error(&pt.cpl))
		errx(1, "wdc_do_clear_dump request returned error");
}

static void
wdc_cap_diag_usage(void)
{
	fprintf(stderr, "usage:\n");
	fprintf(stderr, WDC_CAP_DIAG_USAGE);
	exit(1);
}

static void
wdc_cap_diag(int argc, char *argv[])
{
	char path_tmpl[MAXPATHLEN];
	int ch, fd;

	path_tmpl[0] = '\0';
	while ((ch = getopt(argc, argv, "o:")) != -1) {
		switch ((char)ch) {
		case 'o':
			strlcpy(path_tmpl, optarg, MAXPATHLEN);
			break;
		default:
			wdc_cap_diag_usage();
		}
	}
	/* Check that a controller was specified. */
	if (optind >= argc)
		wdc_cap_diag_usage();
	open_dev(argv[optind], &fd, 1, 1);

	wdc_do_dump(fd, path_tmpl, "cap_diag", WDC_NVME_CAP_DIAG_OPCODE,
	    WDC_NVME_CAP_DIAG_CMD, WDC_NVME_CAP_DIAG_CMD, 4);

	close(fd);

	exit(1);	
}

static void
wdc_drive_log_usage(void)
{
	fprintf(stderr, "usage:\n");
	fprintf(stderr, WDC_DRIVE_LOG_USAGE);
	exit(1);
}

static void
wdc_drive_log(int argc, char *argv[])
{
	char path_tmpl[MAXPATHLEN];
	int ch, fd;

	path_tmpl[0] = '\0';
	while ((ch = getopt(argc, argv, "o:")) != -1) {
		switch ((char)ch) {
		case 'o':
			strlcpy(path_tmpl, optarg, MAXPATHLEN);
			break;
		default:
			wdc_drive_log_usage();
		}
	}
	/* Check that a controller was specified. */
	if (optind >= argc)
		wdc_drive_log_usage();
	open_dev(argv[optind], &fd, 1, 1);

	wdc_do_dump(fd, path_tmpl, "drive_log", WDC_NVME_DIAG_OPCODE,
	    WDC_NVME_DRIVE_LOG_SIZE_CMD, WDC_NVME_DRIVE_LOG_CMD, 0);

	close(fd);

	exit(1);
}

static void
wdc_get_crash_dump_usage(void)
{
	fprintf(stderr, "usage:\n");
	fprintf(stderr, WDC_CAP_DIAG_USAGE);
	exit(1);
}

static void
wdc_get_crash_dump(int argc, char *argv[])
{
	char path_tmpl[MAXPATHLEN];
	int ch, fd;

	while ((ch = getopt(argc, argv, "o:")) != -1) {
		switch ((char)ch) {
		case 'o':
			strlcpy(path_tmpl, optarg, MAXPATHLEN);
			break;
		default:
			wdc_get_crash_dump_usage();
		}
	}
	/* Check that a controller was specified. */
	if (optind >= argc)
		wdc_get_crash_dump_usage();
	open_dev(argv[optind], &fd, 1, 1);

	wdc_do_dump(fd, path_tmpl, "crash_dump", WDC_NVME_DIAG_OPCODE,
	    WDC_NVME_CRASH_DUMP_SIZE_CMD, WDC_NVME_CRASH_DUMP_CMD, 0);
	wdc_do_clear_dump(fd, WDC_NVME_CLEAR_DUMP_OPCODE,
	    WDC_NVME_CLEAR_CRASH_DUMP_CMD);
//	wdc_led_beacon_disable(fd);
	wdc_do_dump(fd, path_tmpl, "pfail_dump", WDC_NVME_DIAG_OPCODE,
	    WDC_NVME_PFAIL_DUMP_SIZE_CMD, WDC_NVME_PFAIL_DUMP_CMD, 0);
	wdc_do_clear_dump(fd, WDC_NVME_CLEAR_DUMP_OPCODE,
		WDC_NVME_CLEAR_PFAIL_DUMP_CMD);

	close(fd);

	exit(1);
}

static void
wdc_purge(int argc, char *argv[])
{
	char path_tmpl[MAXPATHLEN];
	int ch;

	while ((ch = getopt(argc, argv, "o:")) != -1) {
		switch ((char)ch) {
		case 'o':
			strlcpy(path_tmpl, optarg, MAXPATHLEN);
			break;
		default:
			wdc_cap_diag_usage();
		}
	}

	printf("purge has not been implemented.\n");
	exit(1);
}

static void
wdc_purge_monitor(int argc, char *argv[])
{
	char path_tmpl[MAXPATHLEN];
	int ch;

	while ((ch = getopt(argc, argv, "o:")) != -1) {
		switch ((char)ch) {
		case 'o':
			strlcpy(path_tmpl, optarg, MAXPATHLEN);
			break;
		default:
			wdc_cap_diag_usage();
		}
	}

	printf("purge has not been implemented.\n");
	exit(1);
}

void
wdc(int argc, char *argv[])
{

	dispatch(argc, argv, wdc_funcs);
}
