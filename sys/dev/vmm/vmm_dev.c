/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2011 NetApp, Inc.
 * Copyright (C) 2015 Mihai Carabas <mihai.carabas@gmail.com>
 * All rights reserved.
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/ioccom.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/ucred.h>
#include <sys/uio.h>

#include <machine/vmm.h>

#include <vm/vm.h>
#include <vm/vm_object.h>

#include <dev/vmm/vmm_dev.h>
#include <dev/vmm/vmm_mem.h>
#include <dev/vmm/vmm_stat.h>

#ifdef __amd64__
#ifdef COMPAT_FREEBSD12
struct vm_memseg_12 {
	int		segid;
	size_t		len;
	char		name[64];
};
_Static_assert(sizeof(struct vm_memseg_12) == 80, "COMPAT_FREEBSD12 ABI");

#define	VM_ALLOC_MEMSEG_12	\
	_IOW('v', IOCNUM_ALLOC_MEMSEG, struct vm_memseg_12)
#define	VM_GET_MEMSEG_12	\
	_IOWR('v', IOCNUM_GET_MEMSEG, struct vm_memseg_12)
#endif /* COMPAT_FREEBSD12 */
#ifdef COMPAT_FREEBSD14
struct vm_memseg_14 {
	int		segid;
	size_t		len;
	char		name[VM_MAX_SUFFIXLEN + 1];
};
_Static_assert(sizeof(struct vm_memseg_14) == (VM_MAX_SUFFIXLEN + 1 + 16),
    "COMPAT_FREEBSD14 ABI");

#define	VM_ALLOC_MEMSEG_14	\
	_IOW('v', IOCNUM_ALLOC_MEMSEG, struct vm_memseg_14)
#define	VM_GET_MEMSEG_14	\
	_IOWR('v', IOCNUM_GET_MEMSEG, struct vm_memseg_14)
#endif /* COMPAT_FREEBSD14 */
#endif /* __amd64__ */

struct devmem_softc {
	int	segid;
	char	*name;
	struct cdev *cdev;
	struct vmmdev_softc *sc;
	SLIST_ENTRY(devmem_softc) link;
};

struct vmmdev_softc {
	struct vm	*vm;		/* vm instance cookie */
	struct cdev	*cdev;
	struct ucred	*ucred;
	SLIST_ENTRY(vmmdev_softc) link;
	SLIST_HEAD(, devmem_softc) devmem;
	int		flags;
};

static SLIST_HEAD(, vmmdev_softc) head;

static unsigned pr_allow_flag;
static struct sx vmmdev_mtx;
SX_SYSINIT(vmmdev_mtx, &vmmdev_mtx, "vmm device mutex");

static MALLOC_DEFINE(M_VMMDEV, "vmmdev", "vmmdev");

SYSCTL_DECL(_hw_vmm);

static void devmem_destroy(void *arg);
static int devmem_create_cdev(struct vmmdev_softc *sc, int id, char *devmem);

static int
vmm_priv_check(struct ucred *ucred)
{
	if (jailed(ucred) &&
	    !(ucred->cr_prison->pr_allow & pr_allow_flag))
		return (EPERM);

	return (0);
}

static int
vcpu_lock_one(struct vcpu *vcpu)
{
	return (vcpu_set_state(vcpu, VCPU_FROZEN, true));
}

static void
vcpu_unlock_one(struct vcpu *vcpu)
{
	enum vcpu_state state;

	state = vcpu_get_state(vcpu, NULL);
	if (state != VCPU_FROZEN) {
		panic("vcpu %s(%d) has invalid state %d",
		    vm_name(vcpu_vm(vcpu)), vcpu_vcpuid(vcpu), state);
	}

	vcpu_set_state(vcpu, VCPU_IDLE, false);
}

static int
vcpu_lock_all(struct vmmdev_softc *sc)
{
	struct vcpu *vcpu;
	int error;
	uint16_t i, j, maxcpus;

	error = 0;
	vm_slock_vcpus(sc->vm);
	maxcpus = vm_get_maxcpus(sc->vm);
	for (i = 0; i < maxcpus; i++) {
		vcpu = vm_vcpu(sc->vm, i);
		if (vcpu == NULL)
			continue;
		error = vcpu_lock_one(vcpu);
		if (error)
			break;
	}

	if (error) {
		for (j = 0; j < i; j++) {
			vcpu = vm_vcpu(sc->vm, j);
			if (vcpu == NULL)
				continue;
			vcpu_unlock_one(vcpu);
		}
		vm_unlock_vcpus(sc->vm);
	}

	return (error);
}

static void
vcpu_unlock_all(struct vmmdev_softc *sc)
{
	struct vcpu *vcpu;
	uint16_t i, maxcpus;

	maxcpus = vm_get_maxcpus(sc->vm);
	for (i = 0; i < maxcpus; i++) {
		vcpu = vm_vcpu(sc->vm, i);
		if (vcpu == NULL)
			continue;
		vcpu_unlock_one(vcpu);
	}
	vm_unlock_vcpus(sc->vm);
}

