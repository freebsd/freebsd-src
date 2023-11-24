/*-
 * Copyright (c) 2015 Eric McCorkle
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
#include <stddef.h>
#include <stdarg.h>
#include <stdbool.h>

#include <sys/param.h>
#include <sys/queue.h>
#include <efi.h>

#include "boot_module.h"

#include "libzfs.h"
#include "zfsimpl.c"

static dev_info_t *devices;

static char zfs_bootonce[VDEV_PAD_SIZE];

uint64_t
ldi_get_size(void *priv)
{
	dev_info_t *devinfo = priv;

	return (devinfo->dev->Media->BlockSize *
	    (devinfo->dev->Media->LastBlock + 1));
}

static int
vdev_read(vdev_t *vdev, void *priv, off_t off, void *buf, size_t bytes)
{
	dev_info_t *devinfo;
	uint64_t lba;
	size_t size, remainder, rb_size, blksz;
	char *bouncebuf = NULL, *rb_buf;
	EFI_STATUS status;

	devinfo = (dev_info_t *)priv;
	lba = off / devinfo->dev->Media->BlockSize;
	remainder = off % devinfo->dev->Media->BlockSize;

	rb_buf = buf;
	rb_size = bytes;

	/*
	 * If we have remainder from off, we need to add remainder part.
	 * Since buffer must be multiple of the BlockSize, round it all up.
	 */
	size = roundup2(bytes + remainder, devinfo->dev->Media->BlockSize);
	blksz = size;
	if (remainder != 0 || size != bytes) {
		rb_size = devinfo->dev->Media->BlockSize;
		bouncebuf = malloc(rb_size);
		if (bouncebuf == NULL) {
			printf("vdev_read: out of memory\n");
			return (-1);
		}
		rb_buf = bouncebuf;
		blksz = rb_size - remainder;
	}

	while (bytes > 0) {
		status = devinfo->dev->ReadBlocks(devinfo->dev,
		    devinfo->dev->Media->MediaId, lba, rb_size, rb_buf);
		if (EFI_ERROR(status))
				goto error;
		if (bytes < blksz)
			blksz = bytes;
		if (bouncebuf != NULL)
			memcpy(buf, rb_buf + remainder, blksz);
		buf = (void *)((uintptr_t)buf + blksz);
		bytes -= blksz;
		lba++;
		remainder = 0;
		blksz = rb_size;
	}

	free(bouncebuf);
	return (0);

error:
	free(bouncebuf);
	DPRINTF("vdev_read: failed dev: %p, id: %u, lba: %ju, size: %zu,"
	    " rb_size: %zu, status: %lu\n", devinfo->dev,
	    devinfo->dev->Media->MediaId, (uintmax_t)lba, bytes, rb_size,
	    EFI_ERROR_CODE(status));
	return (-1);
}

static EFI_STATUS
probe(dev_info_t *dev)
{
	spa_t *spa;
	dev_info_t *tdev;

	/* ZFS consumes the dev on success so we need a copy. */
	tdev = malloc(sizeof(*dev));
	if (tdev == NULL) {
		DPRINTF("Failed to allocate tdev\n");
		return (EFI_OUT_OF_RESOURCES);
	}
	memcpy(tdev, dev, sizeof(*dev));

	if (vdev_probe(vdev_read, NULL, tdev, &spa) != 0) {
		free(tdev);
		return (EFI_UNSUPPORTED);
	}

	dev->devdata = spa;
	add_device(&devices, dev);

	return (EFI_SUCCESS);
}

