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
#include <sys/stat.h>
#include <sys/types.h>

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

static int
slot_has_valid_firmware(int fd, int slot)
{
	struct nvme_firmware_page	fw;
	int				has_fw = false;

	read_logpage(fd, NVME_LOG_FIRMWARE_SLOT,
	    NVME_GLOBAL_NAMESPACE_TAG, &fw, sizeof(fw));

	if (fw.revision[slot-1] != 0LLU)
		has_fw = true;

	return (has_fw);
}

static void
read_image_file(char *path, void **buf, ssize_t *size)
{
	struct stat	sb;
	int		fd;

	*size = 0;
	*buf = NULL;

	if ((fd = open(path, O_RDONLY)) < 0) {
		fprintf(stderr, "Unable to open '%s'.\n", path);
		exit(EX_IOERR);
	}
	if (fstat(fd, &sb) < 0) {
		fprintf(stderr, "Unable to stat '%s'.\n", path);
		close(fd);
		exit(EX_IOERR);
	}
	if ((*buf = malloc(sb.st_size)) == NULL) {
		fprintf(stderr, "Unable to malloc %zd bytes.\n",
		    sb.st_size);
		close(fd);
		exit(EX_IOERR);
	}
	if ((*size = read(fd, *buf, sb.st_size)) < 0) {
		fprintf(stderr, "Error reading '%s', errno=%d (%s)\n",
		    path, errno, strerror(errno));
		close(fd);
		exit(EX_IOERR);
	}
	if (*size != sb.st_size) {
		fprintf(stderr, "Error reading '%s', "
		    "read %zd bytes, requested %zd bytes\n",
		    path, *size, sb.st_size);
		close(fd);
		exit(EX_IOERR);
	}
}

static void
update_firmware(int fd, uint8_t *payload, uint32_t payload_size)
{
	struct nvme_pt_command	pt;
	size_t			size;
	void			*chunk;
	uint32_t		off, resid;
	int			exit_code = EX_OK;

	off = 0;
	resid = payload_size;

	if ((chunk = malloc((size_t)NVME_MAX_XFER_SIZE)) == NULL) {
		printf("Unable to malloc %d bytes.\n", NVME_MAX_XFER_SIZE);
		exit(EX_IOERR);
	}

	while (resid > 0) {
		size = (resid >= NVME_MAX_XFER_SIZE) ?
		    NVME_MAX_XFER_SIZE : resid;
		memcpy(chunk, payload + off, size);

		memset(&pt, 0, sizeof(pt));
		pt.cmd.opc = NVME_OPC_FIRMWARE_IMAGE_DOWNLOAD;
		pt.cmd.cdw10 = (size / sizeof(uint32_t)) - 1;
		pt.cmd.cdw11 = (off / sizeof(uint32_t));
		pt.buf = chunk;
		pt.len = size;
		pt.is_read = 0;

		if (ioctl(fd, NVME_PASSTHROUGH_CMD, &pt) < 0) {
			printf("Firmware image download request failed. "
			    "errno=%d (%s)\n",
			    errno, strerror(errno));
			exit_code = EX_IOERR;
			break;
		}

		if (nvme_completion_is_error(&pt.cpl)) {
			printf("Passthrough command returned error.\n");
			exit_code = EX_IOERR;
			break;
		}

		resid -= size;
		off += size;
	}
	
	if (exit_code != EX_OK)
		exit(exit_code);
}

static void
activate_firmware(int fd, int slot, int activate_action)
{
	struct nvme_pt_command	pt;

	memset(&pt, 0, sizeof(pt));
	pt.cmd.opc = NVME_OPC_FIRMWARE_ACTIVATE;
	pt.cmd.cdw10 = (activate_action << 3) | slot;
	pt.is_read = 0;

	if (ioctl(fd, NVME_PASSTHROUGH_CMD, &pt) < 0) {
		printf("Firmware activate request failed. errno=%d (%s)\n",
		    errno, strerror(errno));
		exit(EX_IOERR);
	}

	if (nvme_completion_is_error(&pt.cpl)) {
		printf("Passthrough command returned error.\n");
		exit(EX_IOERR);
	}
}

static void
firmware_usage(void)
{
	fprintf(stderr, "usage:\n");
	fprintf(stderr, FIRMWARE_USAGE);
	exit(EX_USAGE);
}

