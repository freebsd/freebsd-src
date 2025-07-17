/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
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
 */

#include <sys/cdefs.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/disk.h>
#include <sys/queue.h>

#include <machine/specialreg.h>
#include <machine/vmm.h>

#include <assert.h>
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <err.h>
#include <fcntl.h>
#include <getopt.h>
#include <libgen.h>
#include <limits.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <termios.h>
#include <unistd.h>

#include <capsicum_helpers.h>
#include <vmmapi.h>

#include "userboot.h"

#define	MB	(1024 * 1024UL)
#define	GB	(1024 * 1024 * 1024UL)
#define	BSP	0

#define	NDISKS	32

/*
 * Reason for our loader reload and reentry, though these aren't really used
 * at the moment.
 */
enum {
	/* 0 cannot be allocated; setjmp(3) return. */
	JMP_SWAPLOADER = 0x01,
	JMP_REBOOT,
};

static struct termios term, oldterm;
static int disk_fd[NDISKS];
static int ndisks;
static int consin_fd, consout_fd;
static int hostbase_fd = -1;

static void *loader_hdl;
static char *loader;
static int explicit_loader_fd = -1;
static jmp_buf jb;

static char *vmname, *progname;
static struct vmctx *ctx;
static struct vcpu *vcpu;

static uint64_t gdtbase, cr3, rsp;

static void cb_exit(void *arg, int v);

/*
 * Console i/o callbacks
 */

static void
cb_putc(void *arg __unused, int ch)
{
	char c = ch;

	(void) write(consout_fd, &c, 1);
}

static int
cb_getc(void *arg __unused)
{
	char c;

	if (read(consin_fd, &c, 1) == 1)
		return (c);
	return (-1);
}

