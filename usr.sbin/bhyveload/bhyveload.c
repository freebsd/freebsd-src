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
#include <stdbool.h>
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

struct env {
	char *str;	/* name=value */
	SLIST_ENTRY(env) next;
};

struct bhyveload_softc {
	struct vmctx *ctx;
	struct vcpu *vcpu;

	SLIST_HEAD(, env) envhead;

	struct termios oldterm;

	char *loader;

	int boot_dfd;			/* directory fd for host /boot */
	int consin_fd, consout_fd;	/* console I/O fds */
	int explicit_loader_fd;		/* handle for alternate userboot.so */
	int hostbase_fd;		/* host root directory fd */

	int ndisks;
	int disk_fd[NDISKS];

	uint64_t gdtbase;
	uint64_t cr3;
	uint64_t rsp;

	jmp_buf jb;
};

static void cb_exit(void *arg, int v);

/*
 * Console i/o callbacks
 */

static void
cb_putc(void *arg, int ch)
{
	struct bhyveload_softc *sc = arg;
	char c = ch;

	(void)write(sc->consout_fd, &c, 1);
}

static int
cb_getc(void *arg)
{
	struct bhyveload_softc *sc = arg;
	char c;
	ssize_t nread;

	nread = read(sc->consin_fd, &c, 1);
	if (nread == 0)
		cb_exit(sc, USERBOOT_EXIT_QUIT);
	if (nread == 1)
		return (c);
	return (-1);
}

