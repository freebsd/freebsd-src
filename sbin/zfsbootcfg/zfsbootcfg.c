/*-
 * Copyright (c) 2016 Andriy Gapon <avg@FreeBSD.org>
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
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <errno.h>
#include <limits.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <kenv.h>

#include <libzfs.h>

/* Keep in sync with zfsboot.c. */
#define MAX_COMMAND_LEN	512

int
install_bootonce(libzfs_handle_t *hdl, uint64_t pool_guid, nvlist_t *nv,
    const char * const data)
{
	nvlist_t **child;
	uint_t children = 0;
	uint64_t guid;
	int rv;

	(void) nvlist_lookup_nvlist_array(nv, ZPOOL_CONFIG_CHILDREN, &child,
	    &children);

	for (int c = 0; c < children; c++) {
		rv = install_bootonce(hdl, pool_guid, child[c], data);
	}

	if (children > 0)
		return (rv);

	if (nvlist_lookup_uint64(nv, ZPOOL_CONFIG_GUID, &guid) != 0) {
		perror("can't get vdev guid");
		return (1);
	}
	if (zpool_nextboot(hdl, pool_guid, guid, data) != 0) {
		perror("ZFS_IOC_NEXTBOOT failed");
		return (1);
	}
	return (0);
}

int main(int argc, const char * const *argv)
{
	char buf[32], *name;
	libzfs_handle_t *hdl;
	zpool_handle_t *zphdl;
	uint64_t pool_guid;
	nvlist_t *nv, *config;
	int rv;
	int len;

	if (argc != 2) {
		fprintf(stderr, "usage: zfsbootcfg <boot.config(5) options>\n");
		return (1);
	}

	len = strlen(argv[1]);
	if (len >= MAX_COMMAND_LEN) {
		fprintf(stderr, "options string is too long\n");
		return (1);
	}

	if (kenv(KENV_GET, "vfs.root.mountfrom", buf, sizeof(buf)) <= 0) {
		perror("can't get vfs.root.mountfrom");
		return (1);
	}

	if (strncmp(buf, "zfs:", 4) == 0) {
		name = strchr(buf + 4, '/');
		if (name != NULL)
			*name = '\0';
		name = buf + 4;
	} else {
		perror("not a zfs root");
		return (1);
	}
		
	if ((hdl = libzfs_init()) == NULL) {
		(void) fprintf(stderr, "internal error: failed to "
		    "initialize ZFS library\n");
		return (1);
	}

	zphdl = zpool_open(hdl, name);
	if (zphdl == NULL) {
		perror("can't open pool");
		libzfs_fini(hdl);
		return (1);
	}

	pool_guid = zpool_get_prop_int(zphdl, ZPOOL_PROP_GUID, NULL);

	config = zpool_get_config(zphdl, NULL);
	if (config == NULL) {
		perror("can't get pool config");
		zpool_close(zphdl);
		libzfs_fini(hdl);
		return (1);
	}

	if (nvlist_lookup_nvlist(config, ZPOOL_CONFIG_VDEV_TREE, &nv) != 0) {
		perror("failed to get vdev tree");
		zpool_close(zphdl);
		libzfs_fini(hdl);
		return (1);
	}

	rv = install_bootonce(hdl, pool_guid, nv, argv[1]);

	zpool_close(zphdl);
	libzfs_fini(hdl);
	if (rv == 0)
		printf("zfs next boot options are successfully written\n");
	return (rv);
}
