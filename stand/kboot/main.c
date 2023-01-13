/*-
 * Copyright (C) 2010-2014 Nathan Whitehorn
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
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stand.h>
#include <sys/param.h>
#include <sys/boot.h>
#include <fdt_platform.h>

#include <machine/cpufunc.h>
#include <bootstrap.h>
#include "host_syscall.h"
#include "kboot.h"

struct arch_switch	archsw;
extern void *_end;

int kboot_getdev(void **vdev, const char *devspec, const char **path);
ssize_t kboot_copyin(const void *src, vm_offset_t dest, const size_t len);
ssize_t kboot_copyout(vm_offset_t src, void *dest, const size_t len);
ssize_t kboot_readin(readin_handle_t fd, vm_offset_t dest, const size_t len);
int kboot_autoload(void);
uint64_t kboot_loadaddr(u_int type, void *data, uint64_t addr);
static void kboot_kseg_get(int *nseg, void **ptr);
static void kboot_zfs_probe(void);

extern int command_fdt_internal(int argc, char *argv[]);

/*
 * NB: getdev should likely be identical to this most places, except maybe
 * we should move to storing the length of the platform devdesc.
 */
int
kboot_getdev(void **vdev, const char *devspec, const char **path)
{
	int rv;
	struct devdesc **dev = (struct devdesc **)vdev;

	/*
	 * If it looks like this is just a path and no device, go with the
	 * current device.
	 */
	if (devspec == NULL || strchr(devspec, ':') == NULL) {
		if (((rv = devparse(dev, getenv("currdev"), NULL)) == 0) &&
		    (path != NULL))
			*path = devspec;
		return (rv);
	}

	/*
	 * Try to parse the device name off the beginning of the devspec
	 */
	return (devparse(dev, devspec, path));
}

static int
parse_args(int argc, const char **argv)
{
	int howto = 0;

	/*
	 * When run as init, sometimes argv[0] is a EFI-ESP path, other times
	 * it's the name of the init program, and sometimes it's a placeholder
	 * string, so we exclude it here. For the other args, look for DOS-like
	 * and Unix-like absolte paths and exclude parsing it if we find that,
	 * otherwise parse it as a command arg (so looking for '-X', 'foo' or
	 * 'foo=bar'). This is a little different than EFI where it argv[0]
	 * often times is the first argument passed in. There are cases when
	 * linux-booting via EFI that we have the EFI path we used to run
	 * bootXXX.efi as the arguments to init, so we need to exclude the paths
	 * there as well.
	 */
	for (int i = 1; i < argc; i++) {
		if (argv[i][0] != '\\' && argv[i][0] != '/') {
			howto |= boot_parse_arg(argv[i]);
		}
	}

	return (howto);
}

static vm_offset_t rsdp;

static vm_offset_t
kboot_rsdp_from_efi(void)
{
	char buffer[512 + 1];
	char *walker, *ep;

	if (!file2str("/sys/firmware/efi/systab", buffer, sizeof(buffer)))
		return (0);	/* Not an EFI system */
	ep = buffer + strlen(buffer);
	walker = buffer;
	while (walker < ep) {
		if (strncmp("ACPI20=", walker, 7) == 0)
			return((vm_offset_t)strtoull(walker + 7, NULL, 0));
		if (strncmp("ACPI=", walker, 5) == 0)
			return((vm_offset_t)strtoull(walker + 5, NULL, 0));
		walker += strcspn(walker, "\n");
	}
	return (0);
}

static void
find_acpi(void)
{
	rsdp = kboot_rsdp_from_efi();
#if 0	/* maybe for amd64 */
	if (rsdp == 0)
		rsdp = find_rsdp_arch();
#endif
}

vm_offset_t
acpi_rsdp(void)
{
	return (rsdp);
}

bool
has_acpi(void)
{
	return rsdp != 0;
}

int
main(int argc, const char **argv)
{
	void *heapbase;
	const size_t heapsize = 128*1024*1024;
	const char *bootdev;

	archsw.arch_getdev = kboot_getdev;
	archsw.arch_copyin = kboot_copyin;
	archsw.arch_copyout = kboot_copyout;
	archsw.arch_readin = kboot_readin;
	archsw.arch_autoload = kboot_autoload;
	archsw.arch_loadaddr = kboot_loadaddr;
	archsw.arch_kexec_kseg_get = kboot_kseg_get;
	archsw.arch_zfs_probe = kboot_zfs_probe;

	/* Give us a sane world if we're running as init */
	do_init();

	/*
	 * Setup the heap 15MB should be plenty
	 */
	heapbase = host_getmem(heapsize);
	setheap(heapbase, heapbase + heapsize);

	/* Parse the command line args -- ignoring for now the console selection */
	parse_args(argc, argv);

	/*
	 * Set up console.
	 */
	cons_probe();

	/* Initialize all the devices */
	devinit();

	bootdev = getenv("bootdev");
	if (bootdev == NULL)
		bootdev="zfs:";
	hostfs_root = getenv("hostfs_root");
	if (hostfs_root == NULL)
		hostfs_root = "/";
#if defined(LOADER_ZFS_SUPPORT)
	if (strcmp(bootdev, "zfs:") == 0) {
		/*
		 * Pseudo device that says go find the right ZFS pool. This will be
		 * the first pool that we find that passes the sanity checks (eg looks
		 * like it might be vbootable) and sets currdev to the right thing based
		 * on active BEs, etc
		 */
		hostdisk_zfs_find_default();
	} else
#endif
	{
		/*
		 * Otherwise, honor what's on the command line. If we've been
		 * given a specific ZFS partition, then we'll honor it w/o BE
		 * processing that would otherwise pick a different snapshot to
		 * boot than the default one in the pool.
		 */
		set_currdev(bootdev);
	}

	printf("Boot device: %s with hostfs_root %s\n", bootdev, hostfs_root);

	printf("\n%s", bootprog_info);

	setenv("LINES", "24", 1);
	setenv("usefdt", "1", 1);

	/*
	 * Find acpi, if it exists
	 */
	find_acpi();

	interact();			/* doesn't return */

	return (0);
}