static struct vmmdev_softc *
vmmdev_lookup(const char *name, struct ucred *cred)
{
	struct vmmdev_softc *sc;

	sx_assert(&vmmdev_mtx, SA_XLOCKED);

	SLIST_FOREACH(sc, &head, link) {
		if (strcmp(name, vm_name(sc->vm)) == 0)
			break;
	}

	if (sc == NULL)
		return (NULL);

	if (cr_cansee(cred, sc->ucred))
		return (NULL);

	return (sc);
}

static struct vmmdev_softc *
vmmdev_lookup2(struct cdev *cdev)
{
	return (cdev->si_drv1);
}

static int
vmmdev_rw(struct cdev *cdev, struct uio *uio, int flags)
{
	int error, off, c, prot;
	vm_paddr_t gpa, maxaddr;
	void *hpa, *cookie;
	struct vmmdev_softc *sc;

	sc = vmmdev_lookup2(cdev);
	if (sc == NULL)
		return (ENXIO);

	/*
	 * Get a read lock on the guest memory map.
	 */
	vm_slock_memsegs(sc->vm);

	error = 0;
	prot = (uio->uio_rw == UIO_WRITE ? VM_PROT_WRITE : VM_PROT_READ);
	maxaddr = vmm_sysmem_maxaddr(sc->vm);
	while (uio->uio_resid > 0 && error == 0) {
		gpa = uio->uio_offset;
		off = gpa & PAGE_MASK;
		c = min(uio->uio_resid, PAGE_SIZE - off);

		/*
		 * The VM has a hole in its physical memory map. If we want to
		 * use 'dd' to inspect memory beyond the hole we need to
		 * provide bogus data for memory that lies in the hole.
		 *
		 * Since this device does not support lseek(2), dd(1) will
		 * read(2) blocks of data to simulate the lseek(2).
		 */
		hpa = vm_gpa_hold_global(sc->vm, gpa, c, prot, &cookie);
		if (hpa == NULL) {
			if (uio->uio_rw == UIO_READ && gpa < maxaddr)
				error = uiomove(__DECONST(void *, zero_region),
				    c, uio);
			else
				error = EFAULT;
		} else {
			error = uiomove(hpa, c, uio);
			vm_gpa_release(cookie);
		}
	}
	vm_unlock_memsegs(sc->vm);
	return (error);
}

CTASSERT(sizeof(((struct vm_memseg *)0)->name) >= VM_MAX_SUFFIXLEN + 1);

static int
get_memseg(struct vmmdev_softc *sc, struct vm_memseg *mseg, size_t len)
{
	struct devmem_softc *dsc;
	int error;
	bool sysmem;

	error = vm_get_memseg(sc->vm, mseg->segid, &mseg->len, &sysmem, NULL);
	if (error || mseg->len == 0)
		return (error);

	if (!sysmem) {
		SLIST_FOREACH(dsc, &sc->devmem, link) {
			if (dsc->segid == mseg->segid)
				break;
		}
		KASSERT(dsc != NULL, ("%s: devmem segment %d not found",
		    __func__, mseg->segid));
		error = copystr(dsc->name, mseg->name, len, NULL);
	} else {
		bzero(mseg->name, len);
	}

	return (error);
}

static int
alloc_memseg(struct vmmdev_softc *sc, struct vm_memseg *mseg, size_t len,
    struct domainset *domainset)
{
	char *name;
	int error;
	bool sysmem;

	error = 0;
	name = NULL;
	sysmem = true;

	/*
	 * The allocation is lengthened by 1 to hold a terminating NUL.  It'll
	 * by stripped off when devfs processes the full string.
	 */
	if (VM_MEMSEG_NAME(mseg)) {
		sysmem = false;
		name = malloc(len, M_VMMDEV, M_WAITOK);
		error = copystr(mseg->name, name, len, NULL);
		if (error)
			goto done;
	}
	error = vm_alloc_memseg(sc->vm, mseg->segid, mseg->len, sysmem, domainset);
	if (error)
		goto done;

	if (VM_MEMSEG_NAME(mseg)) {
		error = devmem_create_cdev(sc, mseg->segid, name);
		if (error)
			vm_free_memseg(sc->vm, mseg->segid);
		else
			name = NULL;	/* freed when 'cdev' is destroyed */
	}
done:
	free(name, M_VMMDEV);
	return (error);
}

#if defined(__amd64__) && \
    (defined(COMPAT_FREEBSD14) || defined(COMPAT_FREEBSD12))
/*
 * Translate pre-15.0 memory segment identifiers into their 15.0 counterparts.
 */
static void
adjust_segid(struct vm_memseg *mseg)
{
	if (mseg->segid != VM_SYSMEM) {
		mseg->segid += (VM_BOOTROM - 1);
	}
}
#endif

static int
vm_get_register_set(struct vcpu *vcpu, unsigned int count, int *regnum,
    uint64_t *regval)
{
	int error, i;

	error = 0;
	for (i = 0; i < count; i++) {
		error = vm_get_register(vcpu, regnum[i], &regval[i]);
		if (error)
			break;
	}
	return (error);
}

static int
vm_set_register_set(struct vcpu *vcpu, unsigned int count, int *regnum,
    uint64_t *regval)
{
	int error, i;

	error = 0;
	for (i = 0; i < count; i++) {
		error = vm_set_register(vcpu, regnum[i], regval[i]);
		if (error)
			break;
	}
	return (error);
}

