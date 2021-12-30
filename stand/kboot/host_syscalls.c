#include "host_syscall.h"
#include "syscall_nr.h"
#include <stand.h>

ssize_t
host_read(int fd, void *buf, size_t nbyte)
{
	return host_syscall(SYS_read, fd, (uintptr_t)buf, nbyte);
	/* XXX original overrode errors */
}

ssize_t
host_write(int fd, const void *buf, size_t nbyte)
{
	return host_syscall(SYS_write, fd, (uintptr_t)buf, nbyte);
}
	
int
host_open(const char *path, int flags, int mode)
{
	return host_syscall(SYS_open, (uintptr_t)path, flags, mode);
	/* XXX original overrode errors */
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
host_close(int fd)
{
	return host_syscall(SYS_close, fd);
}

void *
host_mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off)
{
	return (void *)host_syscall(SYS_mmap, (uintptr_t)addr, len, prot, flags, fd, off);
}

int
host_uname(struct old_utsname *uts)
{
	return host_syscall(SYS_uname, (uintptr_t)uts);
}

int
host_gettimeofday(struct host_timeval *a, void *b)
{
	return host_syscall(SYS_gettimeofday, (uintptr_t)a, (uintptr_t)b);
}

int
host_select(int nfds, long *readfds, long *writefds, long *exceptfds,
    struct host_timeval *timeout)
{
	return host_syscall(SYS_select, nfds, (uintptr_t)readfds, (uintptr_t)writefds, (uintptr_t)exceptfds, (uintptr_t)timeout, 0);
}

int
kexec_load(uint32_t start, int nsegs, uint32_t segs)
{
	return host_syscall(__NR_kexec_load, start, nsegs, segs, KEXEC_ARCH << 16);
}

int
host_reboot(int magic1, int magic2, int cmd, uintptr_t arg)
{
	return host_syscall(SYS_reboot, magic1, magic2, cmd, arg);
}

int
host_getdents(int fd, void *dirp, int count)
{
	return host_syscall(SYS_getdents, fd, (uintptr_t)dirp, count);
}
