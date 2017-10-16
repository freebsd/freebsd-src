/*-
 * Copyright (c) 1998 Robert Nordier
 * All rights reserved.
 * Copyright (c) 2001 Robert Drehmel
 * All rights reserved.
 * Copyright (c) 2014 Nathan Whitehorn
 * All rights reserved.
 * Copyright (c) 2015 Eric McCorkle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are freely
 * permitted provided that the above copyright notice and this
 * paragraph and the following disclaimer are duplicated in all
 * such forms.
 *
 * This software is provided "AS IS" and without any express or
 * implied warranties, including, without limitation, the implied
 * warranties of merchantability and fitness for a particular
 * purpose.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/disk.h>
#include <machine/elf.h>
#include <machine/stdarg.h>
#include <stand.h>
#include <disk.h>

#include <efi.h>
#include <efilib.h>
#include <efiprot.h>
#include <eficonsctl.h>
#ifdef EFI_ZFS_BOOT
#include <libzfs.h>
#endif
typedef CHAR16 efi_char;
#include <efichar.h>

#include <bootstrap.h>

#include "efi_drivers.h"
#include "efizfs.h"
#include "paths.h"

static void efi_panic(EFI_STATUS s, const char *fmt, ...) __dead2 __printflike(2, 3);
#ifdef EFI_DEBUG
#define DPRINTF(fmt, args...) printf(fmt, ##args)
#define DSTALL(d) BS->Stall(d)
#else
#define DPRINTF(fmt, ...) {}
#define DSTALL(d) {}
#endif

struct arch_switch archsw;	/* MI/MD interface boundary */

static const efi_driver_t *efi_drivers[] = {
        NULL
};

extern struct console efi_console;
#if defined(__amd64__) || defined(__i386__)
extern struct console comconsole;
extern struct console nullconsole;
#endif

#ifdef EFI_ZFS_BOOT
uint64_t pool_guid;
#endif

struct fs_ops *file_system[] = {
#ifdef EFI_ZFS_BOOT
	&zfs_fsops,
#endif
	&dosfs_fsops,
#ifdef EFI_UFS_BOOT
	&ufs_fsops,
#endif
	&cd9660_fsops,
	&nfs_fsops,
	&gzipfs_fsops,
	&bzipfs_fsops,
	NULL
};

struct devsw *devsw[] = {
	&efipart_hddev,
	&efipart_fddev,
	&efipart_cddev,
#ifdef EFI_ZFS_BOOT
	&zfs_dev,
#endif
	NULL
};

struct console *consoles[] = {
	&efi_console,
	NULL
};

static EFI_LOADED_IMAGE *boot_image;
static EFI_DEVICE_PATH *imgpath;
static EFI_DEVICE_PATH *imgprefix;

/* Definitions we don't actually need for boot, but we need to define
 * to make the linker happy.
 */
struct file_format *file_formats[] = { NULL };

struct netif_driver *netif_drivers[] = { NULL };

static int
efi_autoload(void)
{
  printf("******** Boot block should not call autoload\n");
  return (-1);
}

static ssize_t
efi_copyin(const void *src __unused, vm_offset_t dest __unused,
    const size_t len __unused)
{
  printf("******** Boot block should not call copyin\n");
  return (-1);
}

static ssize_t
efi_copyout(vm_offset_t src __unused, void *dest __unused,
    const size_t len __unused)
{
  printf("******** Boot block should not call copyout\n");
  return (-1);
}

static ssize_t
efi_readin(int fd __unused, vm_offset_t dest __unused,
    const size_t len __unused)
{
  printf("******** Boot block should not call readin\n");
  return (-1);
}

/* The initial number of handles used to query EFI for partitions. */
#define NUM_HANDLES_INIT	24

static EFI_GUID DevicePathGUID = DEVICE_PATH_PROTOCOL;
static EFI_GUID LoadedImageGUID = LOADED_IMAGE_PROTOCOL;
static EFI_GUID FreeBSDBootVarGUID = FREEBSD_BOOT_VAR_GUID;

