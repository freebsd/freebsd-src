/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Ankur Kothiwal
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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <gbpf.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/ebpf.h>
#include <sys/ebpf_param.h>

#include <sys/xdp.h>
#include <sys/ebpf_probe.h>
#include <gbpf_driver.h>

struct my_data {
	int mymap;
	int myprog;
};

/*
 * Callback when the found from the ELF file. 
 * At this moment, the relocation for the maps are already 
 * done. So, what we need is just load the program and 
 * pick the file descriptor.
 */

void
on_prog(GBPFElfWalker *walker, const char *name,
		struct ebpf_inst *prog, uint32_t prog_len)
{
	struct my_data *data = (struct my_data *)walker->data;

	printf("Found program: %s\n", name);
	
	data->myprog = gbpf_load_prog(walker->driver, EBPF_PROG_TYPE_XDP,
					prog, prog_len);
	assert(data->myprog != -1);
}

/*
 * Callback when the map found from the ELF file.
 * At this moment, the maps is already "created".
 * So, what we need to do is pick the file descriptor.
 */
void
on_map(GBPFElfWalker *walker, const char *name, int desc, 
		struct ebpf_map_def *map)
{
	struct my_data *data = (struct my_data *)walker->data;

	printf("Found map: %s\n", name);
	data->mymap = desc;
}

int
main(int argc, char **argv)
{
	int error;
	struct my_data data = {0};
	EBPFDevDriver *devDriver = ebpf_dev_driver_create();
	assert(devDriver != NULL);

	GBPFDriver *driver = &devDriver->base;

	GBPFElfWalker walker = {
		.driver = driver,
		.on_prog = on_prog,
		.on_map = on_map,
		.data = &data
	};

	error = gbpf_walk_elf(&walker, driver, argv[1]);
	assert(error == 0);

	gbpf_attach_probe(driver, data.myprog, "ebpf", "xdp", "", "xdp_rx", "vtnet0", 0);

	ebpf_dev_driver_destroy(devDriver);

	return EXIT_SUCCESS;
}
