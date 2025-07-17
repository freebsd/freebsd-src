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

#include <sys/param.h>
#include <sys/capsicum.h>
#include <sys/sysctl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/linker.h>
#include <sys/module.h>
#include <sys/_iovec.h>
#include <sys/cpuset.h>

#include <capsicum_helpers.h>
#include <err.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include <libutil.h>

#include <vm/vm.h>
#include <machine/vmm.h>
#ifdef WITH_VMMAPI_SNAPSHOT
#include <machine/vmm_snapshot.h>
#endif

#include <dev/vmm/vmm_dev.h>

#include "vmmapi.h"
#include "internal.h"

#define	MB	(1024 * 1024UL)
#define	GB	(1024 * 1024 * 1024UL)

#ifdef __amd64__
#define	VM_LOWMEM_LIMIT	(3 * GB)
#else
#define	VM_LOWMEM_LIMIT	0
#endif
#define	VM_HIGHMEM_BASE	(4 * GB)

/*
 * Size of the guard region before and after the virtual address space
 * mapping the guest physical memory. This must be a multiple of the
 * superpage size for performance reasons.
 */
#define	VM_MMAP_GUARD_SIZE	(4 * MB)

#define	PROT_RW		(PROT_READ | PROT_WRITE)
#define	PROT_ALL	(PROT_READ | PROT_WRITE | PROT_EXEC)

static int
vm_device_open(const char *name)
{
	char devpath[PATH_MAX];

	assert(strlen(name) <= VM_MAX_NAMELEN);
	(void)snprintf(devpath, sizeof(devpath), "/dev/vmm/%s", name);
	return (open(devpath, O_RDWR));
}

static int
vm_ctl_open(void)
{
	if (modfind("vmm") < 0)
		(void)kldload("vmm");
	return (open("/dev/vmmctl", O_RDWR, 0));
}

static int
vm_ctl_create(const char *name, int ctlfd)
{
	struct vmmctl_vm_create vmc;

	memset(&vmc, 0, sizeof(vmc));
	if (strlcpy(vmc.name, name, sizeof(vmc.name)) >= sizeof(vmc.name)) {
		errno = ENAMETOOLONG;
		return (-1);
	}
	return (ioctl(ctlfd, VMMCTL_VM_CREATE, &vmc));
}

int
vm_create(const char *name)
{
	int error, fd;

	fd = vm_ctl_open();
	if (fd < 0)
		return (-1);

	error = vm_ctl_create(name, fd);
	if (error != 0) {
		error = errno;
		(void)close(fd);
		errno = error;
		return (-1);
	}
	(void)close(fd);
	return (0);
}

struct vmctx *
vm_open(const char *name)
{
	return (vm_openf(name, 0));
}

struct vmctx *
vm_openf(const char *name, int flags)
{
	struct vmctx *vm;
	int saved_errno;
	bool created;

	created = false;

	vm = malloc(sizeof(struct vmctx) + strlen(name) + 1);
	assert(vm != NULL);

	vm->fd = vm->ctlfd = -1;
	vm->memflags = 0;
	vm->name = (char *)(vm + 1);
	strcpy(vm->name, name);
	memset(vm->memsegs, 0, sizeof(vm->memsegs));

	if ((vm->ctlfd = vm_ctl_open()) < 0)
		goto err;

	vm->fd = vm_device_open(vm->name);
	if (vm->fd < 0 && errno == ENOENT) {
		if (flags & VMMAPI_OPEN_CREATE) {
			if (vm_ctl_create(vm->name, vm->ctlfd) != 0)
				goto err;
			vm->fd = vm_device_open(vm->name);
			created = true;
		}
	}
	if (vm->fd < 0)
		goto err;

	if (!created && (flags & VMMAPI_OPEN_REINIT) != 0 && vm_reinit(vm) != 0)
		goto err;

	return (vm);
err:
	saved_errno = errno;
	if (created)
		vm_destroy(vm);
	else
		vm_close(vm);
	errno = saved_errno;
	return (NULL);
}

void
vm_close(struct vmctx *vm)
{
	assert(vm != NULL);

	if (vm->fd >= 0)
		(void)close(vm->fd);
	if (vm->ctlfd >= 0)
		(void)close(vm->ctlfd);
	free(vm);
}