static EFI_STATUS
load(const char *filepath, dev_info_t *devinfo, void **bufp, size_t *bufsize)
{
	spa_t *spa;
	struct zfsmount zmount;
	dnode_phys_t dn;
	struct stat st;
	uint64_t rootobj;
	int err;
	void *buf;

	spa = devinfo->devdata;

#ifdef EFI_DEBUG
	{
		CHAR16 *text = efi_devpath_name(devinfo->devpath);
		DPRINTF("load: '%s' spa: '%s', devpath: %S\n", filepath,
		    spa->spa_name, text);
		efi_free_devpath_name(text);
	}
#endif
	if ((err = zfs_spa_init(spa)) != 0) {
		DPRINTF("Failed to load pool '%s' (%d)\n", spa->spa_name, err);
		return (EFI_NOT_FOUND);
	}

	if (zfs_get_bootonce_spa(spa, OS_BOOTONCE, zfs_bootonce,
	    sizeof(zfs_bootonce)) == 0) {
		/*
		 * If bootonce attribute is present, use it as root dataset.
		 * Any attempt to use it should clear the 'once' flag.  Prior
		 * to now, we'd not be able to clear it anyway.  We don't care
		 * if we can't find the files to boot, or if there's a problem
		 * with it: we've tried to use it once we're able to mount the
		 * ZFS dataset.
		 *
		 * Note: the attribute is prefixed with "zfs:" and suffixed
		 * with ":".
		 */
		char *dname, *end;

		if (zfs_bootonce[0] != 'z' || zfs_bootonce[1] != 'f' ||
		    zfs_bootonce[2] != 's' || zfs_bootonce[3] != ':' ||
		    (dname = strchr(&zfs_bootonce[4], '/')) == NULL ||
		    (end = strrchr(&zfs_bootonce[4], ':')) == NULL) {
			printf("INVALID zfs bootonce: %s\n", zfs_bootonce);
			*zfs_bootonce = '\0';
			rootobj = 0;
		} else {
			dname += 1;
			*end = '\0';
			if (zfs_lookup_dataset(spa, dname, &rootobj) != 0) {
				printf("zfs bootonce dataset %s NOT FOUND\n",
				    dname);
				*zfs_bootonce = '\0';
				rootobj = 0;
			} else
				printf("zfs bootonce: %s\n", zfs_bootonce);
			*end = ':';
		}
	} else {
		*zfs_bootonce = '\0';
		rootobj = 0;
	}

	if ((err = zfs_mount_impl(spa, rootobj, &zmount)) != 0) {
		printf("Failed to mount pool '%s' (%d)\n", spa->spa_name, err);
		return (EFI_NOT_FOUND);
	}

	if ((err = zfs_lookup(&zmount, filepath, &dn)) != 0) {
		if (err == ENOENT) {
			DPRINTF("Failed to find '%s' on pool '%s' (%d)\n",
			    filepath, spa->spa_name, err);
			return (EFI_NOT_FOUND);
		}
		printf("Failed to lookup '%s' on pool '%s' (%d)\n", filepath,
		    spa->spa_name, err);
		return (EFI_INVALID_PARAMETER);
	}

	if ((err = zfs_dnode_stat(spa, &dn, &st)) != 0) {
		printf("Failed to stat '%s' on pool '%s' (%d)\n", filepath,
		    spa->spa_name, err);
		return (EFI_INVALID_PARAMETER);
	}

	buf = malloc(st.st_size);
	if (buf == NULL) {
		printf("Failed to allocate load buffer %jd for pool '%s' for '%s' ",
		    (intmax_t)st.st_size, spa->spa_name, filepath);
		return (EFI_INVALID_PARAMETER);
	}

	if ((err = dnode_read(spa, &dn, 0, buf, st.st_size)) != 0) {
		printf("Failed to read node from %s (%d)\n", spa->spa_name,
		    err);
		free(buf);
		return (EFI_INVALID_PARAMETER);
	}

	*bufsize = st.st_size;
	*bufp = buf;

	return (EFI_SUCCESS);
}

static void
status(void)
{
	spa_t *spa;

	spa = STAILQ_FIRST(&zfs_pools);
	if (spa == NULL) {
		printf("%s found no pools\n", zfs_module.name);
		return;
	}

	printf("%s found the following pools:", zfs_module.name);
	STAILQ_FOREACH(spa, &zfs_pools, spa_link)
		printf(" %s", spa->spa_name);

	printf("\n");
}

static const char *
extra_env(void)
{
	char *rv = NULL;	/* So we return NULL if asprintf fails */

	if (*zfs_bootonce == '\0')
		return NULL;
	asprintf(&rv, "zfs-bootonce=%s", zfs_bootonce);
	return (rv);
}


static void
init(void)
{

	zfs_init();
}

static dev_info_t *
_devices(void)
{

	return (devices);
}

const boot_module_t zfs_module =
{
	.name = "ZFS",
	.init = init,
	.probe = probe,
	.load = load,
	.status = status,
	.devices = _devices,
	.extra_env = extra_env,
};