static EFI_STATUS
do_load(const char *filepath, void **bufp, size_t *bufsize)
{
	struct stat st;
        void *buf = NULL;
        int fd, err;
        size_t fsize, remaining;
        ssize_t readsize;

        if ((fd = open(filepath, O_RDONLY)) < 0) {
                return (ENOTSUP);
        }

        if ((err = fstat(fd, &st)) != 0) {
                goto close_file;
        }

        fsize = st.st_size;

        if ((buf = malloc(fsize)) == NULL) {
                err = ENOMEM;
                goto close_file;
        }

        remaining = fsize;

        do {
                if ((readsize = read(fd, buf, fsize)) < 0) {
                        err = (-readsize);
                        goto free_buf;
                }

                remaining -= readsize;
        } while(remaining != 0);

        close(fd);
        *bufsize = st.st_size;
        *bufp = buf;

 close_file:
        close(fd);

        return errno_to_efi_status(err);

 free_buf:
        free(buf);
        goto close_file;
}

static EFI_STATUS
efi_setenv_freebsd_wcs(const char *varname, CHAR16 *valstr)
{
	CHAR16 *var = NULL;
	size_t len;
	EFI_STATUS rv;

	utf8_to_ucs2(varname, &var, &len);
	if (var == NULL)
		return (EFI_OUT_OF_RESOURCES);
	rv = RS->SetVariable(var, &FreeBSDBootVarGUID,
	    EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
	    (ucs2len(valstr) + 1) * sizeof(efi_char), valstr);
	free(var);
	return (rv);
}

static int
probe_fs(const char *filepath)
{
        int fd;

        if ((fd = open(filepath, O_RDONLY)) < 0) {
                return (ENOTSUP);
        }

        close(fd);

        return (0);
}

static int
probe_dev(struct devsw *dev, int unit, const char *filepath)
{
        struct devdesc currdev;
        char *devname;
        int err;

	currdev.d_dev = dev;
	currdev.d_type = currdev.d_dev->dv_type;
	currdev.d_unit = unit;
	currdev.d_opendata = NULL;
        devname = efi_fmtdev(&currdev);

        env_setenv("currdev", EV_VOLATILE, devname, efi_setcurrdev,
            env_nounset);

        err = probe_fs(filepath);

        return (err);
}

static bool
check_preferred(EFI_HANDLE *h)
{
         EFI_DEVICE_PATH *path = efi_lookup_devpath(h);
         bool out;

         if ((path = efi_lookup_devpath(h)) == NULL)
                 return (false);

         out = efi_devpath_is_prefix(imgpath, path) ||
           efi_devpath_is_prefix(imgprefix, path);

         return (out);
}

bool
efi_zfs_is_preferred(EFI_HANDLE *h)
{
         return (check_preferred(h));
}