void
vm_destroy(struct vmctx *vm)
{
	struct vmmctl_vm_destroy vmd;

	memset(&vmd, 0, sizeof(vmd));
	(void)strlcpy(vmd.name, vm->name, sizeof(vmd.name));
	if (ioctl(vm->ctlfd, VMMCTL_VM_DESTROY, &vmd) != 0)
		warn("ioctl(VMMCTL_VM_DESTROY)");

	vm_close(vm);
}

struct vcpu *
vm_vcpu_open(struct vmctx *ctx, int vcpuid)
{
	struct vcpu *vcpu;

	vcpu = malloc(sizeof(*vcpu));
	vcpu->ctx = ctx;
	vcpu->vcpuid = vcpuid;
	return (vcpu);
}

void
vm_vcpu_close(struct vcpu *vcpu)
{
	free(vcpu);
}

int
vcpu_id(struct vcpu *vcpu)
{
	return (vcpu->vcpuid);
}

int
vm_parse_memsize(const char *opt, size_t *ret_memsize)
{
	char *endptr;
	size_t optval;
	int error;

	optval = strtoul(opt, &endptr, 0);
	if (*opt != '\0' && *endptr == '\0') {
		/*
		 * For the sake of backward compatibility if the memory size
		 * specified on the command line is less than a megabyte then
		 * it is interpreted as being in units of MB.
		 */
		if (optval < MB)
			optval *= MB;
		*ret_memsize = optval;
		error = 0;
	} else
		error = expand_number(opt, ret_memsize);

	return (error);
}

uint32_t
vm_get_lowmem_limit(struct vmctx *ctx __unused)
{

	return (VM_LOWMEM_LIMIT);
}

void
vm_set_memflags(struct vmctx *ctx, int flags)
{

	ctx->memflags = flags;
}

int
vm_get_memflags(struct vmctx *ctx)
{

	return (ctx->memflags);
}

/*
 * Map segment 'segid' starting at 'off' into guest address range [gpa,gpa+len).
 */
int
vm_mmap_memseg(struct vmctx *ctx, vm_paddr_t gpa, int segid, vm_ooffset_t off,
    size_t len, int prot)
{
	struct vm_memmap memmap;
	int error, flags;

	memmap.gpa = gpa;
	memmap.segid = segid;
	memmap.segoff = off;
	memmap.len = len;
	memmap.prot = prot;
	memmap.flags = 0;

	if (ctx->memflags & VM_MEM_F_WIRED)
		memmap.flags |= VM_MEMMAP_F_WIRED;

	/*
	 * If this mapping already exists then don't create it again. This
	 * is the common case for SYSMEM mappings created by bhyveload(8).
	 */
	error = vm_mmap_getnext(ctx, &gpa, &segid, &off, &len, &prot, &flags);
	if (error == 0 && gpa == memmap.gpa) {
		if (segid != memmap.segid || off != memmap.segoff ||
		    prot != memmap.prot || flags != memmap.flags) {
			errno = EEXIST;
			return (-1);
		} else {
			return (0);
		}
	}

	error = ioctl(ctx->fd, VM_MMAP_MEMSEG, &memmap);
	return (error);
}

int
vm_get_guestmem_from_ctx(struct vmctx *ctx, char **guest_baseaddr,
    size_t *lowmem_size, size_t *highmem_size)
{

	*guest_baseaddr = ctx->baseaddr;
	*lowmem_size = ctx->memsegs[VM_MEMSEG_LOW].size;
	*highmem_size = ctx->memsegs[VM_MEMSEG_HIGH].size;
	return (0);
}

int
vm_munmap_memseg(struct vmctx *ctx, vm_paddr_t gpa, size_t len)
{
	struct vm_munmap munmap;
	int error;

	munmap.gpa = gpa;
	munmap.len = len;

	error = ioctl(ctx->fd, VM_MUNMAP_MEMSEG, &munmap);
	return (error);
}

int
vm_mmap_getnext(struct vmctx *ctx, vm_paddr_t *gpa, int *segid,
    vm_ooffset_t *segoff, size_t *len, int *prot, int *flags)
{
	struct vm_memmap memmap;
	int error;

	bzero(&memmap, sizeof(struct vm_memmap));
	memmap.gpa = *gpa;
	error = ioctl(ctx->fd, VM_MMAP_GETNEXT, &memmap);
	if (error == 0) {
		*gpa = memmap.gpa;
		*segid = memmap.segid;
		*segoff = memmap.segoff;
		*len = memmap.len;
		*prot = memmap.prot;
		*flags = memmap.flags;
	}
	return (error);
}