static int
cb_poll(void *arg)
{
	struct bhyveload_softc *sc = arg;
	int n;

	if (ioctl(sc->consin_fd, FIONREAD, &n) >= 0)
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
	struct bhyveload_softc *sc = arg;
	struct cb_file *cf;
	struct stat sb;
	int fd;

	cf = NULL;
	if (sc->hostbase_fd == -1)
		return (ENOENT);

	/* Absolute paths are relative to our hostbase, chop off leading /. */
	if (filename[0] == '/')
		filename++;

	/* Lookup of /, use . instead. */
	if (filename[0] == '\0')
		filename = ".";

	/* May be opening the root dir */
	fd = openat(sc->hostbase_fd, filename, O_RDONLY | O_RESOLVE_BENEATH);
	if (fd < 0)
		return (errno);

	if (fstat(fd, &sb) < 0) {
		int serrno = errno;

		close(fd);
		return (serrno);
	} else if (!S_ISDIR(sb.st_mode) && !S_ISREG(sb.st_mode)) {
		close(fd);
		return (EINVAL);
	}

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
cb_diskread(void *arg, int unit, uint64_t from, void *to, size_t size,
    size_t *resid)
{
	struct bhyveload_softc *sc = arg;
	ssize_t n;

	if (unit < 0 || unit >= sc->ndisks)
		return (EIO);
	n = pread(sc->disk_fd[unit], to, size, from);
	if (n < 0)
		return (errno);
	*resid = size - n;
	return (0);
}

static int
cb_diskwrite(void *arg, int unit, uint64_t offset, void *src, size_t size,
    size_t *resid)
{
	struct bhyveload_softc *sc = arg;
	ssize_t n;

	if (unit < 0 || unit >= sc->ndisks)
		return (EIO);
	n = pwrite(sc->disk_fd[unit], src, size, offset);
	if (n < 0)
		return (errno);
	*resid = size - n;
	return (0);
}

static int
cb_diskioctl(void *arg, int unit, u_long cmd, void *data)
{
	struct bhyveload_softc *sc = arg;
	struct stat sb;

	if (unit < 0 || unit >= sc->ndisks)
		return (EBADF);

	switch (cmd) {
	case DIOCGSECTORSIZE:
		if (fstat(sc->disk_fd[unit], &sb) != 0)
			return (ENOTTY);
		if (S_ISCHR(sb.st_mode)) {
			if (ioctl(sc->disk_fd[unit], DIOCGSECTORSIZE, data) !=
			    0)
				return (ENOTTY);
		} else {
			*(u_int *)data = 512;
		}
		break;
	case DIOCGMEDIASIZE:
		if (fstat(sc->disk_fd[unit], &sb) != 0)
			return (ENOTTY);
		if (S_ISCHR(sb.st_mode) &&
		    ioctl(sc->disk_fd[unit], DIOCGMEDIASIZE, &sb.st_size) != 0)
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
cb_copyin(void *arg, const void *from, uint64_t to, size_t size)
{
	struct bhyveload_softc *sc = arg;
	char *ptr;

	to &= 0x7fffffff;

	ptr = vm_map_gpa(sc->ctx, to, size);
	if (ptr == NULL)
		return (EFAULT);

	memcpy(ptr, from, size);
	return (0);
}

static int
cb_copyout(void *arg, uint64_t from, void *to, size_t size)
{
	struct bhyveload_softc *sc = arg;
	char *ptr;

	from &= 0x7fffffff;

	ptr = vm_map_gpa(sc->ctx, from, size);
	if (ptr == NULL)
		return (EFAULT);

	memcpy(to, ptr, size);
	return (0);
}

static void
cb_setreg(void *arg, int r, uint64_t v)
{
	struct bhyveload_softc *sc = arg;
	int error;
	enum vm_reg_name vmreg;

	vmreg = VM_REG_LAST;

	switch (r) {
	case 4:
		vmreg = VM_REG_GUEST_RSP;
		sc->rsp = v;
		break;
	default:
		break;
	}

	if (vmreg == VM_REG_LAST) {
		printf("test_setreg(%d): not implemented\n", r);
		cb_exit(sc, USERBOOT_EXIT_QUIT);
	}

	error = vm_set_register(sc->vcpu, vmreg, v);
	if (error) {
		perror("vm_set_register");
		cb_exit(sc, USERBOOT_EXIT_QUIT);
	}
}

static void
cb_setmsr(void *arg, int r, uint64_t v)
{
	struct bhyveload_softc *sc = arg;
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
		cb_exit(sc, USERBOOT_EXIT_QUIT);
	}

	error = vm_set_register(sc->vcpu, vmreg, v);
	if (error) {
		perror("vm_set_msr");
		cb_exit(sc, USERBOOT_EXIT_QUIT);
	}
}

static void
cb_setcr(void *arg, int r, uint64_t v)
{
	struct bhyveload_softc *sc = arg;
	int error;
	enum vm_reg_name vmreg;
	
	vmreg = VM_REG_LAST;

	switch (r) {
	case 0:
		vmreg = VM_REG_GUEST_CR0;
		break;
	case 3:
		vmreg = VM_REG_GUEST_CR3;
		sc->cr3 = v;
		break;
	case 4:
		vmreg = VM_REG_GUEST_CR4;
		break;
	default:
		break;
	}

	if (vmreg == VM_REG_LAST) {
		printf("test_setcr(%d): not implemented\n", r);
		cb_exit(sc, USERBOOT_EXIT_QUIT);
	}

	error = vm_set_register(sc->vcpu, vmreg, v);
	if (error) {
		perror("vm_set_cr");
		cb_exit(sc, USERBOOT_EXIT_QUIT);
	}
}

static void
cb_setgdt(void *arg, uint64_t base, size_t size)
{
	struct bhyveload_softc *sc = arg;
	int error;

	error = vm_set_desc(sc->vcpu, VM_REG_GUEST_GDTR, base, size - 1, 0);
	if (error != 0) {
		perror("vm_set_desc(gdt)");
		cb_exit(sc, USERBOOT_EXIT_QUIT);
	}

	sc->gdtbase = base;
}

static void
cb_exec(void *arg, uint64_t rip)
{
	struct bhyveload_softc *sc = arg;
	int error;

	if (sc->cr3 == 0)
		error = vm_setup_freebsd_registers_i386(sc->vcpu, rip,
		    sc->gdtbase, sc->rsp);
	else
		error = vm_setup_freebsd_registers(sc->vcpu, rip,
		    sc->cr3, sc->gdtbase, sc->rsp);
	if (error) {
		perror("vm_setup_freebsd_registers");
		cb_exit(sc, USERBOOT_EXIT_QUIT);
	}

	cb_exit(sc, 0);
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
cb_exit(void *arg, int v)
{
	struct bhyveload_softc *sc = arg;

	tcsetattr(sc->consout_fd, TCSAFLUSH, &sc->oldterm);
	if (v == USERBOOT_EXIT_REBOOT)
		longjmp(sc->jb, JMP_REBOOT);
	exit(v);
}

static void
cb_getmem(void *arg, uint64_t *ret_lowmem, uint64_t *ret_highmem)
{
	struct bhyveload_softc *sc = arg;

	*ret_lowmem = vm_get_lowmem_size(sc->ctx);
	*ret_highmem = vm_get_highmem_size(sc->ctx);
}

static void
addenv(struct bhyveload_softc *sc, const char *str)
{
	struct env *env;

	env = malloc(sizeof(struct env));
	if (env == NULL)
		err(EX_OSERR, "malloc");
	env->str = strdup(str);
	if (env->str == NULL)
		err(EX_OSERR, "strdup");
	SLIST_INSERT_HEAD(&sc->envhead, env, next);
}

static char *
cb_getenv(void *arg, int num)
{
	struct bhyveload_softc *sc = arg;
	struct env *env;
	int i;

	i = 0;
	SLIST_FOREACH(env, &sc->envhead, next) {
		if (i == num)
			return (env->str);
		i++;
	}

	return (NULL);
}

static int
cb_vm_set_register(void *arg, int vcpuid, int reg, uint64_t val)
{
	struct bhyveload_softc *sc = arg;

	assert(vcpuid == BSP);
	return (vm_set_register(sc->vcpu, reg, val));
}

static int
cb_vm_set_desc(void *arg, int vcpuid, int reg, uint64_t base, u_int limit,
    u_int access)
{
	struct bhyveload_softc *sc = arg;

	assert(vcpuid == BSP);
	return (vm_set_desc(sc->vcpu, reg, base, limit, access));
}

static void
cb_swap_interpreter(void *arg, const char *interp_req)
{
	struct bhyveload_softc *sc = arg;

	/*
	 * If the user specified a loader but we detected a mismatch, we should
	 * not try to pivot to a different loader on them.
	 */
	free(sc->loader);
	if (sc->explicit_loader_fd != -1) {
		perror("requested loader interpreter does not match guest userboot");
		cb_exit(sc, 1);
	}
	if (interp_req == NULL || *interp_req == '\0') {
		perror("guest failed to request an interpreter");
		cb_exit(sc, 1);
	}
	if (asprintf(&sc->loader, "userboot_%s.so", interp_req) == -1)
		err(EX_OSERR, "malloc");
	longjmp(sc->jb, JMP_SWAPLOADER);
}

static void
cb_accept_interpreter(void *arg)
{
	struct bhyveload_softc *sc = arg;

	if (sc->boot_dfd == -1)
		return;

	close(sc->boot_dfd);
	sc->boot_dfd = -1;
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

	/* Version 6 additions */
	.accept_interpreter = cb_accept_interpreter,
};

static int
altcons_open(struct bhyveload_softc *sc, const char *path)
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
				sc->consin_fd = sc->consout_fd = fd;
		}
	}

	return (err);
}