void
firmware(int argc, char *argv[])
{
	int				fd = -1, slot = 0;
	int				a_flag, s_flag, f_flag;
	char				ch, *p, *image = NULL;
	char				*controller = NULL, prompt[64];
	void				*buf = NULL;
	ssize_t				size;
	struct nvme_controller_data	cdata;

	a_flag = s_flag = f_flag = false;

	while ((ch = getopt(argc, argv, "af:s:")) != -1) {
		switch (ch) {
		case 'a':
			a_flag = true;
			break;
		case 's':
			slot = strtol(optarg, &p, 0);
			if (p != NULL && *p != '\0') {
				fprintf(stderr,
				    "\"%s\" not valid slot.\n",
				    optarg);
				firmware_usage();
			} else if (slot == 0) {
				fprintf(stderr,
				    "0 is not a valid slot number. "
				    "Slot numbers start at 1.\n");
				firmware_usage();
			} else if (slot > 7) {
				fprintf(stderr,
				    "Slot number %s specified which is "
				    "greater than max allowed slot number of "
				    "7.\n", optarg);
				firmware_usage();
			}
			s_flag = true;
			break;
		case 'f':
			image = optarg;
			f_flag = true;
			break;
		}
	}

	/* Check that a controller (and not a namespace) was specified. */
	if (optind >= argc || strstr(argv[optind], NVME_NS_PREFIX) != NULL)
		firmware_usage();

	if (!f_flag && !a_flag) {
		fprintf(stderr,
		    "Neither a replace ([-f path_to_firmware]) nor "
		    "activate ([-a]) firmware image action\n"
		    "was specified.\n");
		firmware_usage();
	}

	if (!f_flag && a_flag && slot == 0) {
		fprintf(stderr,
		    "Slot number to activate not specified.\n");
		firmware_usage();
	}

	controller = argv[optind];
	open_dev(controller, &fd, 1, 1);
	read_controller_data(fd, &cdata);

	if (cdata.oacs.firmware == 0) {
		fprintf(stderr, 
		    "Controller does not support firmware "
		    "activate/download.\n");
		exit(EX_IOERR);
	}

	if (f_flag && slot == 1 && cdata.frmw.slot1_ro) {
		fprintf(stderr, "Slot %d is marked as read only.\n", slot);
		exit(EX_IOERR);
	}

	if (slot > cdata.frmw.num_slots) {
		fprintf(stderr,
		    "Slot %d was specified but controller only "
		    "supports %d firmware slots.\n",
		    slot, cdata.frmw.num_slots);
		exit(EX_IOERR);
	}

	if (!slot_has_valid_firmware(fd, slot)) {
		fprintf(stderr,
		    "Slot %d does not contain valid firmware.\n"
		    "Try 'nvmecontrol logpage -p 3 %s' to get a list "
		    "of available firmware images.\n",
		    slot, controller);
		exit(EX_IOERR);
	}

	if (f_flag && a_flag)
		printf("You are about to download and activate "
		       "firmware image (%s) to controller %s.\n"
		       "This may damage your controller and/or "
		       "overwrite an existing firmware image.\n",
		       image, controller);
	else if (a_flag)
		printf("You are about to activate a new firmware "
		       "image on controller %s.\n"
		       "This may damage your controller.\n",
		       controller);
	else if (f_flag)
		printf("You are about to download firmware image "
		       "(%s) to controller %s.\n"
		       "This may damage your controller and/or "
		       "overwrite an existing firmware image.\n",
		       image, controller);

	printf("Are you sure you want to continue? (yes/no) ");
	while (1) {
		fgets(prompt, sizeof(prompt), stdin);
		if (strncasecmp(prompt, "yes", 3) == 0)
			break;
		if (strncasecmp(prompt, "no", 2) == 0)
			exit(EX_OK);
		printf("Please answer \"yes\" or \"no\". ");
	}

	if (f_flag) {
		read_image_file(image, &buf, &size);
		update_firmware(fd, buf, size);
		if (a_flag)
			activate_firmware(fd, slot,
			    NVME_AA_REPLACE_ACTIVATE);
		else
			activate_firmware(fd, slot,
			    NVME_AA_REPLACE_NO_ACTIVATE);
	} else {
		activate_firmware(fd, slot, NVME_AA_ACTIVATE);
	}

	if (a_flag) {
		printf("New firmware image activated and will take "
		       "effect after next controller reset.\n"
		       "Controller reset can be initiated via "
		       "'nvmecontrol reset %s'\n",
		       controller);
	}

	close(fd);
	exit(EX_OK);
}