/*
 * Return 0 if the segments are identical and non-zero otherwise.
 *
 * This is slightly complicated by the fact that only device memory segments
 * are named.
 */
static int
cmpseg(size_t len, const char *str, size_t len2, const char *str2)
{

	if (len == len2) {
		if ((!str && !str2) || (str && str2 && !strcmp(str, str2)))
			return (0);
	}
	return (-1);
}

static int
vm_alloc_memseg(struct vmctx *ctx, int segid, size_t len, const char *name)
{
	struct vm_memseg memseg;
	size_t n;
	int error;

	/*
	 * If the memory segment has already been created then just return.
	 * This is the usual case for the SYSMEM segment created by userspace
	 * loaders like bhyveload(8).
	 */
	error = vm_get_memseg(ctx, segid, &memseg.len, memseg.name,
	    sizeof(memseg.name));
	if (error)
		return (error);

	if (memseg.len != 0) {
		if (cmpseg(len, name, memseg.len, VM_MEMSEG_NAME(&memseg))) {
			errno = EINVAL;
			return (-1);
		} else {
			return (0);
		}
	}

	bzero(&memseg, sizeof(struct vm_memseg));
	memseg.segid = segid;
	memseg.len = len;
	if (name != NULL) {
		n = strlcpy(memseg.name, name, sizeof(memseg.name));
		if (n >= sizeof(memseg.name)) {
			errno = ENAMETOOLONG;
			return (-1);
		}
	}

	error = ioctl(ctx->fd, VM_ALLOC_MEMSEG, &memseg);
	return (error);
}

int
vm_get_memseg(struct vmctx *ctx, int segid, size_t *lenp, char *namebuf,
    size_t bufsize)
{
	struct vm_memseg memseg;
	size_t n;
	int error;

	bzero(&memseg, sizeof(memseg));
	memseg.segid = segid;
	error = ioctl(ctx->fd, VM_GET_MEMSEG, &memseg);
	if (error == 0) {
		*lenp = memseg.len;
		n = strlcpy(namebuf, memseg.name, bufsize);
		if (n >= bufsize) {
			errno = ENAMETOOLONG;
			error = -1;
		}
	}
	return (error);
}

static int
setup_memory_segment(struct vmctx *ctx, vm_paddr_t gpa, size_t len, char *base)
{
	char *ptr;
	int error, flags;

	/* Map 'len' bytes starting at 'gpa' in the guest address space */
	error = vm_mmap_memseg(ctx, gpa, VM_SYSMEM, gpa, len, PROT_ALL);
	if (error)
		return (error);

	flags = MAP_SHARED | MAP_FIXED;
	if ((ctx->memflags & VM_MEM_F_INCORE) == 0)
		flags |= MAP_NOCORE;

	/* mmap into the process address space on the host */
	ptr = mmap(base + gpa, len, PROT_RW, flags, ctx->fd, gpa);
	if (ptr == MAP_FAILED)
		return (-1);

	return (0);
}

