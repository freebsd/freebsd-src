/*-
 * Copyright (c) 2011 NetApp, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*-
 * Copyright (c) 2011 Google, Inc.
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
 *
 * $FreeBSD$
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/disk.h>

#include <machine/specialreg.h>
#include <machine/vmm.h>

#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include <vmmapi.h>

#include "userboot.h"

#define	MB	(1024 * 1024UL)
#define	GB	(1024 * 1024 * 1024UL)
#define	BSP	0

static char *host_base = "/";
static struct termios term, oldterm;
static int disk_fd = -1;

static char *vmname, *progname;
static struct vmctx *ctx;

static uint64_t gdtbase, cr3, rsp;

static void cb_exit(void *arg, int v);

/*
 * Console i/o callbacks
 */

static void
cb_putc(void *arg, int ch)
{
	char c = ch;

	write(1, &c, 1);
}

static int
cb_getc(void *arg)
{
	char c;

	if (read(0, &c, 1) == 1)
		return (c);
	return (-1);
}

static int
cb_poll(void *arg)
{
	int n;

	if (ioctl(0, FIONREAD, &n) >= 0)
		return (n > 0);
	return (0);
}

/*
 * Host filesystem i/o callbacks
 */

struct cb_file {
	int cf_isdir;
	size_t cf_size;
	struct stat cf_stat;
	union {
		int fd;
		DIR *dir;
	} cf_u;
};

static int
cb_open(void *arg, const char *filename, void **hp)
{
	struct stat st;
	struct cb_file *cf;
	char path[PATH_MAX];

	if (!host_base)
		return (ENOENT);

	strlcpy(path, host_base, PATH_MAX);
	if (path[strlen(path) - 1] == '/')
		path[strlen(path) - 1] = 0;
	strlcat(path, filename, PATH_MAX);
	cf = malloc(sizeof(struct cb_file));
	if (stat(path, &cf->cf_stat) < 0) {
		free(cf);
		return (errno);
	}

	cf->cf_size = st.st_size;
	if (S_ISDIR(cf->cf_stat.st_mode)) {
		cf->cf_isdir = 1;
		cf->cf_u.dir = opendir(path);
		if (!cf->cf_u.dir)
			goto out;
		*hp = cf;
		return (0);
	}
	if (S_ISREG(cf->cf_stat.st_mode)) {
		cf->cf_isdir = 0;
		cf->cf_u.fd = open(path, O_RDONLY);
		if (cf->cf_u.fd < 0)
			goto out;
		*hp = cf;
		return (0);
	}

out:
	free(cf);
	return (EINVAL);
}

static int
cb_close(void *arg, void *h)
{
	struct cb_file *cf = h;

	if (cf->cf_isdir)
		closedir(cf->cf_u.dir);
	else
		close(cf->cf_u.fd);
	free(cf);

	return (0);
}

static int
cb_isdir(void *arg, void *h)
{
	struct cb_file *cf = h;

	return (cf->cf_isdir);
}

static int
cb_read(void *arg, void *h, void *buf, size_t size, size_t *resid)
{
	struct cb_file *cf = h;
	ssize_t sz;

	if (cf->cf_isdir)
		return (EINVAL);
	sz = read(cf->cf_u.fd, buf, size);
	if (sz < 0)
		return (EINVAL);
	*resid = size - sz;
	return (0);
}

static int
cb_readdir(void *arg, void *h, uint32_t *fileno_return, uint8_t *type_return,
	   size_t *namelen_return, char *name)
{
	struct cb_file *cf = h;
	struct dirent *dp;

	if (!cf->cf_isdir)
		return (EINVAL);

	dp = readdir(cf->cf_u.dir);
	if (!dp)
		return (ENOENT);

	/*
	 * Note: d_namlen is in the range 0..255 and therefore less
	 * than PATH_MAX so we don't need to test before copying.
	 */
	*fileno_return = dp->d_fileno;
	*type_return = dp->d_type;
	*namelen_return = dp->d_namlen;
	memcpy(name, dp->d_name, dp->d_namlen);
	name[dp->d_namlen] = 0;

	return (0);
}

static int
cb_seek(void *arg, void *h, uint64_t offset, int whence)
{
	struct cb_file *cf = h;

	if (cf->cf_isdir)
		return (EINVAL);
	if (lseek(cf->cf_u.fd, offset, whence) < 0)
		return (errno);
	return (0);
}