static int
load_preferred(EFI_LOADED_IMAGE *img, const char *filepath, void **bufp,
    size_t *bufsize, EFI_HANDLE *handlep)
{
	pdinfo_list_t *pdi_list;
	pdinfo_t *dp, *pp;
	char *devname;

#ifdef EFI_ZFS_BOOT
	/* Did efi_zfs_probe() detect the boot pool? */
	if (pool_guid != 0) {
                struct zfs_devdesc currdev;

		currdev.d_dev = &zfs_dev;
		currdev.d_unit = 0;
		currdev.d_type = currdev.d_dev->dv_type;
		currdev.d_opendata = NULL;
		currdev.pool_guid = pool_guid;
		currdev.root_guid = 0;
		devname = efi_fmtdev(&currdev);

		env_setenv("currdev", EV_VOLATILE, devname, efi_setcurrdev,
		    env_nounset);

                if (probe_fs(filepath) == 0 &&
                    do_load(filepath, bufp, bufsize) == EFI_SUCCESS) {
                        *handlep = efizfs_get_handle_by_guid(pool_guid);
                        return (0);
                }
	}
#endif /* EFI_ZFS_BOOT */

	/* We have device lists for hd, cd, fd, walk them all. */
	pdi_list = efiblk_get_pdinfo_list(&efipart_hddev);
	STAILQ_FOREACH(dp, pdi_list, pd_link) {
                struct disk_devdesc currdev;

		currdev.d_dev = &efipart_hddev;
		currdev.d_type = currdev.d_dev->dv_type;
		currdev.d_unit = dp->pd_unit;
		currdev.d_opendata = NULL;
		currdev.d_slice = -1;
		currdev.d_partition = -1;
		devname = efi_fmtdev(&currdev);

		env_setenv("currdev", EV_VOLATILE, devname, efi_setcurrdev,
		    env_nounset);

	        if (check_preferred(dp->pd_handle) &&
                    probe_fs(filepath) == 0 &&
                    do_load(filepath, bufp, bufsize) == EFI_SUCCESS) {
                        *handlep = dp->pd_handle;
                        return (0);
		}

                /* Assuming GPT partitioning. */
		STAILQ_FOREACH(pp, &dp->pd_part, pd_link) {
                        if (check_preferred(pp->pd_handle)) {
				currdev.d_slice = pp->pd_unit;
				currdev.d_partition = 255;
                                devname = efi_fmtdev(&currdev);

                                env_setenv("currdev", EV_VOLATILE, devname,
                                    efi_setcurrdev, env_nounset);

                                if (probe_fs(filepath) == 0 &&
                                    do_load(filepath, bufp, bufsize) ==
                                        EFI_SUCCESS) {
                                        *handlep = pp->pd_handle;
                                        return (0);
                                }
			}
		}
	}

	pdi_list = efiblk_get_pdinfo_list(&efipart_cddev);
	STAILQ_FOREACH(dp, pdi_list, pd_link) {
                if ((dp->pd_handle == img->DeviceHandle ||
		     dp->pd_alias == img->DeviceHandle ||
                     check_preferred(dp->pd_handle)) &&
                    probe_dev(&efipart_cddev, dp->pd_unit, filepath) == 0 &&
                    do_load(filepath, bufp, bufsize) == EFI_SUCCESS) {
                        *handlep = dp->pd_handle;
                        return (0);
		}
	}

	pdi_list = efiblk_get_pdinfo_list(&efipart_fddev);
	STAILQ_FOREACH(dp, pdi_list, pd_link) {
                if ((dp->pd_handle == img->DeviceHandle ||
                     check_preferred(dp->pd_handle)) &&
                    probe_dev(&efipart_cddev, dp->pd_unit, filepath) == 0 &&
                    do_load(filepath, bufp, bufsize) == EFI_SUCCESS) {
                        *handlep = dp->pd_handle;
                        return (0);
		}
	}

	return (ENOENT);
}