static int
vmmdev_open(struct cdev *dev, int flags, int fmt, struct thread *td)
{
	int error;

	/*
	 * A jail without vmm access shouldn't be able to access vmm device
	 * files at all, but check here just to be thorough.
	 */
	error = vmm_priv_check(td->td_ucred);
	if (error != 0)
		return (error);

	return (0);
}

static const struct vmmdev_ioctl vmmdev_ioctls[] = {
	VMMDEV_IOCTL(VM_GET_REGISTER, VMMDEV_IOCTL_LOCK_ONE_VCPU),
	VMMDEV_IOCTL(VM_SET_REGISTER, VMMDEV_IOCTL_LOCK_ONE_VCPU),
	VMMDEV_IOCTL(VM_GET_REGISTER_SET, VMMDEV_IOCTL_LOCK_ONE_VCPU),
	VMMDEV_IOCTL(VM_SET_REGISTER_SET, VMMDEV_IOCTL_LOCK_ONE_VCPU),
	VMMDEV_IOCTL(VM_GET_CAPABILITY, VMMDEV_IOCTL_LOCK_ONE_VCPU),
	VMMDEV_IOCTL(VM_SET_CAPABILITY, VMMDEV_IOCTL_LOCK_ONE_VCPU),
	VMMDEV_IOCTL(VM_ACTIVATE_CPU, VMMDEV_IOCTL_LOCK_ONE_VCPU),
	VMMDEV_IOCTL(VM_INJECT_EXCEPTION, VMMDEV_IOCTL_LOCK_ONE_VCPU),
	VMMDEV_IOCTL(VM_STATS, VMMDEV_IOCTL_LOCK_ONE_VCPU),
	VMMDEV_IOCTL(VM_STAT_DESC, 0),

#ifdef __amd64__
#ifdef COMPAT_FREEBSD12
	VMMDEV_IOCTL(VM_ALLOC_MEMSEG_12,
	    VMMDEV_IOCTL_XLOCK_MEMSEGS | VMMDEV_IOCTL_LOCK_ALL_VCPUS),
#endif
#ifdef COMPAT_FREEBSD14
	VMMDEV_IOCTL(VM_ALLOC_MEMSEG_14,
	    VMMDEV_IOCTL_XLOCK_MEMSEGS | VMMDEV_IOCTL_LOCK_ALL_VCPUS),
#endif
#endif /* __amd64__ */
	VMMDEV_IOCTL(VM_ALLOC_MEMSEG,
	    VMMDEV_IOCTL_XLOCK_MEMSEGS | VMMDEV_IOCTL_LOCK_ALL_VCPUS),
	VMMDEV_IOCTL(VM_MMAP_MEMSEG,
	    VMMDEV_IOCTL_XLOCK_MEMSEGS | VMMDEV_IOCTL_LOCK_ALL_VCPUS),
	VMMDEV_IOCTL(VM_MUNMAP_MEMSEG,
	    VMMDEV_IOCTL_XLOCK_MEMSEGS | VMMDEV_IOCTL_LOCK_ALL_VCPUS),
	VMMDEV_IOCTL(VM_REINIT,
	    VMMDEV_IOCTL_XLOCK_MEMSEGS | VMMDEV_IOCTL_LOCK_ALL_VCPUS),

#ifdef __amd64__
#if defined(COMPAT_FREEBSD12)
	VMMDEV_IOCTL(VM_GET_MEMSEG_12, VMMDEV_IOCTL_SLOCK_MEMSEGS),
#endif
#ifdef COMPAT_FREEBSD14
	VMMDEV_IOCTL(VM_GET_MEMSEG_14, VMMDEV_IOCTL_SLOCK_MEMSEGS),
#endif
#endif /* __amd64__ */
	VMMDEV_IOCTL(VM_GET_MEMSEG, VMMDEV_IOCTL_SLOCK_MEMSEGS),
	VMMDEV_IOCTL(VM_MMAP_GETNEXT, VMMDEV_IOCTL_SLOCK_MEMSEGS),

	VMMDEV_IOCTL(VM_SUSPEND_CPU, VMMDEV_IOCTL_MAYBE_ALLOC_VCPU),
	VMMDEV_IOCTL(VM_RESUME_CPU, VMMDEV_IOCTL_MAYBE_ALLOC_VCPU),

	VMMDEV_IOCTL(VM_SUSPEND, 0),
	VMMDEV_IOCTL(VM_GET_CPUS, 0),
	VMMDEV_IOCTL(VM_GET_TOPOLOGY, 0),
	VMMDEV_IOCTL(VM_SET_TOPOLOGY, 0),
};

static int
vmmdev_ioctl(struct cdev *cdev, u_long cmd, caddr_t data, int fflag,
    struct thread *td)
{
	struct vmmdev_softc *sc;
	struct vcpu *vcpu;
	const struct vmmdev_ioctl *ioctl;
	struct vm_memseg *mseg;
	int error, vcpuid;

	sc = vmmdev_lookup2(cdev);
	if (sc == NULL)
		return (ENXIO);