static int
cb_stat(void *arg, void *h, int *mode, int *uid, int *gid, uint64_t *size)
{
	struct cb_file *cf = h;

	*mode = cf->cf_stat.st_mode;
	*uid = cf->cf_stat.st_uid;
	*gid = cf->cf_stat.st_gid;
	*size = cf->cf_stat.st_size;
	return (0);
}

/*
 * Disk image i/o callbacks
 */

static int
cb_diskread(void *arg, int unit, uint64_t from, void *to, size_t size,
	    size_t *resid)
{
	ssize_t n;

	if (unit != 0 || disk_fd == -1)
		return (EIO);
	n = pread(disk_fd, to, size, from);
	if (n < 0)
		return (errno);
	*resid = size - n;
	return (0);
}

static int
cb_diskioctl(void *arg, int unit, u_long cmd, void *data)
{
	struct stat sb;

	if (unit != 0 || disk_fd == -1)
		return (EBADF);

	switch (cmd) {
	case DIOCGSECTORSIZE:
		*(u_int *)data = 512;
		break;
	case DIOCGMEDIASIZE:
		if (fstat(disk_fd, &sb) == 0)
			*(off_t *)data = sb.st_size;
		else
			return (ENOTTY);
		break;
	default:
		return (ENOTTY);
	}

	return (0);
}

/*
 * Guest virtual machine i/o callbacks
 */
static int
cb_copyin(void *arg, const void *from, uint64_t to, size_t size)
{
	char *ptr;

	to &= 0x7fffffff;

	ptr = vm_map_gpa(ctx, to, size);
	if (ptr == NULL)
		return (EFAULT);

	memcpy(ptr, from, size);
	return (0);
}

static int
cb_copyout(void *arg, uint64_t from, void *to, size_t size)
{
	char *ptr;

	from &= 0x7fffffff;

	ptr = vm_map_gpa(ctx, from, size);
	if (ptr == NULL)
		return (EFAULT);

	memcpy(to, ptr, size);
	return (0);
}

static void
cb_setreg(void *arg, int r, uint64_t v)
{
	int error;
	enum vm_reg_name vmreg;
	
	vmreg = VM_REG_LAST;

	switch (r) {
	case 4:
		vmreg = VM_REG_GUEST_RSP;
		rsp = v;
		break;
	default:
		break;
	}

	if (vmreg == VM_REG_LAST) {
		printf("test_setreg(%d): not implemented\n", r);
		cb_exit(NULL, USERBOOT_EXIT_QUIT);
	}

	error = vm_set_register(ctx, BSP, vmreg, v);
	if (error) {
		perror("vm_set_register");
		cb_exit(NULL, USERBOOT_EXIT_QUIT);
	}
}

static void
cb_setmsr(void *arg, int r, uint64_t v)
{
	int error;
	enum vm_reg_name vmreg;
	
	vmreg = VM_REG_LAST;

	switch (r) {
	case MSR_EFER:
		vmreg = VM_REG_GUEST_EFER;
		break;
	default:
		break;
	}

	if (vmreg == VM_REG_LAST) {
		printf("test_setmsr(%d): not implemented\n", r);
		cb_exit(NULL, USERBOOT_EXIT_QUIT);
	}

	error = vm_set_register(ctx, BSP, vmreg, v);
	if (error) {
		perror("vm_set_msr");
		cb_exit(NULL, USERBOOT_EXIT_QUIT);
	}
}

static void
cb_setcr(void *arg, int r, uint64_t v)
{
	int error;
	enum vm_reg_name vmreg;
	
	vmreg = VM_REG_LAST;

	switch (r) {
	case 0:
		vmreg = VM_REG_GUEST_CR0;
		break;
	case 3:
		vmreg = VM_REG_GUEST_CR3;
		cr3 = v;
		break;
	case 4:
		vmreg = VM_REG_GUEST_CR4;
		break;
	default:
		break;
	}

	if (vmreg == VM_REG_LAST) {
		printf("test_setcr(%d): not implemented\n", r);
		cb_exit(NULL, USERBOOT_EXIT_QUIT);
	}

	error = vm_set_register(ctx, BSP, vmreg, v);
	if (error) {
		perror("vm_set_cr");
		cb_exit(NULL, USERBOOT_EXIT_QUIT);
	}
}