static int
load_all(const char *filepath, void **bufp, size_t *bufsize,
    EFI_HANDLE *handlep)
{
	pdinfo_list_t *pdi_list;
	pdinfo_t *dp, *pp;
	zfsinfo_list_t *zfsi_list;
	zfsinfo_t *zi;
        char *devname;

#ifdef EFI_ZFS_BOOT
	zfsi_list = efizfs_get_zfsinfo_list();
	STAILQ_FOREACH(zi, zfsi_list, zi_link) {
                struct zfs_devdesc currdev;

		currdev.d_dev = &zfs_dev;
		currdev.d_unit = 0;
		currdev.d_type = currdev.d_dev->dv_type;
		currdev.d_opendata = NULL;
		currdev.pool_guid = zi->zi_pool_guid;
		currdev.root_guid = 0;
		devname = efi_fmtdev(&currdev);

		env_setenv("currdev", EV_VOLATILE, devname, efi_setcurrdev,
		    env_nounset);

                if (probe_fs(filepath) == 0 &&
                    do_load(filepath, bufp, bufsize) == EFI_SUCCESS) {
                        *handlep = zi->zi_handle;

                        return (0);
                }
	}
#endif /* EFI_ZFS_BOOT */

	/* We have device lists for hd, cd, fd, walk them all. */
	pdi_list = efiblk_get_pdinfo_list(&efipart_hddev);
	STAILQ_FOREACH(dp, pdi_list, pd_link) {
                struct disk_devdesc currdev;

		currdev.d_dev = &efipart_hddev;
		currdev.d_type = currdev.d_dev->dv_type;
		currdev.d_unit = dp->pd_unit;
		currdev.d_opendata = NULL;
		currdev.d_slice = -1;
		currdev.d_partition = -1;
		devname = efi_fmtdev(&currdev);

		env_setenv("currdev", EV_VOLATILE, devname, efi_setcurrdev,
		    env_nounset);

		if (probe_fs(filepath) == 0 &&
                    do_load(filepath, bufp, bufsize) == EFI_SUCCESS) {
                        *handlep = dp->pd_handle;

                        return (0);
		}

                /* Assuming GPT partitioning. */
		STAILQ_FOREACH(pp, &dp->pd_part, pd_link) {
                        currdev.d_slice = pp->pd_unit;
                        currdev.d_partition = 255;
                        devname = efi_fmtdev(&currdev);

                        env_setenv("currdev", EV_VOLATILE, devname,
                            efi_setcurrdev, env_nounset);

                        if (probe_fs(filepath) == 0 &&
                            do_load(filepath, bufp, bufsize) == EFI_SUCCESS) {
                                *handlep = pp->pd_handle;

                                return (0);
			}
		}
	}

	pdi_list = efiblk_get_pdinfo_list(&efipart_cddev);
	STAILQ_FOREACH(dp, pdi_list, pd_link) {
		if (probe_dev(&efipart_cddev, dp->pd_unit, filepath) == 0 &&
                    do_load(filepath, bufp, bufsize) == EFI_SUCCESS) {
                        *handlep = dp->pd_handle;

                        return (0);
		}
	}

	pdi_list = efiblk_get_pdinfo_list(&efipart_fddev);
	STAILQ_FOREACH(dp, pdi_list, pd_link) {
		if (probe_dev(&efipart_fddev, dp->pd_unit, filepath) == 0 &&
                    do_load(filepath, bufp, bufsize) == EFI_SUCCESS) {
                        *handlep = dp->pd_handle;

                        return (0);
		}
	}

	return (ENOENT);
}

static EFI_STATUS
load_loader(EFI_HANDLE *handlep, void **bufp, size_t *bufsize)
{
        /* Try the preferred handles first, then all the handles */
        if (load_preferred(boot_image, PATH_LOADER_EFI, bufp, bufsize,
                handlep) == 0) {
                return (0);
        }

        if (load_all(PATH_LOADER_EFI, bufp, bufsize, handlep) == 0) {
                return (0);
        }

	return (EFI_NOT_FOUND);
}

/*
 * try_boot only returns if it fails to load the loader. If it succeeds
 * it simply boots, otherwise it returns the status of last EFI call.
 */