	ioctl = NULL;
	for (size_t i = 0; i < nitems(vmmdev_ioctls); i++) {
		if (vmmdev_ioctls[i].cmd == cmd) {
			ioctl = &vmmdev_ioctls[i];
			break;
		}
	}
	if (ioctl == NULL) {
		for (size_t i = 0; i < vmmdev_machdep_ioctl_count; i++) {
			if (vmmdev_machdep_ioctls[i].cmd == cmd) {
				ioctl = &vmmdev_machdep_ioctls[i];
				break;
			}
		}
	}
	if (ioctl == NULL)
		return (ENOTTY);

	if ((ioctl->flags & VMMDEV_IOCTL_XLOCK_MEMSEGS) != 0)
		vm_xlock_memsegs(sc->vm);
	else if ((ioctl->flags & VMMDEV_IOCTL_SLOCK_MEMSEGS) != 0)
		vm_slock_memsegs(sc->vm);

	vcpu = NULL;
	vcpuid = -1;
	if ((ioctl->flags & (VMMDEV_IOCTL_LOCK_ONE_VCPU |
	    VMMDEV_IOCTL_ALLOC_VCPU | VMMDEV_IOCTL_MAYBE_ALLOC_VCPU)) != 0) {
		vcpuid = *(int *)data;
		if (vcpuid == -1) {
			if ((ioctl->flags &
			    VMMDEV_IOCTL_MAYBE_ALLOC_VCPU) == 0) {
				error = EINVAL;
				goto lockfail;
			}
		} else {
			vcpu = vm_alloc_vcpu(sc->vm, vcpuid);
			if (vcpu == NULL) {
				error = EINVAL;
				goto lockfail;
			}
			if ((ioctl->flags & VMMDEV_IOCTL_LOCK_ONE_VCPU) != 0) {
				error = vcpu_lock_one(vcpu);
				if (error)
					goto lockfail;
			}
		}
	}
	if ((ioctl->flags & VMMDEV_IOCTL_LOCK_ALL_VCPUS) != 0) {
		error = vcpu_lock_all(sc);
		if (error)
			goto lockfail;
	}

