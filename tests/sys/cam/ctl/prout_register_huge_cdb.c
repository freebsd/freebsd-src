/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 ConnectWise
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
 * THIS DOCUMENTATION IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Helper that sends a PERSISTENT RESERVATION OUT command to CTL with a
 * ridiculously huge size for the length of the CDB.  This is not possible with
 * ctladm, for good reason.
 */
#include <camlib.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>

#include <cam/scsi/scsi_message.h>
#include <cam/ctl/ctl_io.h>
#include <cam/ctl/ctl.h>
#include <cam/ctl/ctl_ioctl.h>
#include <cam/ctl/ctl_util.h>

int
main(int argc, char **argv)
{
	union ctl_io *io;
	int fd = open("/dev/cam/ctl", O_RDWR);
	int r;
	uint32_t targ_port;

	if (argc < 2)
		errx(2, "usage: prout_register_huge_cdb <target_port>\n");

	targ_port = strtoul(argv[1], NULL, 10);

	io = calloc(1, sizeof(*io));
	io->io_hdr.nexus.initid = 7;	/* 7 is ctladm's default initiator id */
	io->io_hdr.nexus.targ_port = targ_port;
	io->io_hdr.nexus.targ_mapped_lun = 0;
	io->io_hdr.nexus.targ_lun = 0;
	io->io_hdr.io_type = CTL_IO_SCSI;
	io->taskio.tag_type = CTL_TAG_UNTAGGED;
	uint8_t cdb[32] = {};
	// ctl_persistent_reserve_out// 5f 00
	cdb[0] = 0x5f;
	cdb[1] = 0x00;
	struct scsi_per_res_out *cdb_ = ( struct scsi_per_res_out *)cdb;
	// Claim an enormous size of the CDB, but don't actually alloc it all.
	cdb_->length[0] = 0xff;
	cdb_->length[1] = 0xff;
	cdb_->length[2] = 0xff;
	cdb_->length[3] = 0xff;
	io->scsiio.cdb_len = sizeof(cdb);
	memcpy(io->scsiio.cdb, cdb, sizeof(cdb));
	io->io_hdr.flags |= CTL_FLAG_DATA_IN;
	r = ioctl(fd, CTL_IO, io);
	if (r == -1)
		err(1, "ioctl");
	if ((io->io_hdr.status & CTL_STATUS_MASK) == CTL_SUCCESS) {
		return (0);
	} else {
		return (1);
	}
}