void
exit(int code)
{
	host_exit(code);
	__unreachable();
}

void
delay(int usecs)
{
	struct host_timeval tvi, tv;
	uint64_t ti, t;
	host_gettimeofday(&tvi, NULL);
	ti = tvi.tv_sec*1000000 + tvi.tv_usec;
	do {
		host_gettimeofday(&tv, NULL);
		t = tv.tv_sec*1000000 + tv.tv_usec;
	} while (t < ti + usecs);
}

time_t
getsecs(void)
{
	struct host_timeval tv;
	host_gettimeofday(&tv, NULL);
	return (tv.tv_sec);
}

time_t
time(time_t *tloc)
{
	time_t rv;
	
	rv = getsecs();
	if (tloc != NULL)
		*tloc = rv;

	return (rv);
}

struct host_kexec_segment loaded_segments[HOST_KEXEC_SEGMENT_MAX];
int nkexec_segments = 0;

static ssize_t
get_phys_buffer(vm_offset_t dest, const size_t len, void **buf)
{
	int i = 0;
	const size_t segsize = 8*1024*1024;

	if (nkexec_segments == HOST_KEXEC_SEGMENT_MAX)
		panic("Tried to load too many kexec segments");
	for (i = 0; i < nkexec_segments; i++) {
		if (dest >= (vm_offset_t)loaded_segments[i].mem &&
		    dest < (vm_offset_t)loaded_segments[i].mem +
		    loaded_segments[i].memsz)
			goto out;
	}

	loaded_segments[nkexec_segments].buf = host_getmem(segsize);
	loaded_segments[nkexec_segments].bufsz = segsize;
	loaded_segments[nkexec_segments].mem = (void *)rounddown2(dest,segsize);
	loaded_segments[nkexec_segments].memsz = segsize;

	i = nkexec_segments;
	nkexec_segments++;

out:
	*buf = loaded_segments[i].buf + (dest -
	    (vm_offset_t)loaded_segments[i].mem);
	return (min(len,loaded_segments[i].bufsz - (dest -
	    (vm_offset_t)loaded_segments[i].mem)));
}

ssize_t
kboot_copyin(const void *src, vm_offset_t dest, const size_t len)
{
	ssize_t segsize, remainder;
	void *destbuf;

	remainder = len;
	do {
		segsize = get_phys_buffer(dest, remainder, &destbuf);
		bcopy(src, destbuf, segsize);
		remainder -= segsize;
		src += segsize;
		dest += segsize;
	} while (remainder > 0);

	return (len);
}

ssize_t
kboot_copyout(vm_offset_t src, void *dest, const size_t len)
{
	ssize_t segsize, remainder;
	void *srcbuf;

	remainder = len;
	do {
		segsize = get_phys_buffer(src, remainder, &srcbuf);
		bcopy(srcbuf, dest, segsize);
		remainder -= segsize;
		src += segsize;
		dest += segsize;
	} while (remainder > 0);

	return (len);
}

ssize_t
kboot_readin(readin_handle_t fd, vm_offset_t dest, const size_t len)
{
	void            *buf;
	size_t          resid, chunk, get;
	ssize_t         got;
	vm_offset_t     p;

	p = dest;

	chunk = min(PAGE_SIZE, len);
	buf = malloc(chunk);
	if (buf == NULL) {
		printf("kboot_readin: buf malloc failed\n");
		return (0);
	}

	for (resid = len; resid > 0; resid -= got, p += got) {
		get = min(chunk, resid);
		got = VECTX_READ(fd, buf, get);
		if (got <= 0) {
			if (got < 0)
				printf("kboot_readin: read failed\n");
			break;
		}

		kboot_copyin(buf, p, got);
	}

	free (buf);
	return (len - resid);
}

int
kboot_autoload(void)
{

	return (0);
}

uint64_t
kboot_loadaddr(u_int type, void *data, uint64_t addr)
{

	if (type == LOAD_ELF)
		addr = roundup(addr, PAGE_SIZE);
	else
		addr += kboot_get_phys_load_segment();

	return (addr);
}

static void
kboot_kseg_get(int *nseg, void **ptr)
{
#if 0
	int a;

	for (a = 0; a < nkexec_segments; a++) {
		printf("kseg_get: %jx %jx %jx %jx\n",
			(uintmax_t)loaded_segments[a].buf,
			(uintmax_t)loaded_segments[a].bufsz,
			(uintmax_t)loaded_segments[a].mem,
			(uintmax_t)loaded_segments[a].memsz);
	}
#endif

	*nseg = nkexec_segments;
	*ptr = &loaded_segments[0];
}

static void
kboot_zfs_probe(void)
{
#if defined(LOADER_ZFS_SUPPORT)
	/*
	 * Open all the disks and partitions we can find to see if there are ZFS
	 * pools on them.
	 */
	hostdisk_zfs_probe();
#endif
}

/*
 * Since proper fdt command handling function is defined in fdt_loader_cmd.c,
 * and declaring it as extern is in contradiction with COMMAND_SET() macro
 * (which uses static pointer), we're defining wrapper function, which
 * calls the proper fdt handling routine.
 */
static int
command_fdt(int argc, char *argv[])
{

	return (command_fdt_internal(argc, argv));
}
        
COMMAND_SET(fdt, "fdt", "flattened device tree handling", command_fdt);