	switch (cmd) {
	case VM_SUSPEND: {
		struct vm_suspend *vmsuspend;

		vmsuspend = (struct vm_suspend *)data;
		error = vm_suspend(sc->vm, vmsuspend->how);
		break;
	}
	case VM_REINIT:
		error = vm_reinit(sc->vm);
		break;
	case VM_STAT_DESC: {
		struct vm_stat_desc *statdesc;

		statdesc = (struct vm_stat_desc *)data;
		error = vmm_stat_desc_copy(statdesc->index, statdesc->desc,
		    sizeof(statdesc->desc));
		break;
	}
	case VM_STATS: {
		struct vm_stats *vmstats;

		vmstats = (struct vm_stats *)data;
		getmicrotime(&vmstats->tv);
		error = vmm_stat_copy(vcpu, vmstats->index,
		    nitems(vmstats->statbuf), &vmstats->num_entries,
		    vmstats->statbuf);
		break;
	}
	case VM_MMAP_GETNEXT: {
		struct vm_memmap *mm;

		mm = (struct vm_memmap *)data;
		error = vm_mmap_getnext(sc->vm, &mm->gpa, &mm->segid,
		    &mm->segoff, &mm->len, &mm->prot, &mm->flags);
		break;
	}
	case VM_MMAP_MEMSEG: {
		struct vm_memmap *mm;

		mm = (struct vm_memmap *)data;
		error = vm_mmap_memseg(sc->vm, mm->gpa, mm->segid, mm->segoff,
		    mm->len, mm->prot, mm->flags);
		break;
	}
	case VM_MUNMAP_MEMSEG: {
		struct vm_munmap *mu;

		mu = (struct vm_munmap *)data;
		error = vm_munmap_memseg(sc->vm, mu->gpa, mu->len);
		break;
	}
#ifdef __amd64__
#ifdef COMPAT_FREEBSD12
	case VM_ALLOC_MEMSEG_12:
		mseg = (struct vm_memseg *)data;

		adjust_segid(mseg);
		error = alloc_memseg(sc, mseg,
		    sizeof(((struct vm_memseg_12 *)0)->name), NULL);
		break;
	case VM_GET_MEMSEG_12:
		mseg = (struct vm_memseg *)data;

		adjust_segid(mseg);
		error = get_memseg(sc, mseg,
		    sizeof(((struct vm_memseg_12 *)0)->name));
		break;
#endif /* COMPAT_FREEBSD12 */
#ifdef COMPAT_FREEBSD14
	case VM_ALLOC_MEMSEG_14:
		mseg = (struct vm_memseg *)data;

		adjust_segid(mseg);
		error = alloc_memseg(sc, mseg,
		    sizeof(((struct vm_memseg_14 *)0)->name), NULL);
		break;
	case VM_GET_MEMSEG_14:
		mseg = (struct vm_memseg *)data;

		adjust_segid(mseg);
		error = get_memseg(sc, mseg,
		    sizeof(((struct vm_memseg_14 *)0)->name));
		break;
#endif /* COMPAT_FREEBSD14 */
#endif /* __amd64__ */
	case VM_ALLOC_MEMSEG: {
		domainset_t *mask;
		struct domainset *domainset, domain;

		domainset = NULL;
		mseg = (struct vm_memseg *)data;
		if (mseg->ds_policy != DOMAINSET_POLICY_INVALID && mseg->ds_mask != NULL) {
			if (mseg->ds_mask_size < sizeof(domainset_t) ||
			    mseg->ds_mask_size > DOMAINSET_MAXSIZE / NBBY) {
				error = ERANGE;
				break;
			}
			memset(&domain, 0, sizeof(domain));
			mask = malloc(mseg->ds_mask_size, M_VMMDEV, M_WAITOK);
			error = copyin(mseg->ds_mask, mask, mseg->ds_mask_size);
			if (error) {
				free(mask, M_VMMDEV);
				break;
			}
			error = domainset_populate(&domain, mask, mseg->ds_policy,
			    mseg->ds_mask_size);
			if (error) {
				free(mask, M_VMMDEV);
				break;
			}
			domainset = domainset_create(&domain);
			if (domainset == NULL) {
				error = EINVAL;
				free(mask, M_VMMDEV);
				break;
			}
			free(mask, M_VMMDEV);
		}
		error = alloc_memseg(sc, mseg, sizeof(mseg->name), domainset);

		break;
	}
	case VM_GET_MEMSEG:
		error = get_memseg(sc, (struct vm_memseg *)data,
		    sizeof(((struct vm_memseg *)0)->name));
		break;
	case VM_GET_REGISTER: {
		struct vm_register *vmreg;

		vmreg = (struct vm_register *)data;
		error = vm_get_register(vcpu, vmreg->regnum, &vmreg->regval);
		break;
	}
	case VM_SET_REGISTER: {
		struct vm_register *vmreg;

		vmreg = (struct vm_register *)data;
		error = vm_set_register(vcpu, vmreg->regnum, vmreg->regval);
		break;
	}
	case VM_GET_REGISTER_SET: {
		struct vm_register_set *vmregset;
		uint64_t *regvals;
		int *regnums;

		vmregset = (struct vm_register_set *)data;
		if (vmregset->count > VM_REG_LAST) {
			error = EINVAL;
			break;
		}
		regvals = malloc(sizeof(regvals[0]) * vmregset->count, M_VMMDEV,
		    M_WAITOK);
		regnums = malloc(sizeof(regnums[0]) * vmregset->count, M_VMMDEV,
		    M_WAITOK);
		error = copyin(vmregset->regnums, regnums, sizeof(regnums[0]) *
		    vmregset->count);
		if (error == 0)
			error = vm_get_register_set(vcpu,
			    vmregset->count, regnums, regvals);
		if (error == 0)
			error = copyout(regvals, vmregset->regvals,
			    sizeof(regvals[0]) * vmregset->count);
		free(regvals, M_VMMDEV);
		free(regnums, M_VMMDEV);
		break;
	}
	case VM_SET_REGISTER_SET: {
		struct vm_register_set *vmregset;
		uint64_t *regvals;
		int *regnums;

		vmregset = (struct vm_register_set *)data;
		if (vmregset->count > VM_REG_LAST) {
			error = EINVAL;
			break;
		}
		regvals = malloc(sizeof(regvals[0]) * vmregset->count, M_VMMDEV,
		    M_WAITOK);
		regnums = malloc(sizeof(regnums[0]) * vmregset->count, M_VMMDEV,
		    M_WAITOK);
		error = copyin(vmregset->regnums, regnums, sizeof(regnums[0]) *
		    vmregset->count);
		if (error == 0)
			error = copyin(vmregset->regvals, regvals,
			    sizeof(regvals[0]) * vmregset->count);
		if (error == 0)
			error = vm_set_register_set(vcpu,
			    vmregset->count, regnums, regvals);
		free(regvals, M_VMMDEV);
		free(regnums, M_VMMDEV);
		break;
	}
	case VM_GET_CAPABILITY: {
		struct vm_capability *vmcap;

		vmcap = (struct vm_capability *)data;
		error = vm_get_capability(vcpu, vmcap->captype, &vmcap->capval);
		break;
	}
	case VM_SET_CAPABILITY: {
		struct vm_capability *vmcap;

		vmcap = (struct vm_capability *)data;
		error = vm_set_capability(vcpu, vmcap->captype, vmcap->capval);
		break;
	}
	case VM_ACTIVATE_CPU:
		error = vm_activate_cpu(vcpu);
		break;
	case VM_GET_CPUS: {
		struct vm_cpuset *vm_cpuset;
		cpuset_t *cpuset;
		int size;

		error = 0;
		vm_cpuset = (struct vm_cpuset *)data;
		size = vm_cpuset->cpusetsize;
		if (size < 1 || size > CPU_MAXSIZE / NBBY) {
			error = ERANGE;
			break;
		}
		cpuset = malloc(max(size, sizeof(cpuset_t)), M_TEMP,
		    M_WAITOK | M_ZERO);
		if (vm_cpuset->which == VM_ACTIVE_CPUS)
			*cpuset = vm_active_cpus(sc->vm);
		else if (vm_cpuset->which == VM_SUSPENDED_CPUS)
			*cpuset = vm_suspended_cpus(sc->vm);
		else if (vm_cpuset->which == VM_DEBUG_CPUS)
			*cpuset = vm_debug_cpus(sc->vm);
		else
			error = EINVAL;
		if (error == 0 && size < howmany(CPU_FLS(cpuset), NBBY))
			error = ERANGE;
		if (error == 0)
			error = copyout(cpuset, vm_cpuset->cpus, size);
		free(cpuset, M_TEMP);
		break;
	}
	case VM_SUSPEND_CPU:
		error = vm_suspend_cpu(sc->vm, vcpu);
		break;
	case VM_RESUME_CPU:
		error = vm_resume_cpu(sc->vm, vcpu);
		break;
	case VM_SET_TOPOLOGY: {
		struct vm_cpu_topology *topology;

		topology = (struct vm_cpu_topology *)data;
		error = vm_set_topology(sc->vm, topology->sockets,
		    topology->cores, topology->threads, topology->maxcpus);
		break;
	}
	case VM_GET_TOPOLOGY: {
		struct vm_cpu_topology *topology;

		topology = (struct vm_cpu_topology *)data;
		vm_get_topology(sc->vm, &topology->sockets, &topology->cores,
		    &topology->threads, &topology->maxcpus);
		error = 0;
		break;
	}
	default:
		error = vmmdev_machdep_ioctl(sc->vm, vcpu, cmd, data, fflag,
		    td);
		break;
	}