static void
cb_setgdt(void *arg, uint64_t base, size_t size)
{
	int error;

	error = vm_set_desc(ctx, BSP, VM_REG_GUEST_GDTR, base, size - 1, 0);
	if (error != 0) {
		perror("vm_set_desc(gdt)");
		cb_exit(NULL, USERBOOT_EXIT_QUIT);
	}

	gdtbase = base;
}

static void
cb_exec(void *arg, uint64_t rip)
{
	int error;

	error = vm_setup_freebsd_registers(ctx, BSP, rip, cr3, gdtbase, rsp);
	if (error) {
		perror("vm_setup_freebsd_registers");
		cb_exit(NULL, USERBOOT_EXIT_QUIT);
	}

	cb_exit(NULL, 0);
}

/*
 * Misc
 */

static void
cb_delay(void *arg, int usec)
{

	usleep(usec);
}

static void
cb_exit(void *arg, int v)
{

	tcsetattr(0, TCSAFLUSH, &oldterm);
	exit(v);
}

static void
cb_getmem(void *arg, uint64_t *ret_lowmem, uint64_t *ret_highmem)
{

	vm_get_memory_seg(ctx, 0, ret_lowmem);
	vm_get_memory_seg(ctx, 4 * GB, ret_highmem);
}

static const char *
cb_getenv(void *arg, int num)
{
	int max;

	static const char * var[] = {
		"smbios.bios.vendor=BHYVE",
		"boot_serial=1",
		NULL
	};

	max = sizeof(var) / sizeof(var[0]);

	if (num < max)
		return (var[num]);
	else
		return (NULL);
}

static struct loader_callbacks cb = {
	.getc = cb_getc,
	.putc = cb_putc,
	.poll = cb_poll,

	.open = cb_open,
	.close = cb_close,
	.isdir = cb_isdir,
	.read = cb_read,
	.readdir = cb_readdir,
	.seek = cb_seek,
	.stat = cb_stat,

	.diskread = cb_diskread,
	.diskioctl = cb_diskioctl,

	.copyin = cb_copyin,
	.copyout = cb_copyout,
	.setreg = cb_setreg,
	.setmsr = cb_setmsr,
	.setcr = cb_setcr,
	.setgdt = cb_setgdt,
	.exec = cb_exec,

	.delay = cb_delay,
	.exit = cb_exit,
	.getmem = cb_getmem,

	.getenv = cb_getenv,
};

static void
usage(void)
{

	fprintf(stderr,
		"usage: %s [-m mem-size][-d <disk-path>] [-h <host-path>] "
		"<vmname>\n", progname);
	exit(1);
}

int
main(int argc, char** argv)
{
	void *h;
	void (*func)(struct loader_callbacks *, void *, int, int);
	uint64_t mem_size;
	int opt, error;
	char *disk_image;

	progname = argv[0];

	mem_size = 256 * MB;
	disk_image = NULL;

	while ((opt = getopt(argc, argv, "d:h:m:")) != -1) {
		switch (opt) {
		case 'd':
			disk_image = optarg;
			break;

		case 'h':
			host_base = optarg;
			break;

		case 'm':
			mem_size = strtoul(optarg, NULL, 0) * MB;
			break;
		
		case '?':
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage();

	vmname = argv[0];

	error = vm_create(vmname);
	if (error != 0 && errno != EEXIST) {
		perror("vm_create");
		exit(1);

	}

	ctx = vm_open(vmname);
	if (ctx == NULL) {
		perror("vm_open");
		exit(1);
	}

	error = vm_setup_memory(ctx, mem_size, VM_MMAP_ALL);
	if (error) {
		perror("vm_setup_memory");
		exit(1);
	}

	tcgetattr(0, &term);
	oldterm = term;
	term.c_lflag &= ~(ICANON|ECHO);
	term.c_iflag &= ~ICRNL;
	tcsetattr(0, TCSAFLUSH, &term);
	h = dlopen("/boot/userboot.so", RTLD_LOCAL);
	if (!h) {
		printf("%s\n", dlerror());
		return (1);
	}
	func = dlsym(h, "loader_main");
	if (!func) {
		printf("%s\n", dlerror());
		return (1);
	}

	if (disk_image) {
		disk_fd = open(disk_image, O_RDONLY);
	}
	func(&cb, NULL, USERBOOT_VERSION_3, disk_fd >= 0);
}
