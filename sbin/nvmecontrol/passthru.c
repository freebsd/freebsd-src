/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
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

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "nvmecontrol.h"
#include "comnd.h"

static struct options {
	uint8_t		opcode;
	uint8_t		flags;
	uint16_t	rsvd;
	uint32_t	nsid;
	uint32_t	data_len;
	uint32_t	metadata_len;
	uint32_t	timeout;
	uint32_t	cdw2;
	uint32_t	cdw3;
	uint32_t	cdw10;
	uint32_t	cdw11;
	uint32_t	cdw12;
	uint32_t	cdw13;
	uint32_t	cdw14;
	uint32_t	cdw15;
	const char	*ifn;
	bool		binary;
	bool		show_command;
	bool		dry_run;
	bool		read;
	bool		write;
	uint8_t		prefill;
	const char	*dev;
} opt = {
	.binary = false,
	.cdw10 = 0,
	.cdw11 = 0,
	.cdw12 = 0,
	.cdw13 = 0,
	.cdw14 = 0,
	.cdw15 = 0,
	.cdw2 = 0,
	.cdw3 = 0,
	.data_len = 0,
	.dry_run = false,
	.flags = 0,
	.ifn = "",
	.metadata_len = 0,
	.nsid = 0,
	.opcode = 0,
	.prefill = 0,
	.read = false,
	.rsvd = 0,
	.show_command = false,
	.timeout = 0,
	.write = false,
	.dev = NULL,
};

/*
 * Argument names and short names selected to match the nvme-cli program
 * so vendor-siupplied formulas work out of the box on FreeBSD with a simple
 * s/nvme/nvmecontrol/.
 */
#define ARG(l, s, t, opt, addr, desc) { l, s, t, &opt.addr, desc }

static struct opts opts[] = {
	ARG("opcode",		'o',	arg_uint8,	opt, opcode,
	    "NVMe command opcode (required)"),
	ARG("cdw2",		'2',	arg_uint32,	opt, cdw2,
	    "Command dword 2 value"),
	ARG("cdw3",		'3',	arg_uint32,	opt, cdw3,
	    "Command dword 3 value"),
	ARG("cdw10",		'4',	arg_uint32,	opt, cdw10,
	    "Command dword 10 value"),
	ARG("cdw11",		'5',	arg_uint32,	opt, cdw11,
	    "Command dword 11 value"),
	ARG("cdw12",		'6',	arg_uint32,	opt, cdw12,
	    "Command dword 12 value"),
	ARG("cdw13",		'7',	arg_uint32,	opt, cdw13,
	    "Command dword 13 value"),
	ARG("cdw14",		'8',	arg_uint32,	opt, cdw14,
	    "Command dword 14 value"),
	ARG("cdw15",		'9',	arg_uint32,	opt, cdw15,
	    "Command dword 15 value"),
	ARG("data-len",		'l',	arg_uint32,	opt, data_len,
	    "Length of data for I/O (bytes)"),
	ARG("metadata-len",	'm',	arg_uint32,	opt, metadata_len,
	    "Length of metadata segment (bytes) (igored)"),
	ARG("flags",		'f',	arg_uint8,	opt, flags,
	    "NVMe command flags"),
	ARG("input-file",	'i',	arg_path,	opt, ifn,
	    "Input file to send (default stdin)"),
	ARG("namespace-id",	'n',	arg_uint32,	opt, nsid,
	    "Namespace id (ignored on FreeBSD)"),
	ARG("prefill",		'p',	arg_uint8,	opt, prefill,
	    "Value to prefill payload with"),
	ARG("rsvd",		'R',	arg_uint16,	opt, rsvd,
	    "Reserved field value"),
	ARG("timeout",		't',	arg_uint32,	opt, timeout,
	    "Command timeout (ms)"),
	ARG("raw-binary",	'b',	arg_none,	opt, binary,
	    "Output in binary format"),
	ARG("dry-run",		'd',	arg_none,	opt, dry_run,
	    "Don't actually execute the command"),
	ARG("read",		'r',	arg_none,	opt, read,
	    "Command reads data from device"),
	ARG("show-command",	's',	arg_none,	opt, show_command,
	    "Show all the command values on stdout"),
	ARG("write",		'w',	arg_none,	opt, write,
	    "Command writes data to device"),
	{ NULL, 0, arg_none, NULL, NULL }
};

static const struct args args[] = {
	{ arg_string, &opt.dev, "controller-id|namespace-id" },
	{ arg_none, NULL, NULL },
};