static EFI_STATUS
try_boot(void)
{
	size_t bufsize, loadersize, cmdsize;
	void *buf, *loaderbuf;
	char *cmd;
        EFI_HANDLE fshandle;
	EFI_HANDLE loaderhandle;
	EFI_LOADED_IMAGE *loaded_image;
	EFI_STATUS status;
        EFI_DEVICE_PATH *fspath;

	status = load_loader(&fshandle, &loaderbuf, &loadersize);

        if (status != EFI_SUCCESS) {
                return (status);
        }

	fspath = NULL;
	if (status == EFI_SUCCESS) {
		status = BS->OpenProtocol(fshandle, &DevicePathGUID,
                    (void **)&fspath, IH, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
		if (status != EFI_SUCCESS) {
			DPRINTF("Failed to get image DevicePath (%lu)\n",
			    EFI_ERROR_CODE(status));
                }
		DPRINTF("filesystem device path: %s\n", devpath_str(fspath));
	}

	/*
	 * Read in and parse the command line from /boot.config or /boot/config,
	 * if present. We'll pass it the next stage via a simple ASCII
	 * string. loader.efi has a hack for ASCII strings, so we'll use that to
	 * keep the size down here. We only try to read the alternate file if
	 * we get EFI_NOT_FOUND because all other errors mean that the boot_module
	 * had troubles with the filesystem. We could return early, but we'll let
	 * loading the actual kernel sort all that out. Since these files are
	 * optional, we don't report errors in trying to read them.
	 */
	cmd = NULL;
	cmdsize = 0;
	status = do_load(PATH_DOTCONFIG, &buf, &bufsize);
	if (status == EFI_NOT_FOUND)
		status = do_load(PATH_CONFIG, &buf, &bufsize);
	if (status == EFI_SUCCESS) {
		cmdsize = bufsize + 1;
		cmd = malloc(cmdsize);
		if (cmd == NULL)
			goto errout;
		memcpy(cmd, buf, bufsize);
		cmd[bufsize] = '\0';
		free(buf);
		buf = NULL;
	}

	if ((status = BS->LoadImage(TRUE, IH, efi_devpath_last_node(fspath),
	    loaderbuf, loadersize, &loaderhandle)) != EFI_SUCCESS) {
		printf("Failed to load image, size: %zu, (%lu)\n",
		     loadersize, EFI_ERROR_CODE(status));
		goto errout;
	}

	if ((status = BS->OpenProtocol(loaderhandle, &LoadedImageGUID,
            (VOID**)&loaded_image, IH, NULL,
            EFI_OPEN_PROTOCOL_GET_PROTOCOL)) != EFI_SUCCESS) {
		printf("Failed to query LoadedImage (%lu)\n",
		    EFI_ERROR_CODE(status));
		goto errout;
	}

	if (cmd != NULL)
		printf("    command args: %s\n", cmd);

	loaded_image->DeviceHandle = fshandle;
	loaded_image->LoadOptionsSize = cmdsize;
	loaded_image->LoadOptions = cmd;

	DPRINTF("Starting '%s' in 5 seconds...", PATH_LOADER_EFI);
	DSTALL(1000000);
	DPRINTF(".");
	DSTALL(1000000);
	DPRINTF(".");
	DSTALL(1000000);
	DPRINTF(".");
	DSTALL(1000000);
	DPRINTF(".");
	DSTALL(1000000);
	DPRINTF(".\n");

	if ((status = BS->StartImage(loaderhandle, NULL, NULL)) !=
	    EFI_SUCCESS) {
		printf("Failed to start image (%lu)\n",
		    EFI_ERROR_CODE(status));
		loaded_image->LoadOptionsSize = 0;
		loaded_image->LoadOptions = NULL;
	}

errout:
	if (cmd != NULL)
		free(cmd);
	if (buf != NULL)
		free(buf);
	if (loaderbuf != NULL)
		free(loaderbuf);

	return (status);
}

EFI_STATUS
main(int argc __unused, CHAR16 *argv[] __unused)
{
        EFI_STATUS status;

        SIMPLE_TEXT_OUTPUT_INTERFACE *conout = NULL;
        UINTN i, max_dim, best_mode, cols, rows;
        CHAR16 *text;

        archsw.arch_autoload = efi_autoload;
	archsw.arch_getdev = efi_getdev;
	archsw.arch_copyin = efi_copyin;
	archsw.arch_copyout = efi_copyout;
	archsw.arch_readin = efi_readin;
#ifdef EFI_ZFS_BOOT
        /* Note this needs to be set before ZFS init. */
        archsw.arch_zfs_probe = efi_zfs_probe;
#endif

	/* Init the time source */
	efi_time_init();
        cons_probe();

	/*
	 * Reset the console and find the best text mode.
	 */
	conout = ST->ConOut;
	conout->Reset(conout, TRUE);
	max_dim = best_mode = 0;

        for (i = 0; ; i++) {
		status = conout->QueryMode(conout, i, &cols, &rows);
		if (EFI_ERROR(status))
			break;
		if (cols * rows > max_dim) {
			max_dim = cols * rows;
			best_mode = i;
		}
	}

        if (max_dim > 0)
		conout->SetMode(conout, best_mode);

	conout->EnableCursor(conout, TRUE);
	conout->ClearScreen(conout);

        /* Print this here, so people know it's at least starting. */
	printf("\n>> FreeBSD EFI boot block\n");
	printf("   Loader path: %s\n\n", PATH_LOADER_EFI);

        /* Get the image path and trim it to get the disk on which we
         * found this loader.
         */
	if ((status = BS->OpenProtocol(IH, &LoadedImageGUID,
            (VOID**)&boot_image, IH, NULL,
            EFI_OPEN_PROTOCOL_GET_PROTOCOL)) != EFI_SUCCESS) {
		panic("Failed to query LoadedImage (%lu)\n",
		    EFI_ERROR_CODE(status));
	}

	/* Determine the devpath of our image so we can prefer it. */
	status = BS->HandleProtocol(IH, &LoadedImageGUID, (VOID**)&boot_image);
	imgpath = NULL;
	if (status == EFI_SUCCESS) {
		text = efi_devpath_name(boot_image->FilePath);
		printf("   Load Path: %S\n", text);
		efi_setenv_freebsd_wcs("Boot1Path", text);
		efi_free_devpath_name(text);

		status = BS->HandleProtocol(boot_image->DeviceHandle, &DevicePathGUID,
		    (void **)&imgpath);
		if (status != EFI_SUCCESS) {
			DPRINTF("Failed to get image DevicePath (%lu)\n",
			    EFI_ERROR_CODE(status));
		} else {
			text = efi_devpath_name(imgpath);
			printf("   Load Device: %S\n", text);
			efi_setenv_freebsd_wcs("Boot1Dev", text);
			efi_free_devpath_name(text);
		}

	}

        /* The loaded image device path ends with a partition, then a
         * file path.  Trim them both to get the actual disk.
         */
        if ((imgprefix = efi_devpath_trim(imgpath)) == NULL ||
            (imgprefix = efi_devpath_trim(imgprefix)) == NULL) {
                panic("Couldn't trim device path");
        }

	/*
	 * Initialize the block cache. Set the upper limit.
	 */
	bcache_init(32768, 512);

	printf("\n   Initializing modules:");

	for (i = 0; efi_drivers[i] != NULL; i++) {
		printf(" %s", efi_drivers[i]->name);
		if (efi_drivers[i]->init != NULL)
			efi_drivers[i]->init();
	}

	for (i = 0; devsw[i] != NULL; i++) {
                if (devsw[i]->dv_init != NULL) {
                        printf(" %s", devsw[i]->dv_name);
			(devsw[i]->dv_init)();
                }
        }

	putchar('\n');

	try_boot();

	/* If we get here, we're out of luck... */
	efi_panic(EFI_LOAD_ERROR, "No bootable partitions found!");
}

/*
 * OK. We totally give up. Exit back to EFI with a sensible status so
 * it can try the next option on the list.
 */
static void
efi_panic(EFI_STATUS s, const char *fmt, ...)
{
       va_list ap;

       printf("panic: ");
       va_start(ap, fmt);
       vprintf(fmt, ap);
       va_end(ap);
       printf("\n");

       BS->Exit(IH, s, 0, NULL);
}
