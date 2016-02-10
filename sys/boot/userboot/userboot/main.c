/*-
 * Copyright (c) 1998 Michael Smith <msmith@freebsd.org>
 * Copyright (c) 1998,2000 Doug Rabson <dfr@freebsd.org>
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

#include <stand.h>
#include <string.h>
#include <setjmp.h>

#include "bootstrap.h"
#include "disk.h"
#include "libuserboot.h"

#if defined(USERBOOT_ZFS_SUPPORT)
#include "../zfs/libzfs.h"

static void userboot_zfs_probe(void);
static int userboot_zfs_found;
#endif

#define	USERBOOT_VERSION	USERBOOT_VERSION_3

#define	MALLOCSZ		(10*1024*1024)

struct loader_callbacks *callbacks;
void *callbacks_arg;

extern char bootprog_name[];
extern char bootprog_rev[];
extern char bootprog_date[];
extern char bootprog_maker[];
static jmp_buf jb;

struct arch_switch archsw;	/* MI/MD interface boundary */

static void	extract_currdev(void);

void
delay(int usec)
{

        CALLBACK(delay, usec);
}

void
exit(int v)
{

	CALLBACK(exit, v);
	longjmp(jb, 1);
}

void
loader_main(struct loader_callbacks *cb, void *arg, int version, int ndisks)
{
	static char mallocbuf[MALLOCSZ];
	const char *var;
	int i;

        if (version != USERBOOT_VERSION)
                abort();

	callbacks = cb;
        callbacks_arg = arg;
	userboot_disk_maxunit = ndisks;

	/*
	 * initialise the heap as early as possible.  Once this is done,
	 * alloc() is usable.
	 */
	setheap((void *)mallocbuf, (void *)(mallocbuf + sizeof(mallocbuf)));

        /*
         * Hook up the console
         */
	cons_probe();

	printf("\n");
	printf("%s, Revision %s\n", bootprog_name, bootprog_rev);
	printf("(%s, %s)\n", bootprog_maker, bootprog_date);
#if 0
	printf("Memory: %ld k\n", memsize() / 1024);
#endif

	setenv("LINES", "24", 1);	/* optional */

	/*
	 * Set custom environment variables
	 */
	i = 0;
	while (1) {
		var = CALLBACK(getenv, i++);
		if (var == NULL)
			break;
		putenv(var);
	}

	archsw.arch_autoload = userboot_autoload;
	archsw.arch_getdev = userboot_getdev;
	archsw.arch_copyin = userboot_copyin;
	archsw.arch_copyout = userboot_copyout;
	archsw.arch_readin = userboot_readin;
#if defined(USERBOOT_ZFS_SUPPORT)
	archsw.arch_zfs_probe = userboot_zfs_probe;
#endif

	/*
	 * March through the device switch probing for things.
	 */
	for (i = 0; devsw[i] != NULL; i++)
		if (devsw[i]->dv_init != NULL)
			(devsw[i]->dv_init)();

	extract_currdev();

	if (setjmp(jb))
		return;

	interact();			/* doesn't return */

	exit(0);
}

/*
 * Set the 'current device' by (if possible) recovering the boot device as 
 * supplied by the initial bootstrap.
 */
static void
extract_currdev(void)
{
	struct disk_devdesc dev;

	//bzero(&dev, sizeof(dev));

#if defined(USERBOOT_ZFS_SUPPORT)
	if (userboot_zfs_found) {
		struct zfs_devdesc zdev;
	
		/* Leave the pool/root guid's unassigned */
		bzero(&zdev, sizeof(zdev));
		zdev.d_dev = &zfs_dev;
		zdev.d_type = zdev.d_dev->dv_type;
		
		dev = *(struct disk_devdesc *)&zdev;
		init_zfs_bootenv(zfs_fmtdev(&dev));
	} else
#endif

	if (userboot_disk_maxunit > 0) {
		dev.d_dev = &userboot_disk;
		dev.d_type = dev.d_dev->dv_type;
		dev.d_unit = 0;
		dev.d_slice = 0;
		dev.d_partition = 0;
		/*
		 * If we cannot auto-detect the partition type then
		 * access the disk as a raw device.
		 */
		if (dev.d_dev->dv_open(NULL, &dev)) {
			dev.d_slice = -1;
			dev.d_partition = -1;
		}
	} else {
		dev.d_dev = &host_dev;
		dev.d_type = dev.d_dev->dv_type;
		dev.d_unit = 0;
	}

	env_setenv("currdev", EV_VOLATILE, userboot_fmtdev(&dev),
            userboot_setcurrdev, env_nounset);
	env_setenv("loaddev", EV_VOLATILE, userboot_fmtdev(&dev),
            env_noset, env_nounset);
}

#if defined(USERBOOT_ZFS_SUPPORT)
static void
userboot_zfs_probe(void)
{
	char devname[32];
	uint64_t pool_guid;
	int unit;

	/*
	 * Open all the disks we can find and see if we can reconstruct
	 * ZFS pools from them. Record if any were found.
	 */
	for (unit = 0; unit < userboot_disk_maxunit; unit++) {
		sprintf(devname, "disk%d:", unit);
		pool_guid = 0;
		zfs_probe_dev(devname, &pool_guid);
		if (pool_guid != 0)
			userboot_zfs_found = 1;
	}
}

COMMAND_SET(lszfs, "lszfs", "list child datasets of a zfs dataset",
	    command_lszfs);

static int
command_lszfs(int argc, char *argv[])
{
	int err;

	if (argc != 2) {
		command_errmsg = "a single dataset must be supplied";
		return (CMD_ERROR);
	}

	err = zfs_list(argv[1]);
	if (err != 0) {
		command_errmsg = strerror(err);
		return (CMD_ERROR);
	}
	return (CMD_OK);
}

COMMAND_SET(reloadbe, "reloadbe", "refresh the list of ZFS Boot Environments",
	    command_reloadbe);

static int
command_reloadbe(int argc, char *argv[])
{
	int err;
	char *root;

	if (argc > 2) {
		command_errmsg = "wrong number of arguments";
		return (CMD_ERROR);
	}

	if (argc == 2) {
		err = zfs_bootenv(argv[1]);
	} else {
		root = getenv("zfs_be_root");
		if (root == NULL) {
			return (CMD_OK);
		}
		err = zfs_bootenv(root);
	}

	if (err != 0) {
		command_errmsg = strerror(err);
		return (CMD_ERROR);
	}

	return (CMD_OK);
}
#endif /* USERBOOT_ZFS_SUPPORT */

COMMAND_SET(quit, "quit", "exit the loader", command_quit);

static int
command_quit(int argc, char *argv[])
{

	exit(USERBOOT_EXIT_QUIT);
	return (CMD_OK);
}

COMMAND_SET(reboot, "reboot", "reboot the system", command_reboot);

static int
command_reboot(int argc, char *argv[])
{

	exit(USERBOOT_EXIT_REBOOT);
	return (CMD_OK);
}