int
vm_setup_memory(struct vmctx *ctx, size_t memsize, enum vm_mmap_style vms)
{
	size_t objsize, len;
	vm_paddr_t gpa;
	char *baseaddr, *ptr;
	int error;

	assert(vms == VM_MMAP_ALL);

	/*
	 * If 'memsize' cannot fit entirely in the 'lowmem' segment then create
	 * another 'highmem' segment above VM_HIGHMEM_BASE for the remainder.
	 */
	if (memsize > VM_LOWMEM_LIMIT) {
		ctx->memsegs[VM_MEMSEG_LOW].size = VM_LOWMEM_LIMIT;
		ctx->memsegs[VM_MEMSEG_HIGH].size = memsize - VM_LOWMEM_LIMIT;
		objsize = VM_HIGHMEM_BASE + ctx->memsegs[VM_MEMSEG_HIGH].size;
	} else {
		ctx->memsegs[VM_MEMSEG_LOW].size = memsize;
		ctx->memsegs[VM_MEMSEG_HIGH].size = 0;
		objsize = memsize;
	}

	error = vm_alloc_memseg(ctx, VM_SYSMEM, objsize, NULL);
	if (error)
		return (error);

	/*
	 * Stake out a contiguous region covering the guest physical memory
	 * and the adjoining guard regions.
	 */
	len = VM_MMAP_GUARD_SIZE + objsize + VM_MMAP_GUARD_SIZE;
	ptr = mmap(NULL, len, PROT_NONE, MAP_GUARD | MAP_ALIGNED_SUPER, -1, 0);
	if (ptr == MAP_FAILED)
		return (-1);

	baseaddr = ptr + VM_MMAP_GUARD_SIZE;
	if (ctx->memsegs[VM_MEMSEG_HIGH].size > 0) {
		gpa = VM_HIGHMEM_BASE;
		len = ctx->memsegs[VM_MEMSEG_HIGH].size;
		error = setup_memory_segment(ctx, gpa, len, baseaddr);
		if (error)
			return (error);
	}

	if (ctx->memsegs[VM_MEMSEG_LOW].size > 0) {
		gpa = 0;
		len = ctx->memsegs[VM_MEMSEG_LOW].size;
		error = setup_memory_segment(ctx, gpa, len, baseaddr);
		if (error)
			return (error);
	}

	ctx->baseaddr = baseaddr;

	return (0);
}

/*
 * Returns a non-NULL pointer if [gaddr, gaddr+len) is entirely contained in
 * the lowmem or highmem regions.
 *
 * In particular return NULL if [gaddr, gaddr+len) falls in guest MMIO region.
 * The instruction emulation code depends on this behavior.
 */
void *
vm_map_gpa(struct vmctx *ctx, vm_paddr_t gaddr, size_t len)
{
	vm_size_t lowsize, highsize;

	lowsize = ctx->memsegs[VM_MEMSEG_LOW].size;
	if (lowsize > 0) {
		if (gaddr < lowsize && len <= lowsize && gaddr + len <= lowsize)
			return (ctx->baseaddr + gaddr);
	}

	highsize = ctx->memsegs[VM_MEMSEG_HIGH].size;
	if (highsize > 0 && gaddr >= VM_HIGHMEM_BASE) {
		if (gaddr < VM_HIGHMEM_BASE + highsize && len <= highsize &&
		    gaddr + len <= VM_HIGHMEM_BASE + highsize)
			return (ctx->baseaddr + gaddr);
	}

	return (NULL);
}

vm_paddr_t
vm_rev_map_gpa(struct vmctx *ctx, void *addr)
{
	vm_paddr_t offaddr;
	vm_size_t lowsize, highsize;

	offaddr = (char *)addr - ctx->baseaddr;

	lowsize = ctx->memsegs[VM_MEMSEG_LOW].size;
	if (lowsize > 0)
		if (offaddr <= lowsize)
			return (offaddr);

	highsize = ctx->memsegs[VM_MEMSEG_HIGH].size;
	if (highsize > 0)
		if (offaddr >= VM_HIGHMEM_BASE &&
		    offaddr < VM_HIGHMEM_BASE + highsize)
			return (offaddr);

	return ((vm_paddr_t)-1);
}

const char *
vm_get_name(struct vmctx *ctx)
{

	return (ctx->name);
}

size_t
vm_get_lowmem_size(struct vmctx *ctx)
{

	return (ctx->memsegs[VM_MEMSEG_LOW].size);
}

vm_paddr_t
vm_get_highmem_base(struct vmctx *ctx __unused)
{

	return (VM_HIGHMEM_BASE);
}

size_t
vm_get_highmem_size(struct vmctx *ctx)
{

	return (ctx->memsegs[VM_MEMSEG_HIGH].size);
}