	if ((ioctl->flags &
	    (VMMDEV_IOCTL_XLOCK_MEMSEGS | VMMDEV_IOCTL_SLOCK_MEMSEGS)) != 0)
		vm_unlock_memsegs(sc->vm);
	if ((ioctl->flags & VMMDEV_IOCTL_LOCK_ALL_VCPUS) != 0)
		vcpu_unlock_all(sc);
	else if ((ioctl->flags & VMMDEV_IOCTL_LOCK_ONE_VCPU) != 0)
		vcpu_unlock_one(vcpu);

	/*
	 * Make sure that no handler returns a kernel-internal
	 * error value to userspace.
	 */
	KASSERT(error == ERESTART || error >= 0,
	    ("vmmdev_ioctl: invalid error return %d", error));
	return (error);

lockfail:
	if ((ioctl->flags &
	    (VMMDEV_IOCTL_XLOCK_MEMSEGS | VMMDEV_IOCTL_SLOCK_MEMSEGS)) != 0)
		vm_unlock_memsegs(sc->vm);
	return (error);
}

static int
vmmdev_mmap_single(struct cdev *cdev, vm_ooffset_t *offset, vm_size_t mapsize,
    struct vm_object **objp, int nprot)
{
	struct vmmdev_softc *sc;
	vm_paddr_t gpa;
	size_t len;
	vm_ooffset_t segoff, first, last;
	int error, found, segid;
	bool sysmem;

	first = *offset;
	last = first + mapsize;
	if ((nprot & PROT_EXEC) || first < 0 || first >= last)
		return (EINVAL);

	sc = vmmdev_lookup2(cdev);
	if (sc == NULL) {
		/* virtual machine is in the process of being created */
		return (EINVAL);
	}

	/*
	 * Get a read lock on the guest memory map.
	 */
	vm_slock_memsegs(sc->vm);

	gpa = 0;
	found = 0;
	while (!found) {
		error = vm_mmap_getnext(sc->vm, &gpa, &segid, &segoff, &len,
		    NULL, NULL);
		if (error)
			break;

		if (first >= gpa && last <= gpa + len)
			found = 1;
		else
			gpa += len;
	}

	if (found) {
		error = vm_get_memseg(sc->vm, segid, &len, &sysmem, objp);
		KASSERT(error == 0 && *objp != NULL,
		    ("%s: invalid memory segment %d", __func__, segid));
		if (sysmem) {
			vm_object_reference(*objp);
			*offset = segoff + (first - gpa);
		} else {
			error = EINVAL;
		}
	}
	vm_unlock_memsegs(sc->vm);
	return (error);
}

static void
vmmdev_destroy(struct vmmdev_softc *sc)
{
	struct devmem_softc *dsc;
	int error __diagused;

	KASSERT(sc->cdev == NULL, ("%s: cdev not free", __func__));

	/*
	 * Destroy all cdevs:
	 *
	 * - any new operations on the 'cdev' will return an error (ENXIO).
	 *
	 * - the 'devmem' cdevs are destroyed before the virtual machine 'cdev'
	 */
	SLIST_FOREACH(dsc, &sc->devmem, link) {
		KASSERT(dsc->cdev != NULL, ("devmem cdev already destroyed"));
		devmem_destroy(dsc);
	}

	vm_disable_vcpu_creation(sc->vm);
	error = vcpu_lock_all(sc);
	KASSERT(error == 0, ("%s: error %d freezing vcpus", __func__, error));
	vm_unlock_vcpus(sc->vm);

	while ((dsc = SLIST_FIRST(&sc->devmem)) != NULL) {
		KASSERT(dsc->cdev == NULL, ("%s: devmem not free", __func__));
		SLIST_REMOVE_HEAD(&sc->devmem, link);
		free(dsc->name, M_VMMDEV);
		free(dsc, M_VMMDEV);
	}

	if (sc->vm != NULL)
		vm_destroy(sc->vm);

	if (sc->ucred != NULL)
		crfree(sc->ucred);

	sx_xlock(&vmmdev_mtx);
	SLIST_REMOVE(&head, sc, vmmdev_softc, link);
	sx_xunlock(&vmmdev_mtx);
	free(sc, M_VMMDEV);
}