static int
disk_open(struct bhyveload_softc *sc, const char *path)
{
	struct stat sbuf;
	off_t size;
	int fd, ret, sectsz;

	if (sc->ndisks >= NDISKS)
		return (ERANGE);

	fd = open(path, O_RDWR);
	if (fd < 0)
		fd = open(path, O_RDONLY);
	if (fd < 0)
		return (errno);
	if (fstat(fd, &sbuf) < 0) {
		ret = errno;
		goto err;
	}

	size = sbuf.st_size;
	sectsz = DEV_BSIZE;
	if (S_ISCHR(sbuf.st_mode)) {
		if (ioctl(fd, DIOCGMEDIASIZE, &size) < 0 ||
		    ioctl(fd, DIOCGSECTORSIZE, &sectsz) < 0) {
			ret = errno;
			goto err;
		}
		assert(size != 0);
		assert(sectsz != 0);
	}

	sc->disk_fd[sc->ndisks++] = fd;

	return (0);
err:
	close(fd);
	return (ret);
}

static void
usage(const char *progname)
{

	fprintf(stderr,
	    "usage: %s [-S][-c <console-device>] [-d <disk-path>] [-e <name=value>]\n"
	    "       %*s [-h <host-path>] [-m memsize[K|k|M|m|G|g|T|t]] <vmname>\n",
	    progname,
	    (int)strlen(progname), "");
	exit(1);
}

static int
hostbase_open(const char *base)
{
	cap_rights_t rights;
	int fd;

	fd = open(base, O_DIRECTORY | O_PATH);
	if (fd == -1)
		err(EX_OSERR, "open");

	if (caph_rights_limit(fd,
	    cap_rights_init(&rights, CAP_FSTATAT, CAP_LOOKUP, CAP_PREAD)) < 0)
		err(EX_OSERR, "caph_rights_limit");

	return (fd);
}

static void *
loader_open(struct bhyveload_softc *sc)
{
	void *handle;
	int fd;

	if (sc->loader == NULL) {
		sc->loader = strdup("userboot.so");
		if (sc->loader == NULL)
			err(EX_OSERR, "malloc");
	}

	assert(sc->boot_dfd >= 0 || sc->explicit_loader_fd >= 0);
	if (sc->explicit_loader_fd >= 0)
		fd = sc->explicit_loader_fd;
	else
		fd = openat(sc->boot_dfd, sc->loader,
		    O_RDONLY | O_RESOLVE_BENEATH);
	if (fd == -1)
		err(EX_OSERR, "openat");

	handle = fdlopen(fd, RTLD_LOCAL);
	if (handle == NULL)
		errx(EX_OSERR, "dlopen: %s", dlerror());
	if (fd != sc->explicit_loader_fd)
		close(fd);

	return (handle);
}

