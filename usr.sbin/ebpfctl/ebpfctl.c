/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Ryan Stone
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

#include <gbpf.h>

#include <sys/param.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
usage(void)
{
	fprintf(stderr, "usage: ebpfctl probe list\n");

	exit(1);
}

static void
probe_list_command(GBPFDriver *ebpf, int argc, char **argv __unused)
{
	struct ebpf_probe_info info;
	ebpf_probe_id_t prev_id;
	int error;

	if (argc != 0) {
		fprintf(stderr, "probe list command does not accept arguments\n");
		usage();
	}

	printf("%5s %5s NAME\n", "ID", "PROGS");
	prev_id = EBPF_PROBE_FIRST;
	while (1) {
		error = gbpf_get_probe_info(ebpf, prev_id, &info);
		if (error != 0) {
			err(1, "failed to get info for probe #%d", prev_id);
		}

		if (info.id == EBPF_PROBE_FIRST) {
			break;
		}

		printf("%5d %5d %s:%s:%s:%s:%s\n", info.id, info.num_attached,
		    info.name.tracer, info.name.provider, info.name.module,
		    info.name.function, info.name.name);

		prev_id = info.id;
	}

	exit(0);
}

static void
probe_command(GBPFDriver *ebpf, int argc, char **argv)
{
	if (argc == 0) {
		fprintf(stderr, "probe command requires subcommand\n");
		usage();
	}

	if (strcmp(argv[0], "list") == 0) {
		probe_list_command(ebpf, argc - 1, argv + 1);
	} else {
		fprintf(stderr, "unrecognized subcommand '%s'\n", argv[0]);
		usage();
	}
}

int main(int argc, char **argv)
{
	EBPFDevDriver *ebpf = ebpf_dev_driver_create();

	if (ebpf == NULL) {
		err(1, "Failed to driver ebpf dev driver");
	}

	if (argc < 2) {
		usage();
	}

	if (strcmp(argv[1], "probe") == 0) {
		probe_command(&ebpf->base, argc - 2, argv + 2);
	} else {
		fprintf(stderr, "unrecognized command '%s'\n", argv[1]);
		usage();
	}
}