void *
vm_create_devmem(struct vmctx *ctx, int segid, const char *name, size_t len)
{
	char pathname[MAXPATHLEN];
	size_t len2;
	char *base, *ptr;
	int fd, error, flags;

	fd = -1;
	ptr = MAP_FAILED;
	if (name == NULL || strlen(name) == 0) {
		errno = EINVAL;
		goto done;
	}

	error = vm_alloc_memseg(ctx, segid, len, name);
	if (error)
		goto done;

	strlcpy(pathname, "/dev/vmm.io/", sizeof(pathname));
	strlcat(pathname, ctx->name, sizeof(pathname));
	strlcat(pathname, ".", sizeof(pathname));
	strlcat(pathname, name, sizeof(pathname));

	fd = open(pathname, O_RDWR);
	if (fd < 0)
		goto done;

	/*
	 * Stake out a contiguous region covering the device memory and the
	 * adjoining guard regions.
	 */
	len2 = VM_MMAP_GUARD_SIZE + len + VM_MMAP_GUARD_SIZE;
	base = mmap(NULL, len2, PROT_NONE, MAP_GUARD | MAP_ALIGNED_SUPER, -1,
	    0);
	if (base == MAP_FAILED)
		goto done;

	flags = MAP_SHARED | MAP_FIXED;
	if ((ctx->memflags & VM_MEM_F_INCORE) == 0)
		flags |= MAP_NOCORE;

	/* mmap the devmem region in the host address space */
	ptr = mmap(base + VM_MMAP_GUARD_SIZE, len, PROT_RW, flags, fd, 0);
done:
	if (fd >= 0)
		close(fd);
	return (ptr);
}

int
vcpu_ioctl(struct vcpu *vcpu, u_long cmd, void *arg)
{
	/*
	 * XXX: fragile, handle with care
	 * Assumes that the first field of the ioctl data
	 * is the vcpuid.
	 */
	*(int *)arg = vcpu->vcpuid;
	return (ioctl(vcpu->ctx->fd, cmd, arg));
}

int
vm_set_register(struct vcpu *vcpu, int reg, uint64_t val)
{
	int error;
	struct vm_register vmreg;

	bzero(&vmreg, sizeof(vmreg));
	vmreg.regnum = reg;
	vmreg.regval = val;

	error = vcpu_ioctl(vcpu, VM_SET_REGISTER, &vmreg);
	return (error);
}

int
vm_get_register(struct vcpu *vcpu, int reg, uint64_t *ret_val)
{
	int error;
	struct vm_register vmreg;

	bzero(&vmreg, sizeof(vmreg));
	vmreg.regnum = reg;

	error = vcpu_ioctl(vcpu, VM_GET_REGISTER, &vmreg);
	*ret_val = vmreg.regval;
	return (error);
}

int
vm_set_register_set(struct vcpu *vcpu, unsigned int count,
    const int *regnums, uint64_t *regvals)
{
	int error;
	struct vm_register_set vmregset;

	bzero(&vmregset, sizeof(vmregset));
	vmregset.count = count;
	vmregset.regnums = regnums;
	vmregset.regvals = regvals;

	error = vcpu_ioctl(vcpu, VM_SET_REGISTER_SET, &vmregset);
	return (error);
}

int
vm_get_register_set(struct vcpu *vcpu, unsigned int count,
    const int *regnums, uint64_t *regvals)
{
	int error;
	struct vm_register_set vmregset;

	bzero(&vmregset, sizeof(vmregset));
	vmregset.count = count;
	vmregset.regnums = regnums;
	vmregset.regvals = regvals;

	error = vcpu_ioctl(vcpu, VM_GET_REGISTER_SET, &vmregset);
	return (error);
}

int
vm_run(struct vcpu *vcpu, struct vm_run *vmrun)
{
	return (vcpu_ioctl(vcpu, VM_RUN, vmrun));
}

int
vm_suspend(struct vmctx *ctx, enum vm_suspend_how how)
{
	struct vm_suspend vmsuspend;

	bzero(&vmsuspend, sizeof(vmsuspend));
	vmsuspend.how = how;
	return (ioctl(ctx->fd, VM_SUSPEND, &vmsuspend));
}

int
vm_reinit(struct vmctx *ctx)
{

	return (ioctl(ctx->fd, VM_REINIT, 0));
}

int
vm_capability_name2type(const char *capname)
{
	int i;

	for (i = 0; i < VM_CAP_MAX; i++) {
		if (vm_capstrmap[i] != NULL &&
		    strcmp(vm_capstrmap[i], capname) == 0)
			return (i);
	}

	return (-1);
}

const char *
vm_capability_type2name(int type)
{
	if (type >= 0 && type < VM_CAP_MAX)
		return (vm_capstrmap[type]);

	return (NULL);
}