static void
passthru(const struct cmd *f, int argc, char *argv[])
{
	int	fd = -1, ifd = -1;
	size_t	bytes_read;
	void	*data = NULL, *metadata = NULL;
	struct nvme_pt_command	pt;

	if (arg_parse(argc, argv, f))
		return;
	open_dev(opt.dev, &fd, 1, 1);

	if (opt.read && opt.write)
		errx(EX_USAGE, "need exactly one of --read or --write");
	if (opt.data_len != 0 && !opt.read && !opt.write)
		errx(EX_USAGE, "need exactly one of --read or --write");
	if (*opt.ifn && (ifd = open(opt.ifn, O_RDONLY)) == -1) {
		warn("open %s", opt.ifn);
		goto cleanup;
	}
#if notyet	/* No support in kernel for this */
	if (opt.metadata_len != 0) {
		if (posix_memalign(&metadata, getpagesize(), opt.metadata_len)) {
			warn("can't allocate %d bytes for metadata", metadata_len);
			goto cleanup;
		}
	}
#else
	if (opt.metadata_len != 0)
		errx(EX_UNAVAILABLE, "metadata not supported on FreeBSD");
#endif
	if (opt.data_len) {
		if (posix_memalign(&data, getpagesize(), opt.data_len)) {
			warn("can't allocate %d bytes for data", opt.data_len);
			goto cleanup;
		}
		memset(data, opt.prefill, opt.data_len);
		if (opt.write &&
		    (bytes_read = read(ifd, data, opt.data_len)) !=
		    opt.data_len) {
			warn("read %s; expected %u bytes; got %zd",
			     *opt.ifn ? opt.ifn : "stdin",
			     opt.data_len, bytes_read);
			goto cleanup;
		}
	}
	if (opt.show_command) {
		fprintf(stderr, "opcode       : %#02x\n", opt.opcode);
		fprintf(stderr, "flags        : %#02x\n", opt.flags);
		fprintf(stderr, "rsvd1        : %#04x\n", opt.rsvd);
		fprintf(stderr, "nsid         : %#04x\n", opt.nsid);
		fprintf(stderr, "cdw2         : %#08x\n", opt.cdw2);
		fprintf(stderr, "cdw3         : %#08x\n", opt.cdw3);
		fprintf(stderr, "data_len     : %#08x\n", opt.data_len);
		fprintf(stderr, "metadata_len : %#08x\n", opt.metadata_len);
		fprintf(stderr, "data         : %p\n", data);
		fprintf(stderr, "metadata     : %p\n", metadata);
		fprintf(stderr, "cdw10        : %#08x\n", opt.cdw10);
		fprintf(stderr, "cdw11        : %#08x\n", opt.cdw11);
		fprintf(stderr, "cdw12        : %#08x\n", opt.cdw12);
		fprintf(stderr, "cdw13        : %#08x\n", opt.cdw13);
		fprintf(stderr, "cdw14        : %#08x\n", opt.cdw14);
		fprintf(stderr, "cdw15        : %#08x\n", opt.cdw15);
		fprintf(stderr, "timeout_ms   : %d\n", opt.timeout);
	}
	if (opt.dry_run) {
		errno = 0;
		warn("Doing a dry-run, no actual I/O");
		goto cleanup;
	}

	memset(&pt, 0, sizeof(pt));
	pt.cmd.opc = opt.opcode;
	pt.cmd.fuse = opt.flags;
	pt.cmd.cid = htole16(opt.rsvd);
	pt.cmd.nsid = opt.nsid;				/* XXX note: kernel overrides this */
	pt.cmd.rsvd2 = htole32(opt.cdw2);
	pt.cmd.rsvd3 = htole32(opt.cdw3);
	pt.cmd.cdw10 = htole32(opt.cdw10);
	pt.cmd.cdw11 = htole32(opt.cdw11);
	pt.cmd.cdw12 = htole32(opt.cdw12);
	pt.cmd.cdw13 = htole32(opt.cdw13);
	pt.cmd.cdw14 = htole32(opt.cdw14);
	pt.cmd.cdw15 = htole32(opt.cdw15);
	pt.buf = data;
	pt.len = opt.data_len;
	pt.is_read = opt.read;

	errno = 0;
	if (ioctl(fd, NVME_PASSTHROUGH_CMD, &pt) < 0)
		err(EX_IOERR, "passthrough request failed");
	/* XXX report status */
	if (opt.read) {
		if (opt.binary)
			write(STDOUT_FILENO, data, opt.data_len);
		else {
			/* print status here */
			print_hex(data, opt.data_len);
		}
	}
cleanup:
	free(data);
	close(fd);
	if (ifd > -1)
		close(ifd);
	if (errno)
		exit(EX_IOERR);
}

static void
admin_passthru(const struct cmd *nf, int argc, char *argv[])
{

	passthru(nf, argc, argv);
}

static void
io_passthru(const struct cmd *nf, int argc, char *argv[])
{

	passthru(nf, argc, argv);
}

static struct cmd admin_pass_cmd = {
	.name = "admin-passthru",
	.fn = admin_passthru,
	.ctx_size = sizeof(struct options),
	.opts = opts,
	.args = args,
	.descr = "Send a pass through Admin command to the specified device",
};

static struct cmd io_pass_cmd = {
	.name = "io-passthru",
	.fn = io_passthru,
	.ctx_size = sizeof(struct options),
	.opts = opts,
	.args = args,
	.descr = "Send a pass through I/O command to the specified device",
};

CMD_COMMAND(admin_pass_cmd);
CMD_COMMAND(io_pass_cmd);