static int
vmmdev_lookup_and_destroy(const char *name, struct ucred *cred)
{
	struct cdev *cdev;
	struct vmmdev_softc *sc;

	sx_xlock(&vmmdev_mtx);
	sc = vmmdev_lookup(name, cred);
	if (sc == NULL || sc->cdev == NULL) {
		sx_xunlock(&vmmdev_mtx);
		return (EINVAL);
	}

	/*
	 * Setting 'sc->cdev' to NULL is used to indicate that the VM
	 * is scheduled for destruction.
	 */
	cdev = sc->cdev;
	sc->cdev = NULL;
	sx_xunlock(&vmmdev_mtx);

	vm_suspend(sc->vm, VM_SUSPEND_DESTROY);
	destroy_dev(cdev);
	vmmdev_destroy(sc);

	return (0);
}

static int
sysctl_vmm_destroy(SYSCTL_HANDLER_ARGS)
{
	char *buf;
	int error, buflen;

	error = vmm_priv_check(req->td->td_ucred);
	if (error)
		return (error);

	buflen = VM_MAX_NAMELEN + 1;
	buf = malloc(buflen, M_VMMDEV, M_WAITOK | M_ZERO);
	error = sysctl_handle_string(oidp, buf, buflen, req);
	if (error == 0 && req->newptr != NULL)
		error = vmmdev_lookup_and_destroy(buf, req->td->td_ucred);
	free(buf, M_VMMDEV);
	return (error);
}
SYSCTL_PROC(_hw_vmm, OID_AUTO, destroy,
    CTLTYPE_STRING | CTLFLAG_RW | CTLFLAG_PRISON | CTLFLAG_MPSAFE,
    NULL, 0, sysctl_vmm_destroy, "A",
    "Destroy a vmm(4) instance (legacy interface)");

static struct cdevsw vmmdevsw = {
	.d_name		= "vmmdev",
	.d_version	= D_VERSION,
	.d_open		= vmmdev_open,
	.d_ioctl	= vmmdev_ioctl,
	.d_mmap_single	= vmmdev_mmap_single,
	.d_read		= vmmdev_rw,
	.d_write	= vmmdev_rw,
};

static struct vmmdev_softc *
vmmdev_alloc(struct vm *vm, struct ucred *cred)
{
	struct vmmdev_softc *sc;

	sc = malloc(sizeof(*sc), M_VMMDEV, M_WAITOK | M_ZERO);
	SLIST_INIT(&sc->devmem);
	sc->vm = vm;
	sc->ucred = crhold(cred);
	return (sc);
}

static int
vmmdev_create(const char *name, struct ucred *cred)
{
	struct make_dev_args mda;
	struct cdev *cdev;
	struct vmmdev_softc *sc;
	struct vm *vm;
	int error;

	sx_xlock(&vmmdev_mtx);
	sc = vmmdev_lookup(name, cred);
	if (sc != NULL) {
		sx_xunlock(&vmmdev_mtx);
		return (EEXIST);
	}

	error = vm_create(name, &vm);
	if (error != 0) {
		sx_xunlock(&vmmdev_mtx);
		return (error);
	}
	sc = vmmdev_alloc(vm, cred);
	SLIST_INSERT_HEAD(&head, sc, link);

	make_dev_args_init(&mda);
	mda.mda_devsw = &vmmdevsw;
	mda.mda_cr = sc->ucred;
	mda.mda_uid = UID_ROOT;
	mda.mda_gid = GID_WHEEL;
	mda.mda_mode = 0600;
	mda.mda_si_drv1 = sc;
	mda.mda_flags = MAKEDEV_CHECKNAME | MAKEDEV_WAITOK;
	error = make_dev_s(&mda, &cdev, "vmm/%s", name);
	if (error != 0) {
		sx_xunlock(&vmmdev_mtx);
		vmmdev_destroy(sc);
		return (error);
	}
	sc->cdev = cdev;
	sx_xunlock(&vmmdev_mtx);
	return (0);
}

static int
sysctl_vmm_create(SYSCTL_HANDLER_ARGS)
{
	char *buf;
	int error, buflen;

	error = vmm_priv_check(req->td->td_ucred);
	if (error != 0)
		return (error);

	buflen = VM_MAX_NAMELEN + 1;
	buf = malloc(buflen, M_VMMDEV, M_WAITOK | M_ZERO);
	error = sysctl_handle_string(oidp, buf, buflen, req);
	if (error == 0 && req->newptr != NULL)
		error = vmmdev_create(buf, req->td->td_ucred);
	free(buf, M_VMMDEV);
	return (error);
}
SYSCTL_PROC(_hw_vmm, OID_AUTO, create,
    CTLTYPE_STRING | CTLFLAG_RW | CTLFLAG_PRISON | CTLFLAG_MPSAFE,
    NULL, 0, sysctl_vmm_create, "A",
    "Create a vmm(4) instance (legacy interface)");

static int
vmmctl_open(struct cdev *cdev, int flags, int fmt, struct thread *td)
{
	int error;

	error = vmm_priv_check(td->td_ucred);
	if (error != 0)
		return (error);

	if ((flags & FWRITE) == 0)
		return (EPERM);

	return (0);
}