int
vm_get_capability(struct vcpu *vcpu, enum vm_cap_type cap, int *retval)
{
	int error;
	struct vm_capability vmcap;

	bzero(&vmcap, sizeof(vmcap));
	vmcap.captype = cap;

	error = vcpu_ioctl(vcpu, VM_GET_CAPABILITY, &vmcap);
	*retval = vmcap.capval;
	return (error);
}

int
vm_set_capability(struct vcpu *vcpu, enum vm_cap_type cap, int val)
{
	struct vm_capability vmcap;

	bzero(&vmcap, sizeof(vmcap));
	vmcap.captype = cap;
	vmcap.capval = val;

	return (vcpu_ioctl(vcpu, VM_SET_CAPABILITY, &vmcap));
}

uint64_t *
vm_get_stats(struct vcpu *vcpu, struct timeval *ret_tv,
	     int *ret_entries)
{
	static _Thread_local uint64_t *stats_buf;
	static _Thread_local u_int stats_count;
	uint64_t *new_stats;
	struct vm_stats vmstats;
	u_int count, index;
	bool have_stats;

	have_stats = false;
	count = 0;
	for (index = 0;; index += nitems(vmstats.statbuf)) {
		vmstats.index = index;
		if (vcpu_ioctl(vcpu, VM_STATS, &vmstats) != 0)
			break;
		if (stats_count < index + vmstats.num_entries) {
			new_stats = realloc(stats_buf,
			    (index + vmstats.num_entries) * sizeof(uint64_t));
			if (new_stats == NULL) {
				errno = ENOMEM;
				return (NULL);
			}
			stats_count = index + vmstats.num_entries;
			stats_buf = new_stats;
		}
		memcpy(stats_buf + index, vmstats.statbuf,
		    vmstats.num_entries * sizeof(uint64_t));
		count += vmstats.num_entries;
		have_stats = true;

		if (vmstats.num_entries != nitems(vmstats.statbuf))
			break;
	}
	if (have_stats) {
		if (ret_entries)
			*ret_entries = count;
		if (ret_tv)
			*ret_tv = vmstats.tv;
		return (stats_buf);
	} else
		return (NULL);
}

const char *
vm_get_stat_desc(struct vmctx *ctx, int index)
{
	static struct vm_stat_desc statdesc;

	statdesc.index = index;
	if (ioctl(ctx->fd, VM_STAT_DESC, &statdesc) == 0)
		return (statdesc.desc);
	else
		return (NULL);
}

#ifdef __amd64__
int
vm_get_gpa_pmap(struct vmctx *ctx, uint64_t gpa, uint64_t *pte, int *num)
{
	int error, i;
	struct vm_gpa_pte gpapte;

	bzero(&gpapte, sizeof(gpapte));
	gpapte.gpa = gpa;

	error = ioctl(ctx->fd, VM_GET_GPA_PMAP, &gpapte);

	if (error == 0) {
		*num = gpapte.ptenum;
		for (i = 0; i < gpapte.ptenum; i++)
			pte[i] = gpapte.pte[i];
	}

	return (error);
}

int
vm_gla2gpa(struct vcpu *vcpu, struct vm_guest_paging *paging,
    uint64_t gla, int prot, uint64_t *gpa, int *fault)
{
	struct vm_gla2gpa gg;
	int error;

	bzero(&gg, sizeof(struct vm_gla2gpa));
	gg.prot = prot;
	gg.gla = gla;
	gg.paging = *paging;

	error = vcpu_ioctl(vcpu, VM_GLA2GPA, &gg);
	if (error == 0) {
		*fault = gg.fault;
		*gpa = gg.gpa;
	}
	return (error);
}
#endif

int
vm_gla2gpa_nofault(struct vcpu *vcpu, struct vm_guest_paging *paging,
    uint64_t gla, int prot, uint64_t *gpa, int *fault)
{
	struct vm_gla2gpa gg;
	int error;

	bzero(&gg, sizeof(struct vm_gla2gpa));
	gg.prot = prot;
	gg.gla = gla;
	gg.paging = *paging;

	error = vcpu_ioctl(vcpu, VM_GLA2GPA_NOFAULT, &gg);
	if (error == 0) {
		*fault = gg.fault;
		*gpa = gg.gpa;
	}
	return (error);
}

#ifndef min
#define	min(a,b)	(((a) < (b)) ? (a) : (b))
#endif

