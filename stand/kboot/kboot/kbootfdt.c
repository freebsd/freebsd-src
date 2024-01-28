/*-
 * Copyright (C) 2014 Nathan Whitehorn
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <fdt_platform.h>
#include <libfdt.h>
#include "bootstrap.h"
#include "host_syscall.h"
#include "kboot.h"

static void
add_node_to_fdt(void *buffer, const char *path, int fdt_offset)
{
        int child_offset, fd, pfd, error, dentsize;
	char subpath[512];
	void *propbuf;
	ssize_t proplen;

	char dents[2048];
	struct host_dirent64 *dent;
	int d_type;

	fd = host_open(path, O_RDONLY, 0);
	while (1) {
		dentsize = host_getdents64(fd, dents, sizeof(dents));
		if (dentsize <= 0)
			break;
		for (dent = (struct host_dirent64 *)dents;
		     (char *)dent < dents + dentsize;
		     dent = (struct host_dirent64 *)((void *)dent + dent->d_reclen)) {
			sprintf(subpath, "%s/%s", path, dent->d_name);
			if (strcmp(dent->d_name, ".") == 0 ||
			    strcmp(dent->d_name, "..") == 0)
				continue;
			d_type = dent->d_type;
			if (d_type == HOST_DT_DIR) {
				child_offset = fdt_add_subnode(buffer, fdt_offset,
				    dent->d_name);
				if (child_offset < 0) {
					printf("Error %d adding node %s/%s, skipping\n",
					    child_offset, path, dent->d_name);
					continue;
				}
				add_node_to_fdt(buffer, subpath, child_offset);
			} else {
				propbuf = malloc(1024);
				proplen = 0;
				pfd = host_open(subpath, O_RDONLY, 0);
				if (pfd > 0) {
					proplen = host_read(pfd, propbuf, 1024);
					host_close(pfd);
				}
				error = fdt_setprop(buffer, fdt_offset, dent->d_name,
				    propbuf, proplen);
				free(propbuf);
				if (error)
					printf("Error %d adding property %s to "
					    "node %d\n", error, dent->d_name,
					    fdt_offset);
			}
		}
	}

	host_close(fd);
}

int
fdt_platform_load_dtb(void)
{
	void *buffer;
	size_t buflen = 409600;
	int fd;

	/*
	 * Should load /sys/firmware/fdt if it exists, otherwise we walk the
	 * tree from /proc/device-tree. The former is much easier than the
	 * latter and also the newer interface. But as long as we support the
	 * PS3 boot, we'll need the latter due to that kernel's age. It likely
	 * would be better to script the decision between the two, but that
	 * turns out to be tricky...
	 */
	buffer = malloc(buflen);
	fd = host_open("/sys/firmware/fdt", O_RDONLY, 0);
	if (fd != -1) {
		buflen = host_read(fd, buffer, buflen);
		close(fd);
	} else {
		fdt_create_empty_tree(buffer, buflen);
		add_node_to_fdt(buffer, "/proc/device-tree",
		    fdt_path_offset(buffer, "/"));
	}
	fdt_arch_fixups(buffer);

	fdt_pack(buffer);

	fdt_load_dtb_addr(buffer);
	free(buffer);
	
	return (0);
}

void
fdt_platform_load_overlays(void)
{
	fdt_load_dtb_overlays(NULL);
}

void
fdt_platform_fixups(void)
{
	fdt_apply_overlays();
}