static int
vmmctl_ioctl(struct cdev *cdev, u_long cmd, caddr_t data, int fflag,
    struct thread *td)
{
	int error;

	switch (cmd) {
	case VMMCTL_VM_CREATE: {
		struct vmmctl_vm_create *vmc;

		vmc = (struct vmmctl_vm_create *)data;
		vmc->name[VM_MAX_NAMELEN] = '\0';
		for (size_t i = 0; i < nitems(vmc->reserved); i++) {
			if (vmc->reserved[i] != 0) {
				error = EINVAL;
				return (error);
			}
		}

		error = vmmdev_create(vmc->name, td->td_ucred);
		break;
	}
	case VMMCTL_VM_DESTROY: {
		struct vmmctl_vm_destroy *vmd;

		vmd = (struct vmmctl_vm_destroy *)data;
		vmd->name[VM_MAX_NAMELEN] = '\0';
		for (size_t i = 0; i < nitems(vmd->reserved); i++) {
			if (vmd->reserved[i] != 0) {
				error = EINVAL;
				return (error);
			}
		}

		error = vmmdev_lookup_and_destroy(vmd->name, td->td_ucred);
		break;
	}
	default:
		error = ENOTTY;
		break;
	}

	return (error);
}

static struct cdev *vmmctl_cdev;
static struct cdevsw vmmctlsw = {
	.d_name		= "vmmctl",
	.d_version	= D_VERSION,
	.d_open		= vmmctl_open,
	.d_ioctl	= vmmctl_ioctl,
};

int
vmmdev_init(void)
{
	int error;

	sx_xlock(&vmmdev_mtx);
	error = make_dev_p(MAKEDEV_CHECKNAME, &vmmctl_cdev, &vmmctlsw, NULL,
	    UID_ROOT, GID_WHEEL, 0600, "vmmctl");
	if (error == 0)
		pr_allow_flag = prison_add_allow(NULL, "vmm", NULL,
		    "Allow use of vmm in a jail.");
	sx_xunlock(&vmmdev_mtx);

	return (error);
}

int
vmmdev_cleanup(void)
{
	sx_xlock(&vmmdev_mtx);
	if (!SLIST_EMPTY(&head)) {
		sx_xunlock(&vmmdev_mtx);
		return (EBUSY);
	}
	if (vmmctl_cdev != NULL) {
		destroy_dev(vmmctl_cdev);
		vmmctl_cdev = NULL;
	}
	sx_xunlock(&vmmdev_mtx);

	return (0);
}

static int
devmem_mmap_single(struct cdev *cdev, vm_ooffset_t *offset, vm_size_t len,
    struct vm_object **objp, int nprot)
{
	struct devmem_softc *dsc;
	vm_ooffset_t first, last;
	size_t seglen;
	int error;
	bool sysmem;

	dsc = cdev->si_drv1;
	if (dsc == NULL) {
		/* 'cdev' has been created but is not ready for use */
		return (ENXIO);
	}

	first = *offset;
	last = *offset + len;
	if ((nprot & PROT_EXEC) || first < 0 || first >= last)
		return (EINVAL);

	vm_slock_memsegs(dsc->sc->vm);

	error = vm_get_memseg(dsc->sc->vm, dsc->segid, &seglen, &sysmem, objp);
	KASSERT(error == 0 && !sysmem && *objp != NULL,
	    ("%s: invalid devmem segment %d", __func__, dsc->segid));

	if (seglen >= last)
		vm_object_reference(*objp);
	else
		error = EINVAL;

	vm_unlock_memsegs(dsc->sc->vm);
	return (error);
}

static struct cdevsw devmemsw = {
	.d_name		= "devmem",
	.d_version	= D_VERSION,
	.d_mmap_single	= devmem_mmap_single,
};

static int
devmem_create_cdev(struct vmmdev_softc *sc, int segid, char *devname)
{
	struct make_dev_args mda;
	struct devmem_softc *dsc;
	int error;

	sx_xlock(&vmmdev_mtx);

	dsc = malloc(sizeof(struct devmem_softc), M_VMMDEV, M_WAITOK | M_ZERO);
	dsc->segid = segid;
	dsc->name = devname;
	dsc->sc = sc;
	SLIST_INSERT_HEAD(&sc->devmem, dsc, link);

	make_dev_args_init(&mda);
	mda.mda_devsw = &devmemsw;
	mda.mda_cr = sc->ucred;
	mda.mda_uid = UID_ROOT;
	mda.mda_gid = GID_WHEEL;
	mda.mda_mode = 0600;
	mda.mda_si_drv1 = dsc;
	mda.mda_flags = MAKEDEV_CHECKNAME | MAKEDEV_WAITOK;
	error = make_dev_s(&mda, &dsc->cdev, "vmm.io/%s.%s", vm_name(sc->vm),
	    devname);
	if (error != 0) {
		SLIST_REMOVE(&sc->devmem, dsc, devmem_softc, link);
		free(dsc->name, M_VMMDEV);
		free(dsc, M_VMMDEV);
	}

	sx_xunlock(&vmmdev_mtx);

	return (error);
}

static void
devmem_destroy(void *arg)
{
	struct devmem_softc *dsc = arg;

	destroy_dev(dsc->cdev);
	dsc->cdev = NULL;
	dsc->sc = NULL;
}
