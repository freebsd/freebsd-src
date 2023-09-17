/*-
 * Copyright (c) 2022, Netflix, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Mini-init(8) so we can run as init/pid 1 in a LinuxBoot environment.
 */

#include "stand.h"
#include "host_syscall.h"
#include "kboot.h"

/*
 * Create a 'standard' early boot environment. Cribbed from the things that
 * sysvinit, u-root, and initramfs-tools do. This is a minimal environment
 * for modern Linux systems, though the /tmp, /run and /var stuff can likely
 * be done inside the initrd image itself (as can creating the mount points
 * for /proc, /dev and /sys).
 *
 * Note: We ignore errors here. There's no stderr to report them to yet. These
 * operations generally can't fail, but if they do, we may not have the ability
 * to report them later.
 */
static void
init_fs_env(void)
{
	/*
	 * Create directories for mandatory filesystems and mount them.
	 */
	host_mkdir("/proc", 0555);
	host_mount("proc", "/proc", "proc", MS_RELATIME, "");
	host_mkdir("/sys", 0555);
	host_mount("sysfs", "/sys", "sysfs", MS_RELATIME, "");
	host_mkdir("/dev", 0755);
	host_mount("devtmpfs", "/dev", "devtmpfs", MS_RELATIME,
	    "mode=0755,nr_inodes=0");

	/*
	 * Create compat links: /dev/fd lives in /proc, and needs some help to
	 * get setup.
	 */
	host_symlink("/proc/self/fd", "/dev/fd");
	host_symlink("fd/0", "/dev/stdin");
	host_symlink("fd/1", "/dev/stdout");
	host_symlink("fd/2", "/dev/stderr");


	/*
	 * Unsure if we need this, but create a sane /tmp just in case that's useful.
	 * and point /run over to it.
	 */
	host_mkdir("/tmp", 01777);
	host_mount("tmpfs", "/tmp", "tmpfs", MS_RELATIME, "size=10%,mode=1777");
	host_symlink("/tmp", "/run");

	/*
	 * Unsure the loader needs /var and /var/log, but they are easy to
	 * create.
	 */
	host_mkdir("/var", 0555);
	host_mkdir("/var/lock", 0555);
	host_symlink("/tmp", "/var/tmp");
}

static void
init_tty(void)
{
	int fd;

	/*
	 * sysvinit asks the linux kernel to convert the CTRL-ALT-DEL to a SIGINT,
	 * but we skip that.
	 */

	/*
	 * Setup /dev/console as stdin/out/err
	 */
	host_close(0);
	host_close(1);
	host_close(2);
	fd = host_open("/dev/console", HOST_O_RDWR | HOST_O_NOCTTY, 0);
	host_dup(fd);
	host_dup(fd);
#if 0
	/*
	 * I think we may need to put it in 'raw' mode, but maybe not. Linux
	 * sysvinit sets it into 'sane' mode with several tweaks. Not enabled at
	 * the moment since host console initialization seems sufficient.
	 */
	struct host_termios tty;

	host_cfmakeraw(&tty);
	host_tcsetattr(fd, HOST_TCANOW, &tty);
	host_tcflush(fd, HOST_TCIOFLUSH)
#endif
}

static void
init_sig(void)
{
	/*
	 * since we're running as init, we need to catch some signals
	 */

	/*
	 * setup signals here
	 *
	 * sysvinit catches a lot of signals, but the boot loader needn't catch
	 * so many since we don't do as much as it does. If we need to, put the
	 * signal catching / ignoring code here. If we implement a 'shell'
	 * function to spawn a sub-shell, we'll likely need to do a lot more.
	 */
}

void
do_init(void)
{
	/*
	 * Only pid 1 is init
	 */
	if (host_getpid() != 1)
		return;

	init_fs_env();
	init_tty();
	init_sig();
}