static int
cb_poll(void *arg __unused)
{
	int n;

	if (ioctl(consin_fd, FIONREAD, &n) >= 0)
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
cb_open(void *arg __unused, const char *filename, void **hp)
{
	struct cb_file *cf;
	struct stat sb;
	int fd, flags;

	cf = NULL;
	fd = -1;
	flags = O_RDONLY | O_RESOLVE_BENEATH;
	if (hostbase_fd == -1)
		return (ENOENT);

	/* Absolute paths are relative to our hostbase, chop off leading /. */
	if (filename[0] == '/')
		filename++;

	/* Lookup of /, use . instead. */
	if (filename[0] == '\0')
		filename = ".";

	if (fstatat(hostbase_fd, filename, &sb, AT_RESOLVE_BENEATH) < 0)
		return (errno);

	if (!S_ISDIR(sb.st_mode) && !S_ISREG(sb.st_mode))
		return (EINVAL);

	if (S_ISDIR(sb.st_mode))
		flags |= O_DIRECTORY;

	/* May be opening the root dir */
	fd = openat(hostbase_fd, filename, flags);
	if (fd < 0)
		return (errno);

	cf = malloc(sizeof(struct cb_file));
	if (cf == NULL) {
		close(fd);
		return (ENOMEM);
	}

	cf->cf_stat = sb;
	cf->cf_size = cf->cf_stat.st_size;

	if (S_ISDIR(cf->cf_stat.st_mode)) {
		cf->cf_isdir = 1;
		cf->cf_u.dir = fdopendir(fd);
		if (cf->cf_u.dir == NULL) {
			close(fd);
			free(cf);
			return (ENOMEM);
		}
	} else {
		assert(S_ISREG(cf->cf_stat.st_mode));
		cf->cf_isdir = 0;
		cf->cf_u.fd = fd;
	}
	*hp = cf;
	return (0);
}

static int
cb_close(void *arg __unused, void *h)
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
cb_isdir(void *arg __unused, void *h)
{
	struct cb_file *cf = h;

	return (cf->cf_isdir);
}

static int
cb_read(void *arg __unused, void *h, void *buf, size_t size, size_t *resid)
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
cb_readdir(void *arg __unused, void *h, uint32_t *fileno_return,
    uint8_t *type_return, size_t *namelen_return, char *name)
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
cb_seek(void *arg __unused, void *h, uint64_t offset, int whence)
{
	struct cb_file *cf = h;

	if (cf->cf_isdir)
		return (EINVAL);
	if (lseek(cf->cf_u.fd, offset, whence) < 0)
		return (errno);
	return (0);
}

static int
cb_stat(void *arg __unused, void *h, struct stat *sbp)
{
	struct cb_file *cf = h;

	memset(sbp, 0, sizeof(struct stat));
	sbp->st_mode = cf->cf_stat.st_mode;
	sbp->st_uid = cf->cf_stat.st_uid;
	sbp->st_gid = cf->cf_stat.st_gid;
	sbp->st_size = cf->cf_stat.st_size;
	sbp->st_mtime = cf->cf_stat.st_mtime;
	sbp->st_dev = cf->cf_stat.st_dev;
	sbp->st_ino = cf->cf_stat.st_ino;
	
	return (0);
}

/*
 * Disk image i/o callbacks
 */

static int
cb_diskread(void *arg __unused, int unit, uint64_t from, void *to, size_t size,
    size_t *resid)
{
	ssize_t n;

	if (unit < 0 || unit >= ndisks)
		return (EIO);
	n = pread(disk_fd[unit], to, size, from);
	if (n < 0)
		return (errno);
	*resid = size - n;
	return (0);
}

static int
cb_diskwrite(void *arg __unused, int unit, uint64_t offset, void *src,
    size_t size, size_t *resid)
{
	ssize_t n;

	if (unit < 0 || unit >= ndisks)
		return (EIO);
	n = pwrite(disk_fd[unit], src, size, offset);
	if (n < 0)
		return (errno);
	*resid = size - n;
	return (0);
}

static int
cb_diskioctl(void *arg __unused, int unit, u_long cmd, void *data)
{
	struct stat sb;

	if (unit < 0 || unit >= ndisks)
		return (EBADF);

	switch (cmd) {
	case DIOCGSECTORSIZE:
		*(u_int *)data = 512;
		break;
	case DIOCGMEDIASIZE:
		if (fstat(disk_fd[unit], &sb) != 0)
			return (ENOTTY);
		if (S_ISCHR(sb.st_mode) &&
		    ioctl(disk_fd[unit], DIOCGMEDIASIZE, &sb.st_size) != 0)
				return (ENOTTY);
		*(off_t *)data = sb.st_size;
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
cb_copyin(void *arg __unused, const void *from, uint64_t to, size_t size)
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
cb_copyout(void *arg __unused, uint64_t from, void *to, size_t size)
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
cb_setreg(void *arg __unused, int r, uint64_t v)
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

	error = vm_set_register(vcpu, vmreg, v);
	if (error) {
		perror("vm_set_register");
		cb_exit(NULL, USERBOOT_EXIT_QUIT);
	}
}

static void
cb_setmsr(void *arg __unused, int r, uint64_t v)
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

	error = vm_set_register(vcpu, vmreg, v);
	if (error) {
		perror("vm_set_msr");
		cb_exit(NULL, USERBOOT_EXIT_QUIT);
	}
}

static void
cb_setcr(void *arg __unused, int r, uint64_t v)
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

	error = vm_set_register(vcpu, vmreg, v);
	if (error) {
		perror("vm_set_cr");
		cb_exit(NULL, USERBOOT_EXIT_QUIT);
	}
}

static void
cb_setgdt(void *arg __unused, uint64_t base, size_t size)
{
	int error;

	error = vm_set_desc(vcpu, VM_REG_GUEST_GDTR, base, size - 1, 0);
	if (error != 0) {
		perror("vm_set_desc(gdt)");
		cb_exit(NULL, USERBOOT_EXIT_QUIT);
	}

	gdtbase = base;
}

