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

#include <stand.h>
#include <sys/param.h>
#include <sys/boot.h>
#include <fdt_platform.h>

#include <machine/cpufunc.h>
#include <bootstrap.h>
#include "host_syscall.h"
#include "kboot.h"
#include "stand.h"
#include <smbios.h>

struct arch_switch	archsw;
extern void *_end;

int kboot_getdev(void **vdev, const char *devspec, const char **path);
ssize_t kboot_copyin(const void *src, vm_offset_t dest, const size_t len);
ssize_t kboot_copyout(vm_offset_t src, void *dest, const size_t len);
ssize_t kboot_readin(readin_handle_t fd, vm_offset_t dest, const size_t len);
int kboot_autoload(void);
static void kboot_zfs_probe(void);

extern int command_fdt_internal(int argc, char *argv[]);

#define PA_INVAL (vm_offset_t)-1
static vm_offset_t pa_start = PA_INVAL;
static vm_offset_t padding;
static vm_offset_t offset;

static uint64_t commit_limit;
static uint64_t committed_as;
static uint64_t mem_avail;

static void
memory_limits(void)
{
	int fd;
	char buf[128];

	/*
	 * To properly size the slabs, we need to find how much memory we can
	 * commit to using. commit_limit is the max, while commited_as is the
	 * current total. We can use these later to allocate the largetst amount
	 * of memory possible so we can support larger ram disks than we could
	 * by using fixed segment sizes. We also grab the memory available so
	 * we don't use more than 49% of that.
	 */
	fd = open("host:/proc/meminfo", O_RDONLY);
	if (fd != -1) {
		while (fgetstr(buf, sizeof(buf), fd) > 0) {
			if (strncmp(buf, "MemAvailable:", 13) == 0) {
				mem_avail = strtoll(buf + 13, NULL, 0);
				mem_avail <<= 10; /* Units are kB */
			} else if (strncmp(buf, "CommitLimit:", 12) == 0) {
				commit_limit = strtoll(buf + 13, NULL, 0);
				commit_limit <<= 10; /* Units are kB */
			} else if (strncmp(buf, "Committed_AS:", 13) == 0) {
				committed_as = strtoll(buf + 14, NULL, 0);
				committed_as <<= 10; /* Units are kB */
			}
		}
	} else {
		/* Otherwise, on FreeBSD host, for testing 32GB host: */
		mem_avail = 31ul << 30;			/* 31GB free */
		commit_limit = mem_avail * 9 / 10;	/* 90% comittable */
		committed_as = 20ul << 20;		/* 20MB used */
	}
	printf("Commit limit: %lld Committed bytes %lld Available %lld\n",
	    (long long)commit_limit, (long long)committed_as,
	    (long long)mem_avail);
	close(fd);
}

/*
 * NB: getdev should likely be identical to this most places, except maybe
 * we should move to storing the length of the platform devdesc.
 */