int
main(int argc, char** argv)
{
	struct bhyveload_softc sc;
	struct termios term;
	void (*func)(struct loader_callbacks *, void *, int, int);
	void * volatile loader_hdl;
	char *vmname, *progname;
	uint64_t mem_size;
	int opt, error, memflags;
	bool need_reinit;

	progname = basename(argv[0]);

	memset(&sc, 0, sizeof(sc));
	SLIST_INIT(&sc.envhead);
	sc.boot_dfd = sc.explicit_loader_fd = sc.hostbase_fd = -1;
	sc.consin_fd = STDIN_FILENO;
	sc.consout_fd = STDOUT_FILENO;

	memflags = 0;
	mem_size = 256 * MB;

	while ((opt = getopt(argc, argv, "CSc:d:e:h:l:m:")) != -1) {
		switch (opt) {
		case 'c':
			error = altcons_open(&sc, optarg);
			if (error != 0)
				errx(EX_USAGE, "Could not open '%s'", optarg);
			break;

		case 'd':
			error = disk_open(&sc, optarg);
			if (error != 0)
				errc(EX_USAGE, error, "Could not open '%s'", optarg);
			break;

		case 'e':
			addenv(&sc, optarg);
			break;

		case 'h':
			if (sc.hostbase_fd != -1)
				close(sc.hostbase_fd);
			sc.hostbase_fd = hostbase_open(optarg);
			break;

		case 'l':
			if (sc.loader != NULL)
				errx(EX_USAGE, "-l can only be given once");
			sc.loader = strdup(optarg);
			if (sc.loader == NULL)
				err(EX_OSERR, "malloc");
			sc.explicit_loader_fd = open(sc.loader, O_RDONLY);
			if (sc.explicit_loader_fd == -1)
				err(EX_OSERR, "%s", sc.loader);
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
			usage(progname);
		}
	}

	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage(progname);

	vmname = argv[0];

	/*
	 * If we weren't given an explicit loader to use, we need to support the
	 * guest requesting a different one.
	 */
	if (sc.explicit_loader_fd == -1) {
		cap_rights_t rights;
		int fd;

		fd = open("/boot", O_DIRECTORY | O_PATH);
		if (fd == -1)
			err(1, "open");

		/*
		 * boot_dfd will be used to do a lookup of our loader and do an
		 * fdlopen(3) on the loader; thus, we need mmap(2) in addition
		 * to the more usual lookup rights.
		 */
		if (caph_rights_limit(fd, cap_rights_init(&rights,
		    CAP_FSTATAT, CAP_LOOKUP, CAP_MMAP_RX, CAP_PREAD)) < 0)
			err(1, "caph_rights_limit");
		sc.boot_dfd = fd;
	}

	need_reinit = false;
	error = vm_create(vmname);
	if (error) {
		if (errno != EEXIST)
			err(1, "vm_create");
		need_reinit = true;
	}

	sc.ctx = vm_open(vmname);
	if (sc.ctx == NULL)
		err(1, "vm_open");

	sc.vcpu = vm_vcpu_open(sc.ctx, BSP);
	if (sc.vcpu == NULL)
		err(1, "vm_vcpu_open");

	caph_cache_catpages();
	if (caph_enter() < 0)
		err(1, "caph_enter");

	/*
	 * setjmp in the case the guest wants to swap out interpreter,
	 * cb_swap_interpreter will swap out loader as appropriate and set
	 * need_reinit so that we end up in a clean state once again.
	 */
	loader_hdl = NULL;
	if (setjmp(sc.jb) != 0) {
		dlclose(__DEVOLATILE(void *, loader_hdl));
		loader_hdl = NULL;

		need_reinit = true;
	}

	if (need_reinit) {
		error = vm_reinit(sc.ctx);
		if (error)
			err(1, "vm_reinit");
	}

	vm_set_memflags(sc.ctx, memflags);
	error = vm_setup_memory(sc.ctx, mem_size, VM_MMAP_ALL);
	if (error)
		err(1, "vm_setup_memory");

	loader_hdl = loader_open(&sc);
	func = dlsym(__DEVOLATILE(void *, loader_hdl), "loader_main");
	if (!func)
		errx(1, "dlsym: %s", dlerror());

	tcgetattr(sc.consout_fd, &sc.oldterm);
	term = sc.oldterm;
	cfmakeraw(&term);
	term.c_cflag |= CLOCAL;
	tcsetattr(sc.consout_fd, TCSAFLUSH, &term);

	addenv(&sc, "smbios.bios.vendor=BHYVE");
	addenv(&sc, "boot_serial=1");

	func(&cb, &sc, USERBOOT_VERSION_6, sc.ndisks);

	return (0);
}