static void
cb_exec(void *arg __unused, uint64_t rip)
{
	int error;

	if (cr3 == 0)
		error = vm_setup_freebsd_registers_i386(vcpu, rip, gdtbase,
		    rsp);
	else
		error = vm_setup_freebsd_registers(vcpu, rip, cr3, gdtbase,
		    rsp);
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
cb_delay(void *arg __unused, int usec)
{

	usleep(usec);
}

static void
cb_exit(void *arg __unused, int v)
{

	tcsetattr(consout_fd, TCSAFLUSH, &oldterm);
	if (v == USERBOOT_EXIT_REBOOT)
		longjmp(jb, JMP_REBOOT);
	exit(v);
}

static void
cb_getmem(void *arg __unused, uint64_t *ret_lowmem, uint64_t *ret_highmem)
{

	*ret_lowmem = vm_get_lowmem_size(ctx);
	*ret_highmem = vm_get_highmem_size(ctx);
}

struct env {
	char *str;	/* name=value */
	SLIST_ENTRY(env) next;
};

static SLIST_HEAD(envhead, env) envhead;

static void
addenv(const char *str)
{
	struct env *env;

	env = malloc(sizeof(struct env));
	if (env == NULL)
		err(EX_OSERR, "malloc");
	env->str = strdup(str);
	if (env->str == NULL)
		err(EX_OSERR, "strdup");
	SLIST_INSERT_HEAD(&envhead, env, next);
}

static char *
cb_getenv(void *arg __unused, int num)
{
	int i;
	struct env *env;

	i = 0;
	SLIST_FOREACH(env, &envhead, next) {
		if (i == num)
			return (env->str);
		i++;
	}

	return (NULL);
}

static int
cb_vm_set_register(void *arg __unused, int vcpuid, int reg, uint64_t val)
{

	assert(vcpuid == BSP);
	return (vm_set_register(vcpu, reg, val));
}

static int
cb_vm_set_desc(void *arg __unused, int vcpuid, int reg, uint64_t base,
    u_int limit, u_int access)
{

	assert(vcpuid == BSP);
	return (vm_set_desc(vcpu, reg, base, limit, access));
}

static void
cb_swap_interpreter(void *arg __unused, const char *interp_req)
{

	/*
	 * If the user specified a loader but we detected a mismatch, we should
	 * not try to pivot to a different loader on them.
	 */
	free(loader);
	if (explicit_loader_fd != -1) {
		perror("requested loader interpreter does not match guest userboot");
		cb_exit(NULL, 1);
	}
	if (interp_req == NULL || *interp_req == '\0') {
		perror("guest failed to request an interpreter");
		cb_exit(NULL, 1);
	}

	if (asprintf(&loader, "userboot_%s.so", interp_req) == -1)
		err(EX_OSERR, "malloc");
	longjmp(jb, JMP_SWAPLOADER);
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
	.diskwrite = cb_diskwrite,
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

	/* Version 4 additions */
	.vm_set_register = cb_vm_set_register,
	.vm_set_desc = cb_vm_set_desc,

	/* Version 5 additions */
	.swap_interpreter = cb_swap_interpreter,
};

static int
altcons_open(char *path)
{
	struct stat sb;
	int err;
	int fd;

	/*
	 * Allow stdio to be passed in so that the same string
	 * can be used for the bhyveload console and bhyve com-port
	 * parameters
	 */
	if (!strcmp(path, "stdio"))
		return (0);

	err = stat(path, &sb);
	if (err == 0) {
		if (!S_ISCHR(sb.st_mode))
			err = ENOTSUP;
		else {
			fd = open(path, O_RDWR | O_NONBLOCK);
			if (fd < 0)
				err = errno;
			else
				consin_fd = consout_fd = fd;
		}
	}

	return (err);
}

static int
disk_open(char *path)
{
	int fd;

	if (ndisks >= NDISKS)
		return (ERANGE);

	fd = open(path, O_RDWR);
	if (fd < 0)
		return (errno);

	disk_fd[ndisks] = fd;
	ndisks++;

	return (0);
}

static void
usage(void)
{

	fprintf(stderr,
	    "usage: %s [-S][-c <console-device>] [-d <disk-path>] [-e <name=value>]\n"
	    "       %*s [-h <host-path>] [-m memsize[K|k|M|m|G|g|T|t]] <vmname>\n",
	    progname,
	    (int)strlen(progname), "");
	exit(1);
}

static void
hostbase_open(const char *base)
{
	cap_rights_t rights;

	if (hostbase_fd != -1)
		close(hostbase_fd);
	hostbase_fd = open(base, O_DIRECTORY | O_PATH);
	if (hostbase_fd == -1)
		err(EX_OSERR, "open");

	if (caph_rights_limit(hostbase_fd, cap_rights_init(&rights, CAP_FSTATAT,
	    CAP_LOOKUP, CAP_PREAD)) < 0)
		err(EX_OSERR, "caph_rights_limit");
}

static void
loader_open(int bootfd)
{
	int fd;

	if (loader == NULL) {
		loader = strdup("userboot.so");
		if (loader == NULL)
			err(EX_OSERR, "malloc");
	}

	assert(bootfd >= 0 || explicit_loader_fd >= 0);
	if (explicit_loader_fd >= 0)
		fd = explicit_loader_fd;
	else
		fd = openat(bootfd, loader, O_RDONLY | O_RESOLVE_BENEATH);
	if (fd == -1)
		err(EX_OSERR, "openat");

	loader_hdl = fdlopen(fd, RTLD_LOCAL);
	if (!loader_hdl)
		errx(EX_OSERR, "dlopen: %s", dlerror());
	if (fd != explicit_loader_fd)
		close(fd);
}

int
main(int argc, char** argv)
{
	void (*func)(struct loader_callbacks *, void *, int, int);
	uint64_t mem_size;
	int bootfd, opt, error, memflags, need_reinit;

	bootfd = -1;
	progname = basename(argv[0]);

	memflags = 0;
	mem_size = 256 * MB;

	consin_fd = STDIN_FILENO;
	consout_fd = STDOUT_FILENO;

	while ((opt = getopt(argc, argv, "CSc:d:e:h:l:m:")) != -1) {
		switch (opt) {
		case 'c':
			error = altcons_open(optarg);
			if (error != 0)
				errx(EX_USAGE, "Could not open '%s'", optarg);
			break;

		case 'd':
			error = disk_open(optarg);
			if (error != 0)
				errx(EX_USAGE, "Could not open '%s'", optarg);
			break;

		case 'e':
			addenv(optarg);
			break;

		case 'h':
			hostbase_open(optarg);
			break;

		case 'l':
			if (loader != NULL)
				errx(EX_USAGE, "-l can only be given once");
			loader = strdup(optarg);
			if (loader == NULL)
				err(EX_OSERR, "malloc");
			explicit_loader_fd = open(loader, O_RDONLY);
			if (explicit_loader_fd == -1)
				err(EX_OSERR, "%s", loader);
			break;

		case 'm':
			error = vm_parse_memsize(optarg, &mem_size);
			if (error != 0)
				errx(EX_USAGE, "Invalid memsize '%s'", optarg);
			break;
		case 'C':
			memflags |= VM_MEM_F_INCORE;
			break;
		case 'S':
			memflags |= VM_MEM_F_WIRED;
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

	need_reinit = 0;
	error = vm_create(vmname);
	if (error) {
		if (errno != EEXIST)
			err(1, "vm_create");
		need_reinit = 1;
	}

	ctx = vm_open(vmname);
	if (ctx == NULL)
		err(1, "vm_open");

	/*
	 * If we weren't given an explicit loader to use, we need to support the
	 * guest requesting a different one.
	 */
	if (explicit_loader_fd == -1) {
		cap_rights_t rights;

		bootfd = open("/boot", O_DIRECTORY | O_PATH);
		if (bootfd == -1)
			err(1, "open");

		/*
		 * bootfd will be used to do a lookup of our loader and do an
		 * fdlopen(3) on the loader; thus, we need mmap(2) in addition
		 * to the more usual lookup rights.
		 */
		if (caph_rights_limit(bootfd, cap_rights_init(&rights,
		    CAP_FSTATAT, CAP_LOOKUP, CAP_MMAP_RX, CAP_PREAD)) < 0)
			err(1, "caph_rights_limit");
	}

	vcpu = vm_vcpu_open(ctx, BSP);

	caph_cache_catpages();
	if (caph_enter() < 0)
		err(1, "caph_enter");

	/*
	 * setjmp in the case the guest wants to swap out interpreter,
	 * cb_swap_interpreter will swap out loader as appropriate and set
	 * need_reinit so that we end up in a clean state once again.
	 */
	if (setjmp(jb) != 0) {
		dlclose(loader_hdl);
		loader_hdl = NULL;

		need_reinit = 1;
	}

	if (need_reinit) {
		error = vm_reinit(ctx);
		if (error)
			err(1, "vm_reinit");
	}

	vm_set_memflags(ctx, memflags);
	error = vm_setup_memory(ctx, mem_size, VM_MMAP_ALL);
	if (error)
		err(1, "vm_setup_memory");

	loader_open(bootfd);
	func = dlsym(loader_hdl, "loader_main");
	if (!func)
		errx(1, "dlsym: %s", dlerror());

	tcgetattr(consout_fd, &term);
	oldterm = term;
	cfmakeraw(&term);
	term.c_cflag |= CLOCAL;

	tcsetattr(consout_fd, TCSAFLUSH, &term);

	addenv("smbios.bios.vendor=BHYVE");
	addenv("boot_serial=1");

	func(&cb, NULL, USERBOOT_VERSION_5, ndisks);

	free(loader);
	return (0);
}