#ifdef __amd64__
int
vm_copy_setup(struct vcpu *vcpu, struct vm_guest_paging *paging,
    uint64_t gla, size_t len, int prot, struct iovec *iov, int iovcnt,
    int *fault)
{
	void *va;
	uint64_t gpa, off;
	int error, i, n;

	for (i = 0; i < iovcnt; i++) {
		iov[i].iov_base = 0;
		iov[i].iov_len = 0;
	}

	while (len) {
		assert(iovcnt > 0);
		error = vm_gla2gpa(vcpu, paging, gla, prot, &gpa, fault);
		if (error || *fault)
			return (error);

		off = gpa & PAGE_MASK;
		n = MIN(len, PAGE_SIZE - off);

		va = vm_map_gpa(vcpu->ctx, gpa, n);
		if (va == NULL)
			return (EFAULT);

		iov->iov_base = va;
		iov->iov_len = n;
		iov++;
		iovcnt--;

		gla += n;
		len -= n;
	}
	return (0);
}
#endif

void
vm_copy_teardown(struct iovec *iov __unused, int iovcnt __unused)
{
	/*
	 * Intentionally empty.  This is used by the instruction
	 * emulation code shared with the kernel.  The in-kernel
	 * version of this is non-empty.
	 */
}

void
vm_copyin(struct iovec *iov, void *vp, size_t len)
{
	const char *src;
	char *dst;
	size_t n;

	dst = vp;
	while (len) {
		assert(iov->iov_len);
		n = min(len, iov->iov_len);
		src = iov->iov_base;
		bcopy(src, dst, n);

		iov++;
		dst += n;
		len -= n;
	}
}

void
vm_copyout(const void *vp, struct iovec *iov, size_t len)
{
	const char *src;
	char *dst;
	size_t n;

	src = vp;
	while (len) {
		assert(iov->iov_len);
		n = min(len, iov->iov_len);
		dst = iov->iov_base;
		bcopy(src, dst, n);

		iov++;
		src += n;
		len -= n;
	}
}

static int
vm_get_cpus(struct vmctx *ctx, int which, cpuset_t *cpus)
{
	struct vm_cpuset vm_cpuset;
	int error;

	bzero(&vm_cpuset, sizeof(struct vm_cpuset));
	vm_cpuset.which = which;
	vm_cpuset.cpusetsize = sizeof(cpuset_t);
	vm_cpuset.cpus = cpus;

	error = ioctl(ctx->fd, VM_GET_CPUS, &vm_cpuset);
	return (error);
}

int
vm_active_cpus(struct vmctx *ctx, cpuset_t *cpus)
{

	return (vm_get_cpus(ctx, VM_ACTIVE_CPUS, cpus));
}

int
vm_suspended_cpus(struct vmctx *ctx, cpuset_t *cpus)
{

	return (vm_get_cpus(ctx, VM_SUSPENDED_CPUS, cpus));
}

int
vm_debug_cpus(struct vmctx *ctx, cpuset_t *cpus)
{

	return (vm_get_cpus(ctx, VM_DEBUG_CPUS, cpus));
}

int
vm_activate_cpu(struct vcpu *vcpu)
{
	struct vm_activate_cpu ac;
	int error;

	bzero(&ac, sizeof(struct vm_activate_cpu));
	error = vcpu_ioctl(vcpu, VM_ACTIVATE_CPU, &ac);
	return (error);
}

int
vm_suspend_all_cpus(struct vmctx *ctx)
{
	struct vm_activate_cpu ac;
	int error;

	bzero(&ac, sizeof(struct vm_activate_cpu));
	ac.vcpuid = -1;
	error = ioctl(ctx->fd, VM_SUSPEND_CPU, &ac);
	return (error);
}

int
vm_suspend_cpu(struct vcpu *vcpu)
{
	struct vm_activate_cpu ac;
	int error;

	bzero(&ac, sizeof(struct vm_activate_cpu));
	error = vcpu_ioctl(vcpu, VM_SUSPEND_CPU, &ac);
	return (error);
}

int
vm_resume_cpu(struct vcpu *vcpu)
{
	struct vm_activate_cpu ac;
	int error;

	bzero(&ac, sizeof(struct vm_activate_cpu));
	error = vcpu_ioctl(vcpu, VM_RESUME_CPU, &ac);
	return (error);
}