int
kboot_getdev(void **vdev, const char *devspec, const char **path)
{
	struct devdesc **dev = (struct devdesc **)vdev;
	int				rv;

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
		walker += strcspn(walker, "\n") + 1;
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

/*
 * SMBIOS support. We map the physical memory address we get into a VA in this
 * address space with mmap with 64k pages. Once we're done, we cleanup any
 * mappings we made.
 */

#define MAX_MAP	10
#define PAGE	(64<<10)

static struct mapping
{
	uintptr_t pa;
	caddr_t va;
} map[MAX_MAP];
static int smbios_fd;
static int nmap;

caddr_t ptov(uintptr_t pa)
{
	caddr_t va;
	uintptr_t pa2;
	struct mapping *m = map;

	pa2 = rounddown(pa, PAGE);
	for (int i = 0; i < nmap; i++, m++) {
		if (m->pa == pa2) {
			return (m->va + pa - m->pa);
		}
	}
	if (nmap == MAX_MAP)
		panic("Too many maps for smbios");

	/*
	 * host_mmap returns small negative numbers on errors, can't return an
	 * error here, so we have to panic. The Linux wrapper will set errno
	 * based on this and then return HOST_MAP_FAILED. Since we're calling
	 * the raw system call we have to do that ourselves.
	 */
	va = host_mmap(0, PAGE, HOST_PROT_READ, HOST_MAP_SHARED, smbios_fd, pa2);
	if (is_linux_error((long)va))
		panic("smbios mmap offset %#jx failed", (uintmax_t)pa2);
	m = &map[nmap++];
	m->pa = pa2;
	m->va = va;
	return (m->va + pa - m->pa);
}

static void
smbios_cleanup(void)
{
	for (int i = 0; i < nmap; i++) {
		host_munmap(map[i].va, PAGE);
	}
}

static vm_offset_t
kboot_find_smbios(void)
{
	char buffer[512 + 1];
	char *walker, *ep;

	if (!file2str("/sys/firmware/efi/systab", buffer, sizeof(buffer)))
		return (0);	/* Not an EFI system */
	ep = buffer + strlen(buffer);
	walker = buffer;
	while (walker <= ep) {
		if (strncmp("SMBIOS3=", walker, 8) == 0)
			return((vm_offset_t)strtoull(walker + 8, NULL, 0));
		if (strncmp("SMBIOS=", walker, 7) == 0)
			return((vm_offset_t)strtoull(walker + 7, NULL, 0));
		walker += strcspn(walker, "\n") + 1;
	}
	return (0);
}

static void
find_smbios(void)
{
	char buf[40];
	uintptr_t pa;
	caddr_t va;

	pa = kboot_find_smbios();
	printf("SMBIOS at %#jx\n", (uintmax_t)pa);
	if (pa == 0)
		return;

	snprintf(buf, sizeof(buf), "%#jx", (uintmax_t)pa);
	setenv("hint.smbios.0.mem", buf, 1);
	smbios_fd = host_open("/dev/mem", O_RDONLY, 0);
	if (smbios_fd < 0) {
		printf("Can't open /dev/mem to read smbios\n");
		return;
	}
	va = ptov(pa);
	printf("Start of smbios at pa %p va %p\n", (void *)pa, va);
	smbios_detect(va);
	smbios_cleanup();
	host_close(smbios_fd);
}

static void
parse_file(const char *fn)
{
	struct stat st;
	int fd = -1;
	char *env = NULL;

	if (stat(fn, &st) != 0)
		return;
	fd = open(fn, O_RDONLY);
	if (fd == -1)
		return;
	env = malloc(st.st_size + 1);
	if (env == NULL)
		goto out;
	if (read(fd, env, st.st_size) != st.st_size)
		goto out;
	env[st.st_size] = '\0';
	boot_parse_cmdline(env);
out:
	free(env);
	close(fd);
}


int
main(int argc, const char **argv)
{
	void *heapbase;
	const size_t heapsize = 64*1024*1024;
	const char *bootdev;

	archsw.arch_getdev = kboot_getdev;
	archsw.arch_copyin = kboot_copyin;
	archsw.arch_copyout = kboot_copyout;
	archsw.arch_readin = kboot_readin;
	archsw.arch_autoload = kboot_autoload;
	archsw.arch_zfs_probe = kboot_zfs_probe;

	/* Give us a sane world if we're running as init */
	do_init();

	/*
	 * Setup the heap, 64MB is minimum for ZFS booting
	 */
	heapbase = host_getmem(heapsize);
	setheap(heapbase, heapbase + heapsize);

	/* Parse the command line args -- ignoring for now the console selection */
	parse_args(argc, argv);

	parse_file("host:/kboot.conf");

	/*
	 * Set up console.
	 */
	cons_probe();

	/* Initialize all the devices */
	devinit();

	bootdev = getenv("bootdev");
	if (bootdev == NULL)
		bootdev = hostdisk_gen_probe();
	hostfs_root = getenv("hostfs_root");
	if (hostfs_root == NULL)
		hostfs_root = "/";
#if defined(LOADER_ZFS_SUPPORT)
	if (bootdev == NULL || strcmp(bootdev, "zfs:") == 0) {
		/*
		 * Pseudo device that says go find the right ZFS pool. This will be
		 * the first pool that we find that passes the sanity checks (eg looks
		 * like it might be vbootable) and sets currdev to the right thing based
		 * on active BEs, etc
		 */
		if (hostdisk_zfs_find_default())
			bootdev = getenv("currdev");
	}
#endif
	if (bootdev != NULL) {
		/*
		 * Otherwise, honor what's on the command line. If we've been
		 * given a specific ZFS partition, then we'll honor it w/o BE
		 * processing that would otherwise pick a different snapshot to
		 * boot than the default one in the pool.
		 */
		set_currdev(bootdev);
	} else {
		panic("Bootdev is still NULL");
	}

	printf("Boot device: %s with hostfs_root %s\n", bootdev, hostfs_root);

	printf("\n%s", bootprog_info);

	setenv("LINES", "24", 1);

	memory_limits();
	enumerate_memory_arch();

	/*
	 * Find acpi, if it exists
	 */
	find_acpi();

	find_smbios();

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

#define SEGALIGN (1ul<<20)

static ssize_t
get_phys_buffer(vm_offset_t dest, const size_t len, void **buf)
{
	int i = 0;
	const size_t segsize = 64*1024*1024;
	size_t sz, amt, l;

	if (nkexec_segments == HOST_KEXEC_SEGMENT_MAX)
		panic("Tried to load too many kexec segments");
	for (i = 0; i < nkexec_segments; i++) {
		if (dest >= (vm_offset_t)loaded_segments[i].mem &&
		    dest < (vm_offset_t)loaded_segments[i].mem +
		    loaded_segments[i].bufsz) /* Need to use bufsz since memsz is in use size */
			goto out;
	}

	sz = segsize;
	if (nkexec_segments == 0) {
		/* how much space does this segment have */
		sz = space_avail(dest);
		/* Clip to 45% of available memory (need 2 copies) */
		sz = MIN(sz, rounddown2(mem_avail * 45 / 100, SEGALIGN));
		printf("limit to 45%% of mem_avail %zd\n", sz);
		/* And only use 95% of what we can allocate */
		sz = MIN(sz,
		    rounddown2((commit_limit - committed_as) * 95 / 100, SEGALIGN));
		printf("Allocating %zd MB for first segment\n", sz >> 20);
	}

	loaded_segments[nkexec_segments].buf = host_getmem(sz);
	loaded_segments[nkexec_segments].bufsz = sz;
	loaded_segments[nkexec_segments].mem = (void *)rounddown2(dest,SEGALIGN);
	loaded_segments[nkexec_segments].memsz = 0;

	i = nkexec_segments;
	nkexec_segments++;

out:
	/*
	 * Keep track of the highest amount used in a segment
	 */
	amt = dest - (vm_offset_t)loaded_segments[i].mem;
	l = min(len,loaded_segments[i].bufsz - amt);
	*buf = loaded_segments[i].buf + amt;
	if (amt + l > loaded_segments[i].memsz)
		loaded_segments[i].memsz = amt + l;
	return (l);
}

ssize_t
kboot_copyin(const void *src, vm_offset_t dest, const size_t len)
{
	ssize_t segsize, remainder;
	void *destbuf;

	if (pa_start == PA_INVAL) {
		pa_start = kboot_get_phys_load_segment();
//		padding = 2 << 20; /* XXX amd64: revisit this when we make it work */
		padding = 0;
		offset = dest;
		get_phys_buffer(pa_start, len, &destbuf);
	}

	remainder = len;
	do {
		segsize = get_phys_buffer(dest + pa_start + padding - offset, remainder, &destbuf);
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
		segsize = get_phys_buffer(src + pa_start + padding - offset, remainder, &srcbuf);
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

void
kboot_kseg_get(int *nseg, void **ptr)
{
	printf("kseg_get: %d segments\n", nkexec_segments);
	printf("VA               SZ       PA               MEMSZ\n");
	printf("---------------- -------- ---------------- -----\n");
	for (int a = 0; a < nkexec_segments; a++) {
		/*
		 * Truncate each segment to just what we've used in the segment,
		 * rounded up to the next page.
		 */
		loaded_segments[a].memsz = roundup2(loaded_segments[a].memsz,PAGE_SIZE);
		loaded_segments[a].bufsz = loaded_segments[a].memsz;
		printf("%016jx %08jx %016jx %08jx\n",
			(uintmax_t)loaded_segments[a].buf,
			(uintmax_t)loaded_segments[a].bufsz,
			(uintmax_t)loaded_segments[a].mem,
			(uintmax_t)loaded_segments[a].memsz);
	}

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

