#include "host_syscall.h"
#include "syscall_nr.h"
#include <stand.h>

/*
 * Various trivial wrappers for Linux system calls. Please keep sorted
 * alphabetically.
 */

int
host_close(int fd)
{
	return host_syscall(SYS_close, fd);
}

int
host_dup(int fd)
{
	return host_syscall(SYS_dup, fd);
}

int
host_exit(int code)
{
	return host_syscall(SYS_exit, code);
}

/* Same system call with different names on different Linux architectures due to history */
int
host_fstat(int fd, struct host_kstat *sb)
{
#ifdef SYS_newfstat
	return host_syscall(SYS_newfstat, fd, (uintptr_t)sb);
#else
	return host_syscall(SYS_fstat, fd, (uintptr_t)sb);
#endif
}

int
host_getdents64(int fd, void *dirp, int count)
{
	return host_syscall(SYS_getdents64, fd, (uintptr_t)dirp, count);
}

int
host_getpid(void)
{
	return host_syscall(SYS_getpid);
}

int
host_gettimeofday(struct host_timeval *a, void *b)
{
	return host_syscall(SYS_gettimeofday, (uintptr_t)a, (uintptr_t)b);
}

int
host_ioctl(int fd, unsigned long request, unsigned long arg)
{
	return host_syscall(SYS_ioctl, fd, request, arg);
}

ssize_t
host_llseek(int fd, int32_t offset_high, int32_t offset_lo, uint64_t *result, int whence)
{
#ifdef SYS_llseek
	return host_syscall(SYS_llseek, fd, offset_high, offset_lo, (uintptr_t)result, whence);
#else
	int64_t rv = host_syscall(SYS_lseek, fd,
	    (int64_t)((uint64_t)offset_high << 32 | (uint32_t)offset_lo), whence);
	if (rv > 0)
		*result = (uint64_t)rv;
	return (rv);
#endif
}

int
host_kexec_load(unsigned long entry, unsigned long nsegs, struct host_kexec_segment *segs, unsigned long flags)
{
	return host_syscall(SYS_kexec_load, entry, nsegs, segs, flags);
}

int
host_mkdir(const char *path, host_mode_t mode)
{
	return host_syscall(SYS_mkdirat, HOST_AT_FDCWD, (uintptr_t)path, mode);
}

void *
host_mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off)
{
	return (void *)host_syscall(SYS_mmap, (uintptr_t)addr, len, prot, flags, fd, off);
}

int
host_mount(const char *src, const char *target, const char *type, unsigned long flags,
    void *data)
{
	return host_syscall(SYS_mount, src, target, type, flags, data);
}

int
host_munmap(void *addr, size_t len)
{
	return host_syscall(SYS_munmap, (uintptr_t)addr, len);
}

int
host_open(const char *path, int flags, int mode)
{
	return host_syscall(SYS_openat, HOST_AT_FDCWD, (uintptr_t)path, flags, mode);
	/* XXX original overrode errors */
}

ssize_t
host_read(int fd, void *buf, size_t nbyte)
{
	return host_syscall(SYS_read, fd, (uintptr_t)buf, nbyte);
	/* XXX original overrode errors */
}

int
host_reboot(int magic1, int magic2, int cmd, uintptr_t arg)
{
	return host_syscall(SYS_reboot, magic1, magic2, cmd, arg);
}

int
host_select(int nfds, long *readfds, long *writefds, long *exceptfds,
    struct host_timeval *timeout)
{
	struct timespec ts = { .tv_sec = timeout->tv_sec, .tv_nsec = timeout->tv_usec * 1000 };

	/*
	 * Note, final arg is a sigset_argpack since most arch can only have 6
	 * syscall args. Since we're not masking signals, though, we can just
	 * pass a NULL.
	 */
	return host_syscall(SYS_pselect6, nfds, (uintptr_t)readfds, (uintptr_t)writefds,
	    (uintptr_t)exceptfds, (uintptr_t)&ts, (uintptr_t)NULL);
}

int
host_stat(const char *path, struct host_kstat *sb)
{
	return host_syscall(SYS_newfstatat, HOST_AT_FDCWD, (uintptr_t)path, (uintptr_t)sb, 0);
}

int
host_symlink(const char *path1, const char *path2)
{
	return host_syscall(SYS_symlinkat, HOST_AT_FDCWD, path1, path2);
}

int
host_uname(struct old_utsname *uts)
{
	return host_syscall(SYS_uname, (uintptr_t)uts);
}

ssize_t
host_write(int fd, const void *buf, size_t nbyte)
{
	return host_syscall(SYS_write, fd, (uintptr_t)buf, nbyte);
}