int
vm_resume_all_cpus(struct vmctx *ctx)
{
	struct vm_activate_cpu ac;
	int error;

	bzero(&ac, sizeof(struct vm_activate_cpu));
	ac.vcpuid = -1;
	error = ioctl(ctx->fd, VM_RESUME_CPU, &ac);
	return (error);
}

#ifdef __amd64__
int
vm_get_intinfo(struct vcpu *vcpu, uint64_t *info1, uint64_t *info2)
{
	struct vm_intinfo vmii;
	int error;

	bzero(&vmii, sizeof(struct vm_intinfo));
	error = vcpu_ioctl(vcpu, VM_GET_INTINFO, &vmii);
	if (error == 0) {
		*info1 = vmii.info1;
		*info2 = vmii.info2;
	}
	return (error);
}

int
vm_set_intinfo(struct vcpu *vcpu, uint64_t info1)
{
	struct vm_intinfo vmii;
	int error;

	bzero(&vmii, sizeof(struct vm_intinfo));
	vmii.info1 = info1;
	error = vcpu_ioctl(vcpu, VM_SET_INTINFO, &vmii);
	return (error);
}
#endif

#ifdef WITH_VMMAPI_SNAPSHOT
int
vm_restart_instruction(struct vcpu *vcpu)
{
	int arg;

	return (vcpu_ioctl(vcpu, VM_RESTART_INSTRUCTION, &arg));
}

int
vm_snapshot_req(struct vmctx *ctx, struct vm_snapshot_meta *meta)
{

	if (ioctl(ctx->fd, VM_SNAPSHOT_REQ, meta) == -1) {
#ifdef SNAPSHOT_DEBUG
		fprintf(stderr, "%s: snapshot failed for %s: %d\r\n",
		    __func__, meta->dev_name, errno);
#endif
		return (-1);
	}
	return (0);
}

int
vm_restore_time(struct vmctx *ctx)
{
	int dummy;

	dummy = 0;
	return (ioctl(ctx->fd, VM_RESTORE_TIME, &dummy));
}
#endif

int
vm_set_topology(struct vmctx *ctx,
    uint16_t sockets, uint16_t cores, uint16_t threads, uint16_t maxcpus)
{
	struct vm_cpu_topology topology;

	bzero(&topology, sizeof (struct vm_cpu_topology));
	topology.sockets = sockets;
	topology.cores = cores;
	topology.threads = threads;
	topology.maxcpus = maxcpus;
	return (ioctl(ctx->fd, VM_SET_TOPOLOGY, &topology));
}

int
vm_get_topology(struct vmctx *ctx,
    uint16_t *sockets, uint16_t *cores, uint16_t *threads, uint16_t *maxcpus)
{
	struct vm_cpu_topology topology;
	int error;

	bzero(&topology, sizeof (struct vm_cpu_topology));
	error = ioctl(ctx->fd, VM_GET_TOPOLOGY, &topology);
	if (error == 0) {
		*sockets = topology.sockets;
		*cores = topology.cores;
		*threads = topology.threads;
		*maxcpus = topology.maxcpus;
	}
	return (error);
}

int
vm_limit_rights(struct vmctx *ctx)
{
	cap_rights_t rights;

	cap_rights_init(&rights, CAP_IOCTL, CAP_MMAP_RW);
	if (caph_rights_limit(ctx->fd, &rights) != 0)
		return (-1);
	if (caph_ioctls_limit(ctx->fd, vm_ioctl_cmds, vm_ioctl_ncmds) != 0)
		return (-1);
	return (0);
}

/*
 * Avoid using in new code.  Operations on the fd should be wrapped here so that
 * capability rights can be kept in sync.
 */
int
vm_get_device_fd(struct vmctx *ctx)
{

	return (ctx->fd);
}

/* Legacy interface, do not use. */
const cap_ioctl_t *
vm_get_ioctls(size_t *len)
{
	cap_ioctl_t *cmds;
	size_t sz;

	if (len == NULL) {
		sz = vm_ioctl_ncmds * sizeof(vm_ioctl_cmds[0]);
		cmds = malloc(sz);
		if (cmds == NULL)
			return (NULL);
		bcopy(vm_ioctl_cmds, cmds, sz);
		return (cmds);
	}

	*len = vm_ioctl_ncmds;
	return (NULL);
}
