/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2002 Doug Rabson
 * Copyright (c) 1994-1995 Søren Schmidt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_compat.h"

#include <sys/param.h>
#include <sys/blist.h>
#include <sys/fcntl.h>
#if defined(__i386__)
#include <sys/imgact_aout.h>
#endif
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/msgbuf.h>
#include <sys/mutex.h>
#include <sys/namei.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/procctl.h>
#include <sys/reboot.h>
#include <sys/racct.h>
#include <sys/random.h>
#include <sys/resourcevar.h>
#include <sys/sched.h>
#include <sys/sdt.h>
#include <sys/signalvar.h>
#include <sys/stat.h>
#include <sys/syscallsubr.h>
#include <sys/sysctl.h>
#include <sys/sysproto.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/vmmeter.h>
#include <sys/vnode.h>
#include <sys/wait.h>
#include <sys/cpuset.h>
#include <sys/uio.h>

#include <security/mac/mac_framework.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_kern.h>
#include <vm/vm_map.h>
#include <vm/vm_extern.h>
#include <vm/swap_pager.h>

#ifdef COMPAT_LINUX32
#include <machine/../linux32/linux.h>
#include <machine/../linux32/linux32_proto.h>
#else
#include <machine/../linux/linux.h>
#include <machine/../linux/linux_proto.h>
#endif

#include <compat/linux/linux_dtrace.h>
#include <compat/linux/linux_file.h>
#include <compat/linux/linux_mib.h>
#include <compat/linux/linux_signal.h>
#include <compat/linux/linux_timer.h>
#include <compat/linux/linux_util.h>
#include <compat/linux/linux_sysproto.h>
#include <compat/linux/linux_emul.h>
#include <compat/linux/linux_misc.h>

/**
 * Special DTrace provider for the linuxulator.
 *
 * In this file we define the provider for the entire linuxulator. All
 * modules (= files of the linuxulator) use it.
 *
 * We define a different name depending on the emulated bitsize, see
 * ../../<ARCH>/linux{,32}/linux.h, e.g.:
 *      native bitsize          = linuxulator
 *      amd64, 32bit emulation  = linuxulator32
 */
LIN_SDT_PROVIDER_DEFINE(LINUX_DTRACE);

int stclohz;				/* Statistics clock frequency */

static unsigned int linux_to_bsd_resource[LINUX_RLIM_NLIMITS] = {
	RLIMIT_CPU, RLIMIT_FSIZE, RLIMIT_DATA, RLIMIT_STACK,
	RLIMIT_CORE, RLIMIT_RSS, RLIMIT_NPROC, RLIMIT_NOFILE,
	RLIMIT_MEMLOCK, RLIMIT_AS
};

struct l_sysinfo {
	l_long		uptime;		/* Seconds since boot */
	l_ulong		loads[3];	/* 1, 5, and 15 minute load averages */
#define LINUX_SYSINFO_LOADS_SCALE 65536
	l_ulong		totalram;	/* Total usable main memory size */
	l_ulong		freeram;	/* Available memory size */
	l_ulong		sharedram;	/* Amount of shared memory */
	l_ulong		bufferram;	/* Memory used by buffers */
	l_ulong		totalswap;	/* Total swap space size */
	l_ulong		freeswap;	/* swap space still available */
	l_ushort	procs;		/* Number of current processes */
	l_ushort	pads;
	l_ulong		totalhigh;
	l_ulong		freehigh;
	l_uint		mem_unit;
	char		_f[20-2*sizeof(l_long)-sizeof(l_int)];	/* padding */
};

struct l_pselect6arg {
	l_uintptr_t	ss;
	l_size_t	ss_len;
};

static int	linux_utimensat_nsec_valid(l_long);

int
linux_sysinfo(struct thread *td, struct linux_sysinfo_args *args)
{
	struct l_sysinfo sysinfo;
	int i, j;
	struct timespec ts;

	bzero(&sysinfo, sizeof(sysinfo));
	getnanouptime(&ts);
	if (ts.tv_nsec != 0)
		ts.tv_sec++;
	sysinfo.uptime = ts.tv_sec;

	/* Use the information from the mib to get our load averages */
	for (i = 0; i < 3; i++)
		sysinfo.loads[i] = averunnable.ldavg[i] *
		    LINUX_SYSINFO_LOADS_SCALE / averunnable.fscale;

	sysinfo.totalram = physmem * PAGE_SIZE;
	sysinfo.freeram = (u_long)vm_free_count() * PAGE_SIZE;

	/*
	 * sharedram counts pages allocated to named, swap-backed objects such
	 * as shared memory segments and tmpfs files.  There is no cheap way to
	 * compute this, so just leave the field unpopulated.  Linux itself only
	 * started setting this field in the 3.x timeframe.
	 */
	sysinfo.sharedram = 0;
	sysinfo.bufferram = 0;

	swap_pager_status(&i, &j);
	sysinfo.totalswap = i * PAGE_SIZE;
	sysinfo.freeswap = (i - j) * PAGE_SIZE;

	sysinfo.procs = nprocs;

	/*
	 * Platforms supported by the emulation layer do not have a notion of
	 * high memory.
	 */
	sysinfo.totalhigh = 0;
	sysinfo.freehigh = 0;

	sysinfo.mem_unit = 1;

	return (copyout(&sysinfo, args->info, sizeof(sysinfo)));
}

#ifdef LINUX_LEGACY_SYSCALLS
int
linux_alarm(struct thread *td, struct linux_alarm_args *args)
{
	struct itimerval it, old_it;
	u_int secs;
	int error;

	secs = args->secs;
	/*
	 * Linux alarm() is always successful. Limit secs to INT32_MAX / 2
	 * to match kern_setitimer()'s limit to avoid error from it.
	 *
	 * XXX. Linux limit secs to INT_MAX on 32 and does not limit on 64-bit
	 * platforms.
	 */
	if (secs > INT32_MAX / 2)
		secs = INT32_MAX / 2;

	it.it_value.tv_sec = secs;
	it.it_value.tv_usec = 0;
	timevalclear(&it.it_interval);
	error = kern_setitimer(td, ITIMER_REAL, &it, &old_it);
	KASSERT(error == 0, ("kern_setitimer returns %d", error));

	if ((old_it.it_value.tv_sec == 0 && old_it.it_value.tv_usec > 0) ||
	    old_it.it_value.tv_usec >= 500000)
		old_it.it_value.tv_sec++;
	td->td_retval[0] = old_it.it_value.tv_sec;
	return (0);
}
#endif

int
linux_brk(struct thread *td, struct linux_brk_args *args)
{
	struct vmspace *vm = td->td_proc->p_vmspace;
	uintptr_t new, old;

	old = (uintptr_t)vm->vm_daddr + ctob(vm->vm_dsize);
	new = (uintptr_t)args->dsend;
	if ((caddr_t)new > vm->vm_daddr && !kern_break(td, &new))
		td->td_retval[0] = (register_t)new;
	else
		td->td_retval[0] = (register_t)old;

	return (0);
}

#if defined(__i386__)
/* XXX: what about amd64/linux32? */

int
linux_uselib(struct thread *td, struct linux_uselib_args *args)
{
	struct nameidata ni;
	struct vnode *vp;
	struct exec *a_out;
	vm_map_t map;
	vm_map_entry_t entry;
	struct vattr attr;
	vm_offset_t vmaddr;
	unsigned long file_offset;
	unsigned long bss_size;
	char *library;
	ssize_t aresid;
	int error;
	bool locked, opened, textset;

	a_out = NULL;
	vp = NULL;
	locked = false;
	textset = false;
	opened = false;

	if (!LUSECONVPATH(td)) {
		NDINIT(&ni, LOOKUP, ISOPEN | FOLLOW | LOCKLEAF | AUDITVNODE1,
		    UIO_USERSPACE, args->library, td);
		error = namei(&ni);
	} else {
		LCONVPATHEXIST(td, args->library, &library);
		NDINIT(&ni, LOOKUP, ISOPEN | FOLLOW | LOCKLEAF | AUDITVNODE1,
		    UIO_SYSSPACE, library, td);
		error = namei(&ni);
		LFREEPATH(library);
	}
	if (error)
		goto cleanup;

	vp = ni.ni_vp;
	NDFREE(&ni, NDF_ONLY_PNBUF);

	/*
	 * From here on down, we have a locked vnode that must be unlocked.
	 * XXX: The code below largely duplicates exec_check_permissions().
	 */
	locked = true;

	/* Executable? */
	error = VOP_GETATTR(vp, &attr, td->td_ucred);
	if (error)
		goto cleanup;

	if ((vp->v_mount->mnt_flag & MNT_NOEXEC) ||
	    ((attr.va_mode & 0111) == 0) || (attr.va_type != VREG)) {
		/* EACCESS is what exec(2) returns. */
		error = ENOEXEC;
		goto cleanup;
	}

	/* Sensible size? */
	if (attr.va_size == 0) {
		error = ENOEXEC;
		goto cleanup;
	}

	/* Can we access it? */
	error = VOP_ACCESS(vp, VEXEC, td->td_ucred, td);
	if (error)
		goto cleanup;

	/*
	 * XXX: This should use vn_open() so that it is properly authorized,
	 * and to reduce code redundancy all over the place here.
	 * XXX: Not really, it duplicates far more of exec_check_permissions()
	 * than vn_open().
	 */
#ifdef MAC
	error = mac_vnode_check_open(td->td_ucred, vp, VREAD);
	if (error)
		goto cleanup;
#endif
	error = VOP_OPEN(vp, FREAD, td->td_ucred, td, NULL);
	if (error)
		goto cleanup;
	opened = true;

	/* Pull in executable header into exec_map */
	error = vm_mmap(exec_map, (vm_offset_t *)&a_out, PAGE_SIZE,
	    VM_PROT_READ, VM_PROT_READ, 0, OBJT_VNODE, vp, 0);
	if (error)
		goto cleanup;

	/* Is it a Linux binary ? */
	if (((a_out->a_magic >> 16) & 0xff) != 0x64) {
		error = ENOEXEC;
		goto cleanup;
	}

	/*
	 * While we are here, we should REALLY do some more checks
	 */

	/* Set file/virtual offset based on a.out variant. */
	switch ((int)(a_out->a_magic & 0xffff)) {
	case 0413:			/* ZMAGIC */
		file_offset = 1024;
		break;
	case 0314:			/* QMAGIC */
		file_offset = 0;
		break;
	default:
		error = ENOEXEC;
		goto cleanup;
	}

	bss_size = round_page(a_out->a_bss);

	/* Check various fields in header for validity/bounds. */
	if (a_out->a_text & PAGE_MASK || a_out->a_data & PAGE_MASK) {
		error = ENOEXEC;
		goto cleanup;
	}

	/* text + data can't exceed file size */
	if (a_out->a_data + a_out->a_text > attr.va_size) {
		error = EFAULT;
		goto cleanup;
	}

	/*
	 * text/data/bss must not exceed limits
	 * XXX - this is not complete. it should check current usage PLUS
	 * the resources needed by this library.
	 */
	PROC_LOCK(td->td_proc);
	if (a_out->a_text > maxtsiz ||
	    a_out->a_data + bss_size > lim_cur_proc(td->td_proc, RLIMIT_DATA) ||
	    racct_set(td->td_proc, RACCT_DATA, a_out->a_data +
	    bss_size) != 0) {
		PROC_UNLOCK(td->td_proc);
		error = ENOMEM;
		goto cleanup;
	}
	PROC_UNLOCK(td->td_proc);

	/*
	 * Prevent more writers.
	 */
	error = VOP_SET_TEXT(vp);
	if (error != 0)
		goto cleanup;
	textset = true;

	/*
	 * Lock no longer needed
	 */
	locked = false;
	VOP_UNLOCK(vp);

	/*
	 * Check if file_offset page aligned. Currently we cannot handle
	 * misalinged file offsets, and so we read in the entire image
	 * (what a waste).
	 */
	if (file_offset & PAGE_MASK) {
		/* Map text+data read/write/execute */

		/* a_entry is the load address and is page aligned */
		vmaddr = trunc_page(a_out->a_entry);

		/* get anon user mapping, read+write+execute */
		error = vm_map_find(&td->td_proc->p_vmspace->vm_map, NULL, 0,
		    &vmaddr, a_out->a_text + a_out->a_data, 0, VMFS_NO_SPACE,
		    VM_PROT_ALL, VM_PROT_ALL, 0);
		if (error)
			goto cleanup;

		error = vn_rdwr(UIO_READ, vp, (void *)vmaddr, file_offset,
		    a_out->a_text + a_out->a_data, UIO_USERSPACE, 0,
		    td->td_ucred, NOCRED, &aresid, td);
		if (error != 0)
			goto cleanup;
		if (aresid != 0) {
			error = ENOEXEC;
			goto cleanup;
		}
	} else {
		/*
		 * for QMAGIC, a_entry is 20 bytes beyond the load address
		 * to skip the executable header
		 */
		vmaddr = trunc_page(a_out->a_entry);

		/*
		 * Map it all into the process's space as a single
		 * copy-on-write "data" segment.
		 */
		map = &td->td_proc->p_vmspace->vm_map;
		error = vm_mmap(map, &vmaddr,
		    a_out->a_text + a_out->a_data, VM_PROT_ALL, VM_PROT_ALL,
		    MAP_PRIVATE | MAP_FIXED, OBJT_VNODE, vp, file_offset);
		if (error)
			goto cleanup;
		vm_map_lock(map);
		if (!vm_map_lookup_entry(map, vmaddr, &entry)) {
			vm_map_unlock(map);
			error = EDOOFUS;
			goto cleanup;
		}
		entry->eflags |= MAP_ENTRY_VN_EXEC;
		vm_map_unlock(map);
		textset = false;
	}

	if (bss_size != 0) {
		/* Calculate BSS start address */
		vmaddr = trunc_page(a_out->a_entry) + a_out->a_text +
		    a_out->a_data;

		/* allocate some 'anon' space */
		error = vm_map_find(&td->td_proc->p_vmspace->vm_map, NULL, 0,
		    &vmaddr, bss_size, 0, VMFS_NO_SPACE, VM_PROT_ALL,
		    VM_PROT_ALL, 0);
		if (error)
			goto cleanup;
	}

cleanup:
	if (opened) {
		if (locked)
			VOP_UNLOCK(vp);
		locked = false;
		VOP_CLOSE(vp, FREAD, td->td_ucred, td);
	}
	if (textset) {
		if (!locked) {
			locked = true;
			VOP_LOCK(vp, LK_SHARED | LK_RETRY);
		}
		VOP_UNSET_TEXT_CHECKED(vp);
	}
	if (locked)
		VOP_UNLOCK(vp);

	/* Release the temporary mapping. */
	if (a_out)
		kmap_free_wakeup(exec_map, (vm_offset_t)a_out, PAGE_SIZE);

	return (error);
}

#endif	/* __i386__ */

#ifdef LINUX_LEGACY_SYSCALLS
int
linux_select(struct thread *td, struct linux_select_args *args)
{
	l_timeval ltv;
	struct timeval tv0, tv1, utv, *tvp;
	int error;

	/*
	 * Store current time for computation of the amount of
	 * time left.
	 */
	if (args->timeout) {
		if ((error = copyin(args->timeout, &ltv, sizeof(ltv))))
			goto select_out;
		utv.tv_sec = ltv.tv_sec;
		utv.tv_usec = ltv.tv_usec;

		if (itimerfix(&utv)) {
			/*
			 * The timeval was invalid.  Convert it to something
			 * valid that will act as it does under Linux.
			 */
			utv.tv_sec += utv.tv_usec / 1000000;
			utv.tv_usec %= 1000000;
			if (utv.tv_usec < 0) {
				utv.tv_sec -= 1;
				utv.tv_usec += 1000000;
			}
			if (utv.tv_sec < 0)
				timevalclear(&utv);
		}
		microtime(&tv0);
		tvp = &utv;
	} else
		tvp = NULL;

	error = kern_select(td, args->nfds, args->readfds, args->writefds,
	    args->exceptfds, tvp, LINUX_NFDBITS);
	if (error)
		goto select_out;

	if (args->timeout) {
		if (td->td_retval[0]) {
			/*
			 * Compute how much time was left of the timeout,
			 * by subtracting the current time and the time
			 * before we started the call, and subtracting
			 * that result from the user-supplied value.
			 */
			microtime(&tv1);
			timevalsub(&tv1, &tv0);
			timevalsub(&utv, &tv1);
			if (utv.tv_sec < 0)
				timevalclear(&utv);
		} else
			timevalclear(&utv);
		ltv.tv_sec = utv.tv_sec;
		ltv.tv_usec = utv.tv_usec;
		if ((error = copyout(&ltv, args->timeout, sizeof(ltv))))
			goto select_out;
	}

select_out:
	return (error);
}
#endif

int
linux_mremap(struct thread *td, struct linux_mremap_args *args)
{
	uintptr_t addr;
	size_t len;
	int error = 0;

	if (args->flags & ~(LINUX_MREMAP_FIXED | LINUX_MREMAP_MAYMOVE)) {
		td->td_retval[0] = 0;
		return (EINVAL);
	}

	/*
	 * Check for the page alignment.
	 * Linux defines PAGE_MASK to be FreeBSD ~PAGE_MASK.
	 */
	if (args->addr & PAGE_MASK) {
		td->td_retval[0] = 0;
		return (EINVAL);
	}

	args->new_len = round_page(args->new_len);
	args->old_len = round_page(args->old_len);

	if (args->new_len > args->old_len) {
		td->td_retval[0] = 0;
		return (ENOMEM);
	}

	if (args->new_len < args->old_len) {
		addr = args->addr + args->new_len;
		len = args->old_len - args->new_len;
		error = kern_munmap(td, addr, len);
	}

	td->td_retval[0] = error ? 0 : (uintptr_t)args->addr;
	return (error);
}

#define LINUX_MS_ASYNC       0x0001
#define LINUX_MS_INVALIDATE  0x0002
#define LINUX_MS_SYNC        0x0004

int
linux_msync(struct thread *td, struct linux_msync_args *args)
{

	return (kern_msync(td, args->addr, args->len,
	    args->fl & ~LINUX_MS_SYNC));
}

#ifdef LINUX_LEGACY_SYSCALLS
int
linux_time(struct thread *td, struct linux_time_args *args)
{
	struct timeval tv;
	l_time_t tm;
	int error;

	microtime(&tv);
	tm = tv.tv_sec;
	if (args->tm && (error = copyout(&tm, args->tm, sizeof(tm))))
		return (error);
	td->td_retval[0] = tm;
	return (0);
}
#endif

struct l_times_argv {
	l_clock_t	tms_utime;
	l_clock_t	tms_stime;
	l_clock_t	tms_cutime;
	l_clock_t	tms_cstime;
};

/*
 * Glibc versions prior to 2.2.1 always use hard-coded CLK_TCK value.
 * Since 2.2.1 Glibc uses value exported from kernel via AT_CLKTCK
 * auxiliary vector entry.
 */
#define	CLK_TCK		100

#define	CONVOTCK(r)	(r.tv_sec * CLK_TCK + r.tv_usec / (1000000 / CLK_TCK))
#define	CONVNTCK(r)	(r.tv_sec * stclohz + r.tv_usec / (1000000 / stclohz))

#define	CONVTCK(r)	(linux_kernver(td) >= LINUX_KERNVER_2004000 ?		\
			    CONVNTCK(r) : CONVOTCK(r))

int
linux_times(struct thread *td, struct linux_times_args *args)
{
	struct timeval tv, utime, stime, cutime, cstime;
	struct l_times_argv tms;
	struct proc *p;
	int error;

	if (args->buf != NULL) {
		p = td->td_proc;
		PROC_LOCK(p);
		PROC_STATLOCK(p);
		calcru(p, &utime, &stime);
		PROC_STATUNLOCK(p);
		calccru(p, &cutime, &cstime);
		PROC_UNLOCK(p);

		tms.tms_utime = CONVTCK(utime);
		tms.tms_stime = CONVTCK(stime);

		tms.tms_cutime = CONVTCK(cutime);
		tms.tms_cstime = CONVTCK(cstime);

		if ((error = copyout(&tms, args->buf, sizeof(tms))))
			return (error);
	}

	microuptime(&tv);
	td->td_retval[0] = (int)CONVTCK(tv);
	return (0);
}

int
linux_newuname(struct thread *td, struct linux_newuname_args *args)
{
	struct l_new_utsname utsname;
	char osname[LINUX_MAX_UTSNAME];
	char osrelease[LINUX_MAX_UTSNAME];
	char *p;

	linux_get_osname(td, osname);
	linux_get_osrelease(td, osrelease);

	bzero(&utsname, sizeof(utsname));
	strlcpy(utsname.sysname, osname, LINUX_MAX_UTSNAME);
	getcredhostname(td->td_ucred, utsname.nodename, LINUX_MAX_UTSNAME);
	getcreddomainname(td->td_ucred, utsname.domainname, LINUX_MAX_UTSNAME);
	strlcpy(utsname.release, osrelease, LINUX_MAX_UTSNAME);
	strlcpy(utsname.version, version, LINUX_MAX_UTSNAME);
	for (p = utsname.version; *p != '\0'; ++p)
		if (*p == '\n') {
			*p = '\0';
			break;
		}
#if defined(__amd64__)
	/*
	 * On amd64, Linux uname(2) needs to return "x86_64"
	 * for both 64-bit and 32-bit applications.  On 32-bit,
	 * the string returned by getauxval(AT_PLATFORM) needs
	 * to remain "i686", though.
	 */
	strlcpy(utsname.machine, "x86_64", LINUX_MAX_UTSNAME);
#else
	strlcpy(utsname.machine, linux_kplatform, LINUX_MAX_UTSNAME);
#endif

	return (copyout(&utsname, args->buf, sizeof(utsname)));
}

struct l_utimbuf {
	l_time_t l_actime;
	l_time_t l_modtime;
};

#ifdef LINUX_LEGACY_SYSCALLS
int
linux_utime(struct thread *td, struct linux_utime_args *args)
{
	struct timeval tv[2], *tvp;
	struct l_utimbuf lut;
	char *fname;
	int error;
	bool convpath;

	convpath = LUSECONVPATH(td);
	if (convpath)
		LCONVPATHEXIST(td, args->fname, &fname);

	if (args->times) {
		if ((error = copyin(args->times, &lut, sizeof lut))) {
			if (convpath)
				LFREEPATH(fname);
			return (error);
		}
		tv[0].tv_sec = lut.l_actime;
		tv[0].tv_usec = 0;
		tv[1].tv_sec = lut.l_modtime;
		tv[1].tv_usec = 0;
		tvp = tv;
	} else
		tvp = NULL;

	if (!convpath) {
		error = kern_utimesat(td, AT_FDCWD, args->fname, UIO_USERSPACE,
		    tvp, UIO_SYSSPACE);
	} else {
		error = kern_utimesat(td, AT_FDCWD, fname, UIO_SYSSPACE, tvp,
		    UIO_SYSSPACE);
		LFREEPATH(fname);
	}
	return (error);
}
#endif

#ifdef LINUX_LEGACY_SYSCALLS
int
linux_utimes(struct thread *td, struct linux_utimes_args *args)
{
	l_timeval ltv[2];
	struct timeval tv[2], *tvp = NULL;
	char *fname;
	int error;
	bool convpath;

	convpath = LUSECONVPATH(td);
	if (convpath)
		LCONVPATHEXIST(td, args->fname, &fname);

	if (args->tptr != NULL) {
		if ((error = copyin(args->tptr, ltv, sizeof ltv))) {
			LFREEPATH(fname);
			return (error);
		}
		tv[0].tv_sec = ltv[0].tv_sec;
		tv[0].tv_usec = ltv[0].tv_usec;
		tv[1].tv_sec = ltv[1].tv_sec;
		tv[1].tv_usec = ltv[1].tv_usec;
		tvp = tv;
	}

	if (!convpath) {
		error = kern_utimesat(td, AT_FDCWD, args->fname, UIO_USERSPACE,
		    tvp, UIO_SYSSPACE);
	} else {
		error = kern_utimesat(td, AT_FDCWD, fname, UIO_SYSSPACE,
		    tvp, UIO_SYSSPACE);
		LFREEPATH(fname);
	}
	return (error);
}
#endif

static int
linux_utimensat_nsec_valid(l_long nsec)
{

	if (nsec == LINUX_UTIME_OMIT || nsec == LINUX_UTIME_NOW)
		return (0);
	if (nsec >= 0 && nsec <= 999999999)
		return (0);
	return (1);
}

int
linux_utimensat(struct thread *td, struct linux_utimensat_args *args)
{
	struct l_timespec l_times[2];
	struct timespec times[2], *timesp = NULL;
	char *path = NULL;
	int error, dfd, flags = 0;

	dfd = (args->dfd == LINUX_AT_FDCWD) ? AT_FDCWD : args->dfd;

	if (args->flags & ~LINUX_AT_SYMLINK_NOFOLLOW)
		return (EINVAL);

	if (args->times != NULL) {
		error = copyin(args->times, l_times, sizeof(l_times));
		if (error != 0)
			return (error);

		if (linux_utimensat_nsec_valid(l_times[0].tv_nsec) != 0 ||
		    linux_utimensat_nsec_valid(l_times[1].tv_nsec) != 0)
			return (EINVAL);

		times[0].tv_sec = l_times[0].tv_sec;
		switch (l_times[0].tv_nsec)
		{
		case LINUX_UTIME_OMIT:
			times[0].tv_nsec = UTIME_OMIT;
			break;
		case LINUX_UTIME_NOW:
			times[0].tv_nsec = UTIME_NOW;
			break;
		default:
			times[0].tv_nsec = l_times[0].tv_nsec;
		}

		times[1].tv_sec = l_times[1].tv_sec;
		switch (l_times[1].tv_nsec)
		{
		case LINUX_UTIME_OMIT:
			times[1].tv_nsec = UTIME_OMIT;
			break;
		case LINUX_UTIME_NOW:
			times[1].tv_nsec = UTIME_NOW;
			break;
		default:
			times[1].tv_nsec = l_times[1].tv_nsec;
			break;
		}
		timesp = times;

		/* This breaks POSIX, but is what the Linux kernel does
		 * _on purpose_ (documented in the man page for utimensat(2)),
		 * so we must follow that behaviour. */
		if (times[0].tv_nsec == UTIME_OMIT &&
		    times[1].tv_nsec == UTIME_OMIT)
			return (0);
	}

	if (!LUSECONVPATH(td)) {
		if (args->pathname != NULL) {
			return (kern_utimensat(td, dfd, args->pathname,
			    UIO_USERSPACE, timesp, UIO_SYSSPACE, flags));
		}
	}

	if (args->pathname != NULL)
		LCONVPATHEXIST_AT(td, args->pathname, &path, dfd);
	else if (args->flags != 0)
		return (EINVAL);

	if (args->flags & LINUX_AT_SYMLINK_NOFOLLOW)
		flags |= AT_SYMLINK_NOFOLLOW;

	if (path == NULL)
		error = kern_futimens(td, dfd, timesp, UIO_SYSSPACE);
	else {
		error = kern_utimensat(td, dfd, path, UIO_SYSSPACE, timesp,
			UIO_SYSSPACE, flags);
		LFREEPATH(path);
	}

	return (error);
}

#ifdef LINUX_LEGACY_SYSCALLS
int
linux_futimesat(struct thread *td, struct linux_futimesat_args *args)
{
	l_timeval ltv[2];
	struct timeval tv[2], *tvp = NULL;
	char *fname;
	int error, dfd;
	bool convpath;

	convpath = LUSECONVPATH(td);
	dfd = (args->dfd == LINUX_AT_FDCWD) ? AT_FDCWD : args->dfd;
	if (convpath)
		LCONVPATHEXIST_AT(td, args->filename, &fname, dfd);

	if (args->utimes != NULL) {
		if ((error = copyin(args->utimes, ltv, sizeof ltv))) {
			if (convpath)
				LFREEPATH(fname);
			return (error);
		}
		tv[0].tv_sec = ltv[0].tv_sec;
		tv[0].tv_usec = ltv[0].tv_usec;
		tv[1].tv_sec = ltv[1].tv_sec;
		tv[1].tv_usec = ltv[1].tv_usec;
		tvp = tv;
	}

	if (!convpath) {
		error = kern_utimesat(td, dfd, args->filename, UIO_USERSPACE,
		    tvp, UIO_SYSSPACE);
	} else {
		error = kern_utimesat(td, dfd, fname, UIO_SYSSPACE, tvp, UIO_SYSSPACE);
		LFREEPATH(fname);
	}
	return (error);
}
#endif

static int
linux_common_wait(struct thread *td, int pid, int *statusp,
    int options, struct __wrusage *wrup)
{
	siginfo_t siginfo;
	idtype_t idtype;
	id_t id;
	int error, status, tmpstat;

	if (pid == WAIT_ANY) {
		idtype = P_ALL;
		id = 0;
	} else if (pid < 0) {
		idtype = P_PGID;
		id = (id_t)-pid;
	} else {
		idtype = P_PID;
		id = (id_t)pid;
	}

	/*
	 * For backward compatibility we implicitly add flags WEXITED
	 * and WTRAPPED here.
	 */
	options |= WEXITED | WTRAPPED;
	error = kern_wait6(td, idtype, id, &status, options, wrup, &siginfo);
	if (error)
		return (error);

	if (statusp) {
		tmpstat = status & 0xffff;
		if (WIFSIGNALED(tmpstat)) {
			tmpstat = (tmpstat & 0xffffff80) |
			    bsd_to_linux_signal(WTERMSIG(tmpstat));
		} else if (WIFSTOPPED(tmpstat)) {
			tmpstat = (tmpstat & 0xffff00ff) |
			    (bsd_to_linux_signal(WSTOPSIG(tmpstat)) << 8);
#if defined(__amd64__) && !defined(COMPAT_LINUX32)
			if (WSTOPSIG(status) == SIGTRAP) {
				tmpstat = linux_ptrace_status(td,
				    siginfo.si_pid, tmpstat);
			}
#endif
		} else if (WIFCONTINUED(tmpstat)) {
			tmpstat = 0xffff;
		}
		error = copyout(&tmpstat, statusp, sizeof(int));
	}

	return (error);
}

#if defined(__i386__) || (defined(__amd64__) && defined(COMPAT_LINUX32))
int
linux_waitpid(struct thread *td, struct linux_waitpid_args *args)
{
	struct linux_wait4_args wait4_args;

	wait4_args.pid = args->pid;
	wait4_args.status = args->status;
	wait4_args.options = args->options;
	wait4_args.rusage = NULL;

	return (linux_wait4(td, &wait4_args));
}
#endif /* __i386__ || (__amd64__ && COMPAT_LINUX32) */

int
linux_wait4(struct thread *td, struct linux_wait4_args *args)
{
	int error, options;
	struct __wrusage wru, *wrup;

	if (args->options & ~(LINUX_WUNTRACED | LINUX_WNOHANG |
	    LINUX_WCONTINUED | __WCLONE | __WNOTHREAD | __WALL))
		return (EINVAL);

	options = WEXITED;
	linux_to_bsd_waitopts(args->options, &options);

	if (args->rusage != NULL)
		wrup = &wru;
	else
		wrup = NULL;
	error = linux_common_wait(td, args->pid, args->status, options, wrup);
	if (error != 0)
		return (error);
	if (args->rusage != NULL)
		error = linux_copyout_rusage(&wru.wru_self, args->rusage);
	return (error);
}

int
linux_waitid(struct thread *td, struct linux_waitid_args *args)
{
	int status, options, sig;
	struct __wrusage wru;
	siginfo_t siginfo;
	l_siginfo_t lsi;
	idtype_t idtype;
	struct proc *p;
	int error;

	options = 0;
	linux_to_bsd_waitopts(args->options, &options);

	if (options & ~(WNOHANG | WNOWAIT | WEXITED | WUNTRACED | WCONTINUED))
		return (EINVAL);
	if (!(options & (WEXITED | WUNTRACED | WCONTINUED)))
		return (EINVAL);

	switch (args->idtype) {
	case LINUX_P_ALL:
		idtype = P_ALL;
		break;
	case LINUX_P_PID:
		if (args->id <= 0)
			return (EINVAL);
		idtype = P_PID;
		break;
	case LINUX_P_PGID:
		if (args->id <= 0)
			return (EINVAL);
		idtype = P_PGID;
		break;
	default:
		return (EINVAL);
	}

	error = kern_wait6(td, idtype, args->id, &status, options,
	    &wru, &siginfo);
	if (error != 0)
		return (error);
	if (args->rusage != NULL) {
		error = linux_copyout_rusage(&wru.wru_children,
		    args->rusage);
		if (error != 0)
			return (error);
	}
	if (args->info != NULL) {
		p = td->td_proc;
		bzero(&lsi, sizeof(lsi));
		if (td->td_retval[0] != 0) {
			sig = bsd_to_linux_signal(siginfo.si_signo);
			siginfo_to_lsiginfo(&siginfo, &lsi, sig);
		}
		error = copyout(&lsi, args->info, sizeof(lsi));
	}
	td->td_retval[0] = 0;

	return (error);
}

#ifdef LINUX_LEGACY_SYSCALLS
int
linux_mknod(struct thread *td, struct linux_mknod_args *args)
{
	char *path;
	int error;
	enum uio_seg seg;
	bool convpath;

	convpath = LUSECONVPATH(td);
	if (!convpath) {
		path = args->path;
		seg = UIO_USERSPACE;
	} else {
		LCONVPATHCREAT(td, args->path, &path);
		seg = UIO_SYSSPACE;
	}

	switch (args->mode & S_IFMT) {
	case S_IFIFO:
	case S_IFSOCK:
		error = kern_mkfifoat(td, AT_FDCWD, path, seg,
		    args->mode);
		break;

	case S_IFCHR:
	case S_IFBLK:
		error = kern_mknodat(td, AT_FDCWD, path, seg,
		    args->mode, args->dev);
		break;

	case S_IFDIR:
		error = EPERM;
		break;

	case 0:
		args->mode |= S_IFREG;
		/* FALLTHROUGH */
	case S_IFREG:
		error = kern_openat(td, AT_FDCWD, path, seg,
		    O_WRONLY | O_CREAT | O_TRUNC, args->mode);
		if (error == 0)
			kern_close(td, td->td_retval[0]);
		break;

	default:
		error = EINVAL;
		break;
	}
	if (convpath)
		LFREEPATH(path);
	return (error);
}
#endif

int
linux_mknodat(struct thread *td, struct linux_mknodat_args *args)
{
	char *path;
	int error, dfd;
	enum uio_seg seg;
	bool convpath;

	dfd = (args->dfd == LINUX_AT_FDCWD) ? AT_FDCWD : args->dfd;

	convpath = LUSECONVPATH(td);
	if (!convpath) {
		path = __DECONST(char *, args->filename);
		seg = UIO_USERSPACE;
	} else {
		LCONVPATHCREAT_AT(td, args->filename, &path, dfd);
		seg = UIO_SYSSPACE;
	}

	switch (args->mode & S_IFMT) {
	case S_IFIFO:
	case S_IFSOCK:
		error = kern_mkfifoat(td, dfd, path, seg, args->mode);
		break;

	case S_IFCHR:
	case S_IFBLK:
		error = kern_mknodat(td, dfd, path, seg, args->mode,
		    args->dev);
		break;

	case S_IFDIR:
		error = EPERM;
		break;

	case 0:
		args->mode |= S_IFREG;
		/* FALLTHROUGH */
	case S_IFREG:
		error = kern_openat(td, dfd, path, seg,
		    O_WRONLY | O_CREAT | O_TRUNC, args->mode);
		if (error == 0)
			kern_close(td, td->td_retval[0]);
		break;

	default:
		error = EINVAL;
		break;
	}
	if (convpath)
		LFREEPATH(path);
	return (error);
}

/*
 * UGH! This is just about the dumbest idea I've ever heard!!
 */
int
linux_personality(struct thread *td, struct linux_personality_args *args)
{
	struct linux_pemuldata *pem;
	struct proc *p = td->td_proc;
	uint32_t old;

	PROC_LOCK(p);
	pem = pem_find(p);
	old = pem->persona;
	if (args->per != 0xffffffff)
		pem->persona = args->per;
	PROC_UNLOCK(p);

	td->td_retval[0] = old;
	return (0);
}

struct l_itimerval {
	l_timeval it_interval;
	l_timeval it_value;
};

#define	B2L_ITIMERVAL(bip, lip)						\
	(bip)->it_interval.tv_sec = (lip)->it_interval.tv_sec;		\
	(bip)->it_interval.tv_usec = (lip)->it_interval.tv_usec;	\
	(bip)->it_value.tv_sec = (lip)->it_value.tv_sec;		\
	(bip)->it_value.tv_usec = (lip)->it_value.tv_usec;

int
linux_setitimer(struct thread *td, struct linux_setitimer_args *uap)
{
	int error;
	struct l_itimerval ls;
	struct itimerval aitv, oitv;

	if (uap->itv == NULL) {
		uap->itv = uap->oitv;
		return (linux_getitimer(td, (struct linux_getitimer_args *)uap));
	}

	error = copyin(uap->itv, &ls, sizeof(ls));
	if (error != 0)
		return (error);
	B2L_ITIMERVAL(&aitv, &ls);
	error = kern_setitimer(td, uap->which, &aitv, &oitv);
	if (error != 0 || uap->oitv == NULL)
		return (error);
	B2L_ITIMERVAL(&ls, &oitv);

	return (copyout(&ls, uap->oitv, sizeof(ls)));
}

int
linux_getitimer(struct thread *td, struct linux_getitimer_args *uap)
{
	int error;
	struct l_itimerval ls;
	struct itimerval aitv;

	error = kern_getitimer(td, uap->which, &aitv);
	if (error != 0)
		return (error);
	B2L_ITIMERVAL(&ls, &aitv);
	return (copyout(&ls, uap->itv, sizeof(ls)));
}

#if defined(__i386__) || (defined(__amd64__) && defined(COMPAT_LINUX32))
int
linux_nice(struct thread *td, struct linux_nice_args *args)
{

	return (kern_setpriority(td, PRIO_PROCESS, 0, args->inc));
}
#endif /* __i386__ || (__amd64__ && COMPAT_LINUX32) */

int
linux_setgroups(struct thread *td, struct linux_setgroups_args *args)
{
	struct ucred *newcred, *oldcred;
	l_gid_t *linux_gidset;
	gid_t *bsd_gidset;
	int ngrp, error;
	struct proc *p;

	ngrp = args->gidsetsize;
	if (ngrp < 0 || ngrp >= ngroups_max + 1)
		return (EINVAL);
	linux_gidset = malloc(ngrp * sizeof(*linux_gidset), M_LINUX, M_WAITOK);
	error = copyin(args->grouplist, linux_gidset, ngrp * sizeof(l_gid_t));
	if (error)
		goto out;
	newcred = crget();
	crextend(newcred, ngrp + 1);
	p = td->td_proc;
	PROC_LOCK(p);
	oldcred = p->p_ucred;
	crcopy(newcred, oldcred);

	/*
	 * cr_groups[0] holds egid. Setting the whole set from
	 * the supplied set will cause egid to be changed too.
	 * Keep cr_groups[0] unchanged to prevent that.
	 */

	if ((error = priv_check_cred(oldcred, PRIV_CRED_SETGROUPS)) != 0) {
		PROC_UNLOCK(p);
		crfree(newcred);
		goto out;
	}

	if (ngrp > 0) {
		newcred->cr_ngroups = ngrp + 1;

		bsd_gidset = newcred->cr_groups;
		ngrp--;
		while (ngrp >= 0) {
			bsd_gidset[ngrp + 1] = linux_gidset[ngrp];
			ngrp--;
		}
	} else
		newcred->cr_ngroups = 1;

	setsugid(p);
	proc_set_cred(p, newcred);
	PROC_UNLOCK(p);
	crfree(oldcred);
	error = 0;
out:
	free(linux_gidset, M_LINUX);
	return (error);
}

int
linux_getgroups(struct thread *td, struct linux_getgroups_args *args)
{
	struct ucred *cred;
	l_gid_t *linux_gidset;
	gid_t *bsd_gidset;
	int bsd_gidsetsz, ngrp, error;

	cred = td->td_ucred;
	bsd_gidset = cred->cr_groups;
	bsd_gidsetsz = cred->cr_ngroups - 1;

	/*
	 * cr_groups[0] holds egid. Returning the whole set
	 * here will cause a duplicate. Exclude cr_groups[0]
	 * to prevent that.
	 */

	if ((ngrp = args->gidsetsize) == 0) {
		td->td_retval[0] = bsd_gidsetsz;
		return (0);
	}

	if (ngrp < bsd_gidsetsz)
		return (EINVAL);

	ngrp = 0;
	linux_gidset = malloc(bsd_gidsetsz * sizeof(*linux_gidset),
	    M_LINUX, M_WAITOK);
	while (ngrp < bsd_gidsetsz) {
		linux_gidset[ngrp] = bsd_gidset[ngrp + 1];
		ngrp++;
	}

	error = copyout(linux_gidset, args->grouplist, ngrp * sizeof(l_gid_t));
	free(linux_gidset, M_LINUX);
	if (error)
		return (error);

	td->td_retval[0] = ngrp;
	return (0);
}

static bool
linux_get_dummy_limit(l_uint resource, struct rlimit *rlim)
{

	if (linux_dummy_rlimits == 0)
		return (false);

	switch (resource) {
	case LINUX_RLIMIT_LOCKS:
	case LINUX_RLIMIT_SIGPENDING:
	case LINUX_RLIMIT_MSGQUEUE:
	case LINUX_RLIMIT_RTTIME:
		rlim->rlim_cur = LINUX_RLIM_INFINITY;
		rlim->rlim_max = LINUX_RLIM_INFINITY;
		return (true);
	case LINUX_RLIMIT_NICE:
	case LINUX_RLIMIT_RTPRIO:
		rlim->rlim_cur = 0;
		rlim->rlim_max = 0;
		return (true);
	default:
		return (false);
	}
}

int
linux_setrlimit(struct thread *td, struct linux_setrlimit_args *args)
{
	struct rlimit bsd_rlim;
	struct l_rlimit rlim;
	u_int which;
	int error;

	if (args->resource >= LINUX_RLIM_NLIMITS)
		return (EINVAL);

	which = linux_to_bsd_resource[args->resource];
	if (which == -1)
		return (EINVAL);

	error = copyin(args->rlim, &rlim, sizeof(rlim));
	if (error)
		return (error);

	bsd_rlim.rlim_cur = (rlim_t)rlim.rlim_cur;
	bsd_rlim.rlim_max = (rlim_t)rlim.rlim_max;
	return (kern_setrlimit(td, which, &bsd_rlim));
}

#if defined(__i386__) || (defined(__amd64__) && defined(COMPAT_LINUX32))
int
linux_old_getrlimit(struct thread *td, struct linux_old_getrlimit_args *args)
{
	struct l_rlimit rlim;
	struct rlimit bsd_rlim;
	u_int which;

	if (linux_get_dummy_limit(args->resource, &bsd_rlim)) {
		rlim.rlim_cur = bsd_rlim.rlim_cur;
		rlim.rlim_max = bsd_rlim.rlim_max;
		return (copyout(&rlim, args->rlim, sizeof(rlim)));
	}

	if (args->resource >= LINUX_RLIM_NLIMITS)
		return (EINVAL);

	which = linux_to_bsd_resource[args->resource];
	if (which == -1)
		return (EINVAL);

	lim_rlimit(td, which, &bsd_rlim);

#ifdef COMPAT_LINUX32
	rlim.rlim_cur = (unsigned int)bsd_rlim.rlim_cur;
	if (rlim.rlim_cur == UINT_MAX)
		rlim.rlim_cur = INT_MAX;
	rlim.rlim_max = (unsigned int)bsd_rlim.rlim_max;
	if (rlim.rlim_max == UINT_MAX)
		rlim.rlim_max = INT_MAX;
#else
	rlim.rlim_cur = (unsigned long)bsd_rlim.rlim_cur;
	if (rlim.rlim_cur == ULONG_MAX)
		rlim.rlim_cur = LONG_MAX;
	rlim.rlim_max = (unsigned long)bsd_rlim.rlim_max;
	if (rlim.rlim_max == ULONG_MAX)
		rlim.rlim_max = LONG_MAX;
#endif
	return (copyout(&rlim, args->rlim, sizeof(rlim)));
}
#endif /* __i386__ || (__amd64__ && COMPAT_LINUX32) */

int
linux_getrlimit(struct thread *td, struct linux_getrlimit_args *args)
{
	struct l_rlimit rlim;
	struct rlimit bsd_rlim;
	u_int which;

	if (linux_get_dummy_limit(args->resource, &bsd_rlim)) {
		rlim.rlim_cur = bsd_rlim.rlim_cur;
		rlim.rlim_max = bsd_rlim.rlim_max;
		return (copyout(&rlim, args->rlim, sizeof(rlim)));
	}

	if (args->resource >= LINUX_RLIM_NLIMITS)
		return (EINVAL);

	which = linux_to_bsd_resource[args->resource];
	if (which == -1)
		return (EINVAL);

	lim_rlimit(td, which, &bsd_rlim);

	rlim.rlim_cur = (l_ulong)bsd_rlim.rlim_cur;
	rlim.rlim_max = (l_ulong)bsd_rlim.rlim_max;
	return (copyout(&rlim, args->rlim, sizeof(rlim)));
}

int
linux_sched_setscheduler(struct thread *td,
    struct linux_sched_setscheduler_args *args)
{
	struct sched_param sched_param;
	struct thread *tdt;
	int error, policy;

	switch (args->policy) {
	case LINUX_SCHED_OTHER:
		policy = SCHED_OTHER;
		break;
	case LINUX_SCHED_FIFO:
		policy = SCHED_FIFO;
		break;
	case LINUX_SCHED_RR:
		policy = SCHED_RR;
		break;
	default:
		return (EINVAL);
	}

	error = copyin(args->param, &sched_param, sizeof(sched_param));
	if (error)
		return (error);

	if (linux_map_sched_prio) {
		switch (policy) {
		case SCHED_OTHER:
			if (sched_param.sched_priority != 0)
				return (EINVAL);

			sched_param.sched_priority =
			    PRI_MAX_TIMESHARE - PRI_MIN_TIMESHARE;
			break;
		case SCHED_FIFO:
		case SCHED_RR:
			if (sched_param.sched_priority < 1 ||
			    sched_param.sched_priority >= LINUX_MAX_RT_PRIO)
				return (EINVAL);

			/*
			 * Map [1, LINUX_MAX_RT_PRIO - 1] to
			 * [0, RTP_PRIO_MAX - RTP_PRIO_MIN] (rounding down).
			 */
			sched_param.sched_priority =
			    (sched_param.sched_priority - 1) *
			    (RTP_PRIO_MAX - RTP_PRIO_MIN + 1) /
			    (LINUX_MAX_RT_PRIO - 1);
			break;
		}
	}

	tdt = linux_tdfind(td, args->pid, -1);
	if (tdt == NULL)
		return (ESRCH);

	error = kern_sched_setscheduler(td, tdt, policy, &sched_param);
	PROC_UNLOCK(tdt->td_proc);
	return (error);
}

int
linux_sched_getscheduler(struct thread *td,
    struct linux_sched_getscheduler_args *args)
{
	struct thread *tdt;
	int error, policy;

	tdt = linux_tdfind(td, args->pid, -1);
	if (tdt == NULL)
		return (ESRCH);

	error = kern_sched_getscheduler(td, tdt, &policy);
	PROC_UNLOCK(tdt->td_proc);

	switch (policy) {
	case SCHED_OTHER:
		td->td_retval[0] = LINUX_SCHED_OTHER;
		break;
	case SCHED_FIFO:
		td->td_retval[0] = LINUX_SCHED_FIFO;
		break;
	case SCHED_RR:
		td->td_retval[0] = LINUX_SCHED_RR;
		break;
	}
	return (error);
}

int
linux_sched_get_priority_max(struct thread *td,
    struct linux_sched_get_priority_max_args *args)
{
	struct sched_get_priority_max_args bsd;

	if (linux_map_sched_prio) {
		switch (args->policy) {
		case LINUX_SCHED_OTHER:
			td->td_retval[0] = 0;
			return (0);
		case LINUX_SCHED_FIFO:
		case LINUX_SCHED_RR:
			td->td_retval[0] = LINUX_MAX_RT_PRIO - 1;
			return (0);
		default:
			return (EINVAL);
		}
	}

	switch (args->policy) {
	case LINUX_SCHED_OTHER:
		bsd.policy = SCHED_OTHER;
		break;
	case LINUX_SCHED_FIFO:
		bsd.policy = SCHED_FIFO;
		break;
	case LINUX_SCHED_RR:
		bsd.policy = SCHED_RR;
		break;
	default:
		return (EINVAL);
	}
	return (sys_sched_get_priority_max(td, &bsd));
}

int
linux_sched_get_priority_min(struct thread *td,
    struct linux_sched_get_priority_min_args *args)
{
	struct sched_get_priority_min_args bsd;

	if (linux_map_sched_prio) {
		switch (args->policy) {
		case LINUX_SCHED_OTHER:
			td->td_retval[0] = 0;
			return (0);
		case LINUX_SCHED_FIFO:
		case LINUX_SCHED_RR:
			td->td_retval[0] = 1;
			return (0);
		default:
			return (EINVAL);
		}
	}

	switch (args->policy) {
	case LINUX_SCHED_OTHER:
		bsd.policy = SCHED_OTHER;
		break;
	case LINUX_SCHED_FIFO:
		bsd.policy = SCHED_FIFO;
		break;
	case LINUX_SCHED_RR:
		bsd.policy = SCHED_RR;
		break;
	default:
		return (EINVAL);
	}
	return (sys_sched_get_priority_min(td, &bsd));
}

#define REBOOT_CAD_ON	0x89abcdef
#define REBOOT_CAD_OFF	0
#define REBOOT_HALT	0xcdef0123
#define REBOOT_RESTART	0x01234567
#define REBOOT_RESTART2	0xA1B2C3D4
#define REBOOT_POWEROFF	0x4321FEDC
#define REBOOT_MAGIC1	0xfee1dead
#define REBOOT_MAGIC2	0x28121969
#define REBOOT_MAGIC2A	0x05121996
#define REBOOT_MAGIC2B	0x16041998

int
linux_reboot(struct thread *td, struct linux_reboot_args *args)
{
	struct reboot_args bsd_args;

	if (args->magic1 != REBOOT_MAGIC1)
		return (EINVAL);

	switch (args->magic2) {
	case REBOOT_MAGIC2:
	case REBOOT_MAGIC2A:
	case REBOOT_MAGIC2B:
		break;
	default:
		return (EINVAL);
	}

	switch (args->cmd) {
	case REBOOT_CAD_ON:
	case REBOOT_CAD_OFF:
		return (priv_check(td, PRIV_REBOOT));
	case REBOOT_HALT:
		bsd_args.opt = RB_HALT;
		break;
	case REBOOT_RESTART:
	case REBOOT_RESTART2:
		bsd_args.opt = 0;
		break;
	case REBOOT_POWEROFF:
		bsd_args.opt = RB_POWEROFF;
		break;
	default:
		return (EINVAL);
	}
	return (sys_reboot(td, &bsd_args));
}

int
linux_getpid(struct thread *td, struct linux_getpid_args *args)
{

	td->td_retval[0] = td->td_proc->p_pid;

	return (0);
}

int
linux_gettid(struct thread *td, struct linux_gettid_args *args)
{
	struct linux_emuldata *em;

	em = em_find(td);
	KASSERT(em != NULL, ("gettid: emuldata not found.\n"));

	td->td_retval[0] = em->em_tid;

	return (0);
}

int
linux_getppid(struct thread *td, struct linux_getppid_args *args)
{

	td->td_retval[0] = kern_getppid(td);
	return (0);
}

int
linux_getgid(struct thread *td, struct linux_getgid_args *args)
{

	td->td_retval[0] = td->td_ucred->cr_rgid;
	return (0);
}

int
linux_getuid(struct thread *td, struct linux_getuid_args *args)
{

	td->td_retval[0] = td->td_ucred->cr_ruid;
	return (0);
}

int
linux_getsid(struct thread *td, struct linux_getsid_args *args)
{

	return (kern_getsid(td, args->pid));
}

int
linux_nosys(struct thread *td, struct nosys_args *ignore)
{

	return (ENOSYS);
}

int
linux_getpriority(struct thread *td, struct linux_getpriority_args *args)
{
	int error;

	error = kern_getpriority(td, args->which, args->who);
	td->td_retval[0] = 20 - td->td_retval[0];
	return (error);
}

int
linux_sethostname(struct thread *td, struct linux_sethostname_args *args)
{
	int name[2];

	name[0] = CTL_KERN;
	name[1] = KERN_HOSTNAME;
	return (userland_sysctl(td, name, 2, 0, 0, 0, args->hostname,
	    args->len, 0, 0));
}

int
linux_setdomainname(struct thread *td, struct linux_setdomainname_args *args)
{
	int name[2];

	name[0] = CTL_KERN;
	name[1] = KERN_NISDOMAINNAME;
	return (userland_sysctl(td, name, 2, 0, 0, 0, args->name,
	    args->len, 0, 0));
}

int
linux_exit_group(struct thread *td, struct linux_exit_group_args *args)
{

	LINUX_CTR2(exit_group, "thread(%d) (%d)", td->td_tid,
	    args->error_code);

	/*
	 * XXX: we should send a signal to the parent if
	 * SIGNAL_EXIT_GROUP is set. We ignore that (temporarily?)
	 * as it doesnt occur often.
	 */
	exit1(td, args->error_code, 0);
		/* NOTREACHED */
}

#define _LINUX_CAPABILITY_VERSION_1  0x19980330
#define _LINUX_CAPABILITY_VERSION_2  0x20071026
#define _LINUX_CAPABILITY_VERSION_3  0x20080522

struct l_user_cap_header {
	l_int	version;
	l_int	pid;
};

struct l_user_cap_data {
	l_int	effective;
	l_int	permitted;
	l_int	inheritable;
};

int
linux_capget(struct thread *td, struct linux_capget_args *uap)
{
	struct l_user_cap_header luch;
	struct l_user_cap_data lucd[2];
	int error, u32s;

	if (uap->hdrp == NULL)
		return (EFAULT);

	error = copyin(uap->hdrp, &luch, sizeof(luch));
	if (error != 0)
		return (error);

	switch (luch.version) {
	case _LINUX_CAPABILITY_VERSION_1:
		u32s = 1;
		break;
	case _LINUX_CAPABILITY_VERSION_2:
	case _LINUX_CAPABILITY_VERSION_3:
		u32s = 2;
		break;
	default:
		luch.version = _LINUX_CAPABILITY_VERSION_1;
		error = copyout(&luch, uap->hdrp, sizeof(luch));
		if (error)
			return (error);
		return (EINVAL);
	}

	if (luch.pid)
		return (EPERM);

	if (uap->datap) {
		/*
		 * The current implementation doesn't support setting
		 * a capability (it's essentially a stub) so indicate
		 * that no capabilities are currently set or available
		 * to request.
		 */
		memset(&lucd, 0, u32s * sizeof(lucd[0]));
		error = copyout(&lucd, uap->datap, u32s * sizeof(lucd[0]));
	}

	return (error);
}

int
linux_capset(struct thread *td, struct linux_capset_args *uap)
{
	struct l_user_cap_header luch;
	struct l_user_cap_data lucd[2];
	int error, i, u32s;

	if (uap->hdrp == NULL || uap->datap == NULL)
		return (EFAULT);

	error = copyin(uap->hdrp, &luch, sizeof(luch));
	if (error != 0)
		return (error);

	switch (luch.version) {
	case _LINUX_CAPABILITY_VERSION_1:
		u32s = 1;
		break;
	case _LINUX_CAPABILITY_VERSION_2:
	case _LINUX_CAPABILITY_VERSION_3:
		u32s = 2;
		break;
	default:
		luch.version = _LINUX_CAPABILITY_VERSION_1;
		error = copyout(&luch, uap->hdrp, sizeof(luch));
		if (error)
			return (error);
		return (EINVAL);
	}

	if (luch.pid)
		return (EPERM);

	error = copyin(uap->datap, &lucd, u32s * sizeof(lucd[0]));
	if (error != 0)
		return (error);

	/* We currently don't support setting any capabilities. */
	for (i = 0; i < u32s; i++) {
		if (lucd[i].effective || lucd[i].permitted ||
		    lucd[i].inheritable) {
			linux_msg(td,
			    "capset[%d] effective=0x%x, permitted=0x%x, "
			    "inheritable=0x%x is not implemented", i,
			    (int)lucd[i].effective, (int)lucd[i].permitted,
			    (int)lucd[i].inheritable);
			return (EPERM);
		}
	}

	return (0);
}

int
linux_prctl(struct thread *td, struct linux_prctl_args *args)
{
	int error = 0, max_size;
	struct proc *p = td->td_proc;
	char comm[LINUX_MAX_COMM_LEN];
	int pdeath_signal, trace_state;

	switch (args->option) {
	case LINUX_PR_SET_PDEATHSIG:
		if (!LINUX_SIG_VALID(args->arg2))
			return (EINVAL);
		pdeath_signal = linux_to_bsd_signal(args->arg2);
		return (kern_procctl(td, P_PID, 0, PROC_PDEATHSIG_CTL,
		    &pdeath_signal));
	case LINUX_PR_GET_PDEATHSIG:
		error = kern_procctl(td, P_PID, 0, PROC_PDEATHSIG_STATUS,
		    &pdeath_signal);
		if (error != 0)
			return (error);
		pdeath_signal = bsd_to_linux_signal(pdeath_signal);
		return (copyout(&pdeath_signal,
		    (void *)(register_t)args->arg2,
		    sizeof(pdeath_signal)));
	/*
	 * In Linux, this flag controls if set[gu]id processes can coredump.
	 * There are additional semantics imposed on processes that cannot
	 * coredump:
	 * - Such processes can not be ptraced.
	 * - There are some semantics around ownership of process-related files
	 *   in the /proc namespace.
	 *
	 * In FreeBSD, we can (and by default, do) disable setuid coredump
	 * system-wide with 'sugid_coredump.'  We control tracability on a
	 * per-process basis with the procctl PROC_TRACE (=> P2_NOTRACE flag).
	 * By happy coincidence, P2_NOTRACE also prevents coredumping.  So the
	 * procctl is roughly analogous to Linux's DUMPABLE.
	 *
	 * So, proxy these knobs to the corresponding PROC_TRACE setting.
	 */
	case LINUX_PR_GET_DUMPABLE:
		error = kern_procctl(td, P_PID, p->p_pid, PROC_TRACE_STATUS,
		    &trace_state);
		if (error != 0)
			return (error);
		td->td_retval[0] = (trace_state != -1);
		return (0);
	case LINUX_PR_SET_DUMPABLE:
		/*
		 * It is only valid for userspace to set one of these two
		 * flags, and only one at a time.
		 */
		switch (args->arg2) {
		case LINUX_SUID_DUMP_DISABLE:
			trace_state = PROC_TRACE_CTL_DISABLE_EXEC;
			break;
		case LINUX_SUID_DUMP_USER:
			trace_state = PROC_TRACE_CTL_ENABLE;
			break;
		default:
			return (EINVAL);
		}
		return (kern_procctl(td, P_PID, p->p_pid, PROC_TRACE_CTL,
		    &trace_state));
	case LINUX_PR_GET_KEEPCAPS:
		/*
		 * Indicate that we always clear the effective and
		 * permitted capability sets when the user id becomes
		 * non-zero (actually the capability sets are simply
		 * always zero in the current implementation).
		 */
		td->td_retval[0] = 0;
		break;
	case LINUX_PR_SET_KEEPCAPS:
		/*
		 * Ignore requests to keep the effective and permitted
		 * capability sets when the user id becomes non-zero.
		 */
		break;
	case LINUX_PR_SET_NAME:
		/*
		 * To be on the safe side we need to make sure to not
		 * overflow the size a Linux program expects. We already
		 * do this here in the copyin, so that we don't need to
		 * check on copyout.
		 */
		max_size = MIN(sizeof(comm), sizeof(p->p_comm));
		error = copyinstr((void *)(register_t)args->arg2, comm,
		    max_size, NULL);

		/* Linux silently truncates the name if it is too long. */
		if (error == ENAMETOOLONG) {
			/*
			 * XXX: copyinstr() isn't documented to populate the
			 * array completely, so do a copyin() to be on the
			 * safe side. This should be changed in case
			 * copyinstr() is changed to guarantee this.
			 */
			error = copyin((void *)(register_t)args->arg2, comm,
			    max_size - 1);
			comm[max_size - 1] = '\0';
		}
		if (error)
			return (error);

		PROC_LOCK(p);
		strlcpy(p->p_comm, comm, sizeof(p->p_comm));
		PROC_UNLOCK(p);
		break;
	case LINUX_PR_GET_NAME:
		PROC_LOCK(p);
		strlcpy(comm, p->p_comm, sizeof(comm));
		PROC_UNLOCK(p);
		error = copyout(comm, (void *)(register_t)args->arg2,
		    strlen(comm) + 1);
		break;
	case LINUX_PR_GET_SECCOMP:
	case LINUX_PR_SET_SECCOMP:
		/*
		 * Same as returned by Linux without CONFIG_SECCOMP enabled.
		 */
		error = EINVAL;
		break;
	case LINUX_PR_SET_NO_NEW_PRIVS:
		linux_msg(td, "unsupported prctl PR_SET_NO_NEW_PRIVS");
		error = EINVAL;
		break;
	case LINUX_PR_SET_PTRACER:
		linux_msg(td, "unsupported prctl PR_SET_PTRACER");
		error = EINVAL;
		break;
	default:
		linux_msg(td, "unsupported prctl option %d", args->option);
		error = EINVAL;
		break;
	}

	return (error);
}

int
linux_sched_setparam(struct thread *td,
    struct linux_sched_setparam_args *uap)
{
	struct sched_param sched_param;
	struct thread *tdt;
	int error, policy;

	error = copyin(uap->param, &sched_param, sizeof(sched_param));
	if (error)
		return (error);

	tdt = linux_tdfind(td, uap->pid, -1);
	if (tdt == NULL)
		return (ESRCH);

	if (linux_map_sched_prio) {
		error = kern_sched_getscheduler(td, tdt, &policy);
		if (error)
			goto out;

		switch (policy) {
		case SCHED_OTHER:
			if (sched_param.sched_priority != 0) {
				error = EINVAL;
				goto out;
			}
			sched_param.sched_priority =
			    PRI_MAX_TIMESHARE - PRI_MIN_TIMESHARE;
			break;
		case SCHED_FIFO:
		case SCHED_RR:
			if (sched_param.sched_priority < 1 ||
			    sched_param.sched_priority >= LINUX_MAX_RT_PRIO) {
				error = EINVAL;
				goto out;
			}
			/*
			 * Map [1, LINUX_MAX_RT_PRIO - 1] to
			 * [0, RTP_PRIO_MAX - RTP_PRIO_MIN] (rounding down).
			 */
			sched_param.sched_priority =
			    (sched_param.sched_priority - 1) *
			    (RTP_PRIO_MAX - RTP_PRIO_MIN + 1) /
			    (LINUX_MAX_RT_PRIO - 1);
			break;
		}
	}

	error = kern_sched_setparam(td, tdt, &sched_param);
out:	PROC_UNLOCK(tdt->td_proc);
	return (error);
}

int
linux_sched_getparam(struct thread *td,
    struct linux_sched_getparam_args *uap)
{
	struct sched_param sched_param;
	struct thread *tdt;
	int error, policy;

	tdt = linux_tdfind(td, uap->pid, -1);
	if (tdt == NULL)
		return (ESRCH);

	error = kern_sched_getparam(td, tdt, &sched_param);
	if (error) {
		PROC_UNLOCK(tdt->td_proc);
		return (error);
	}

	if (linux_map_sched_prio) {
		error = kern_sched_getscheduler(td, tdt, &policy);
		PROC_UNLOCK(tdt->td_proc);
		if (error)
			return (error);

		switch (policy) {
		case SCHED_OTHER:
			sched_param.sched_priority = 0;
			break;
		case SCHED_FIFO:
		case SCHED_RR:
			/*
			 * Map [0, RTP_PRIO_MAX - RTP_PRIO_MIN] to
			 * [1, LINUX_MAX_RT_PRIO - 1] (rounding up).
			 */
			sched_param.sched_priority =
			    (sched_param.sched_priority *
			    (LINUX_MAX_RT_PRIO - 1) +
			    (RTP_PRIO_MAX - RTP_PRIO_MIN - 1)) /
			    (RTP_PRIO_MAX - RTP_PRIO_MIN) + 1;
			break;
		}
	} else
		PROC_UNLOCK(tdt->td_proc);

	error = copyout(&sched_param, uap->param, sizeof(sched_param));
	return (error);
}

/*
 * Get affinity of a process.
 */
int
linux_sched_getaffinity(struct thread *td,
    struct linux_sched_getaffinity_args *args)
{
	int error;
	struct thread *tdt;

	if (args->len < sizeof(cpuset_t))
		return (EINVAL);

	tdt = linux_tdfind(td, args->pid, -1);
	if (tdt == NULL)
		return (ESRCH);

	PROC_UNLOCK(tdt->td_proc);

	error = kern_cpuset_getaffinity(td, CPU_LEVEL_WHICH, CPU_WHICH_TID,
	    tdt->td_tid, sizeof(cpuset_t), (cpuset_t *)args->user_mask_ptr);
	if (error == 0)
		td->td_retval[0] = sizeof(cpuset_t);

	return (error);
}

/*
 *  Set affinity of a process.
 */
int
linux_sched_setaffinity(struct thread *td,
    struct linux_sched_setaffinity_args *args)
{
	struct thread *tdt;

	if (args->len < sizeof(cpuset_t))
		return (EINVAL);

	tdt = linux_tdfind(td, args->pid, -1);
	if (tdt == NULL)
		return (ESRCH);

	PROC_UNLOCK(tdt->td_proc);

	return (kern_cpuset_setaffinity(td, CPU_LEVEL_WHICH, CPU_WHICH_TID,
	    tdt->td_tid, sizeof(cpuset_t), (cpuset_t *) args->user_mask_ptr));
}

struct linux_rlimit64 {
	uint64_t	rlim_cur;
	uint64_t	rlim_max;
};

int
linux_prlimit64(struct thread *td, struct linux_prlimit64_args *args)
{
	struct rlimit rlim, nrlim;
	struct linux_rlimit64 lrlim;
	struct proc *p;
	u_int which;
	int flags;
	int error;

	if (args->new == NULL && args->old != NULL) {
		if (linux_get_dummy_limit(args->resource, &rlim)) {
			lrlim.rlim_cur = rlim.rlim_cur;
			lrlim.rlim_max = rlim.rlim_max;
			return (copyout(&lrlim, args->old, sizeof(lrlim)));
		}
	}

	if (args->resource >= LINUX_RLIM_NLIMITS)
		return (EINVAL);

	which = linux_to_bsd_resource[args->resource];
	if (which == -1)
		return (EINVAL);

	if (args->new != NULL) {
		/*
		 * Note. Unlike FreeBSD where rlim is signed 64-bit Linux
		 * rlim is unsigned 64-bit. FreeBSD treats negative limits
		 * as INFINITY so we do not need a conversion even.
		 */
		error = copyin(args->new, &nrlim, sizeof(nrlim));
		if (error != 0)
			return (error);
	}

	flags = PGET_HOLD | PGET_NOTWEXIT;
	if (args->new != NULL)
		flags |= PGET_CANDEBUG;
	else
		flags |= PGET_CANSEE;
	if (args->pid == 0) {
		p = td->td_proc;
		PHOLD(p);
	} else {
		error = pget(args->pid, flags, &p);
		if (error != 0)
			return (error);
	}
	if (args->old != NULL) {
		PROC_LOCK(p);
		lim_rlimit_proc(p, which, &rlim);
		PROC_UNLOCK(p);
		if (rlim.rlim_cur == RLIM_INFINITY)
			lrlim.rlim_cur = LINUX_RLIM_INFINITY;
		else
			lrlim.rlim_cur = rlim.rlim_cur;
		if (rlim.rlim_max == RLIM_INFINITY)
			lrlim.rlim_max = LINUX_RLIM_INFINITY;
		else
			lrlim.rlim_max = rlim.rlim_max;
		error = copyout(&lrlim, args->old, sizeof(lrlim));
		if (error != 0)
			goto out;
	}

	if (args->new != NULL)
		error = kern_proc_setrlimit(td, p, which, &nrlim);

 out:
	PRELE(p);
	return (error);
}

int
linux_pselect6(struct thread *td, struct linux_pselect6_args *args)
{
	struct timeval utv, tv0, tv1, *tvp;
	struct l_pselect6arg lpse6;
	struct l_timespec lts;
	struct timespec uts;
	l_sigset_t l_ss;
	sigset_t *ssp;
	sigset_t ss;
	int error;

	ssp = NULL;
	if (args->sig != NULL) {
		error = copyin(args->sig, &lpse6, sizeof(lpse6));
		if (error != 0)
			return (error);
		if (lpse6.ss_len != sizeof(l_ss))
			return (EINVAL);
		if (lpse6.ss != 0) {
			error = copyin(PTRIN(lpse6.ss), &l_ss,
			    sizeof(l_ss));
			if (error != 0)
				return (error);
			linux_to_bsd_sigset(&l_ss, &ss);
			ssp = &ss;
		}
	}

	/*
	 * Currently glibc changes nanosecond number to microsecond.
	 * This mean losing precision but for now it is hardly seen.
	 */
	if (args->tsp != NULL) {
		error = copyin(args->tsp, &lts, sizeof(lts));
		if (error != 0)
			return (error);
		error = linux_to_native_timespec(&uts, &lts);
		if (error != 0)
			return (error);

		TIMESPEC_TO_TIMEVAL(&utv, &uts);
		if (itimerfix(&utv))
			return (EINVAL);

		microtime(&tv0);
		tvp = &utv;
	} else
		tvp = NULL;

	error = kern_pselect(td, args->nfds, args->readfds, args->writefds,
	    args->exceptfds, tvp, ssp, LINUX_NFDBITS);

	if (error == 0 && args->tsp != NULL) {
		if (td->td_retval[0] != 0) {
			/*
			 * Compute how much time was left of the timeout,
			 * by subtracting the current time and the time
			 * before we started the call, and subtracting
			 * that result from the user-supplied value.
			 */

			microtime(&tv1);
			timevalsub(&tv1, &tv0);
			timevalsub(&utv, &tv1);
			if (utv.tv_sec < 0)
				timevalclear(&utv);
		} else
			timevalclear(&utv);

		TIMEVAL_TO_TIMESPEC(&utv, &uts);

		error = native_to_linux_timespec(&lts, &uts);
		if (error == 0)
			error = copyout(&lts, args->tsp, sizeof(lts));
	}

	return (error);
}

int
linux_ppoll(struct thread *td, struct linux_ppoll_args *args)
{
	struct timespec ts0, ts1;
	struct l_timespec lts;
	struct timespec uts, *tsp;
	l_sigset_t l_ss;
	sigset_t *ssp;
	sigset_t ss;
	int error;

	if (args->sset != NULL) {
		if (args->ssize != sizeof(l_ss))
			return (EINVAL);
		error = copyin(args->sset, &l_ss, sizeof(l_ss));
		if (error)
			return (error);
		linux_to_bsd_sigset(&l_ss, &ss);
		ssp = &ss;
	} else
		ssp = NULL;
	if (args->tsp != NULL) {
		error = copyin(args->tsp, &lts, sizeof(lts));
		if (error)
			return (error);
		error = linux_to_native_timespec(&uts, &lts);
		if (error != 0)
			return (error);

		nanotime(&ts0);
		tsp = &uts;
	} else
		tsp = NULL;

	error = kern_poll(td, args->fds, args->nfds, tsp, ssp);

	if (error == 0 && args->tsp != NULL) {
		if (td->td_retval[0]) {
			nanotime(&ts1);
			timespecsub(&ts1, &ts0, &ts1);
			timespecsub(&uts, &ts1, &uts);
			if (uts.tv_sec < 0)
				timespecclear(&uts);
		} else
			timespecclear(&uts);

		error = native_to_linux_timespec(&lts, &uts);
		if (error == 0)
			error = copyout(&lts, args->tsp, sizeof(lts));
	}

	return (error);
}

int
linux_sched_rr_get_interval(struct thread *td,
    struct linux_sched_rr_get_interval_args *uap)
{
	struct timespec ts;
	struct l_timespec lts;
	struct thread *tdt;
	int error;

	/*
	 * According to man in case the invalid pid specified
	 * EINVAL should be returned.
	 */
	if (uap->pid < 0)
		return (EINVAL);

	tdt = linux_tdfind(td, uap->pid, -1);
	if (tdt == NULL)
		return (ESRCH);

	error = kern_sched_rr_get_interval_td(td, tdt, &ts);
	PROC_UNLOCK(tdt->td_proc);
	if (error != 0)
		return (error);
	error = native_to_linux_timespec(&lts, &ts);
	if (error != 0)
		return (error);
	return (copyout(&lts, uap->interval, sizeof(lts)));
}

/*
 * In case when the Linux thread is the initial thread in
 * the thread group thread id is equal to the process id.
 * Glibc depends on this magic (assert in pthread_getattr_np.c).
 */
struct thread *
linux_tdfind(struct thread *td, lwpid_t tid, pid_t pid)
{
	struct linux_emuldata *em;
	struct thread *tdt;
	struct proc *p;

	tdt = NULL;
	if (tid == 0 || tid == td->td_tid) {
		tdt = td;
		PROC_LOCK(tdt->td_proc);
	} else if (tid > PID_MAX)
		tdt = tdfind(tid, pid);
	else {
		/*
		 * Initial thread where the tid equal to the pid.
		 */
		p = pfind(tid);
		if (p != NULL) {
			if (SV_PROC_ABI(p) != SV_ABI_LINUX) {
				/*
				 * p is not a Linuxulator process.
				 */
				PROC_UNLOCK(p);
				return (NULL);
			}
			FOREACH_THREAD_IN_PROC(p, tdt) {
				em = em_find(tdt);
				if (tid == em->em_tid)
					return (tdt);
			}
			PROC_UNLOCK(p);
		}
		return (NULL);
	}

	return (tdt);
}

void
linux_to_bsd_waitopts(int options, int *bsdopts)
{

	if (options & LINUX_WNOHANG)
		*bsdopts |= WNOHANG;
	if (options & LINUX_WUNTRACED)
		*bsdopts |= WUNTRACED;
	if (options & LINUX_WEXITED)
		*bsdopts |= WEXITED;
	if (options & LINUX_WCONTINUED)
		*bsdopts |= WCONTINUED;
	if (options & LINUX_WNOWAIT)
		*bsdopts |= WNOWAIT;

	if (options & __WCLONE)
		*bsdopts |= WLINUXCLONE;
}

int
linux_getrandom(struct thread *td, struct linux_getrandom_args *args)
{
	struct uio uio;
	struct iovec iov;
	int error;

	if (args->flags & ~(LINUX_GRND_NONBLOCK|LINUX_GRND_RANDOM))
		return (EINVAL);
	if (args->count > INT_MAX)
		args->count = INT_MAX;

	iov.iov_base = args->buf;
	iov.iov_len = args->count;

	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_resid = iov.iov_len;
	uio.uio_segflg = UIO_USERSPACE;
	uio.uio_rw = UIO_READ;
	uio.uio_td = td;

	error = read_random_uio(&uio, args->flags & LINUX_GRND_NONBLOCK);
	if (error == 0)
		td->td_retval[0] = args->count - uio.uio_resid;
	return (error);
}

int
linux_mincore(struct thread *td, struct linux_mincore_args *args)
{

	/* Needs to be page-aligned */
	if (args->start & PAGE_MASK)
		return (EINVAL);
	return (kern_mincore(td, args->start, args->len, args->vec));
}

#define	SYSLOG_TAG	"<6>"

int
linux_syslog(struct thread *td, struct linux_syslog_args *args)
{
	char buf[128], *src, *dst;
	u_int seq;
	int buflen, error;

	if (args->type != LINUX_SYSLOG_ACTION_READ_ALL) {
		linux_msg(td, "syslog unsupported type 0x%x", args->type);
		return (EINVAL);
	}

	if (args->len < 6) {
		td->td_retval[0] = 0;
		return (0);
	}

	error = priv_check(td, PRIV_MSGBUF);
	if (error)
		return (error);

	mtx_lock(&msgbuf_lock);
	msgbuf_peekbytes(msgbufp, NULL, 0, &seq);
	mtx_unlock(&msgbuf_lock);

	dst = args->buf;
	error = copyout(&SYSLOG_TAG, dst, sizeof(SYSLOG_TAG));
	/* The -1 is to skip the trailing '\0'. */
	dst += sizeof(SYSLOG_TAG) - 1;

	while (error == 0) {
		mtx_lock(&msgbuf_lock);
		buflen = msgbuf_peekbytes(msgbufp, buf, sizeof(buf), &seq);
		mtx_unlock(&msgbuf_lock);

		if (buflen == 0)
			break;

		for (src = buf; src < buf + buflen && error == 0; src++) {
			if (*src == '\0')
				continue;

			if (dst >= args->buf + args->len)
				goto out;

			error = copyout(src, dst, 1);
			dst++;

			if (*src == '\n' && *(src + 1) != '<' &&
			    dst + sizeof(SYSLOG_TAG) < args->buf + args->len) {
				error = copyout(&SYSLOG_TAG,
				    dst, sizeof(SYSLOG_TAG));
				dst += sizeof(SYSLOG_TAG) - 1;
			}
		}
	}
out:
	td->td_retval[0] = dst - args->buf;
	return (error);
}

int
linux_getcpu(struct thread *td, struct linux_getcpu_args *args)
{
	int cpu, error, node;

	cpu = td->td_oncpu; /* Make sure it doesn't change during copyout(9) */
	error = 0;
	node = cpuid_to_pcpu[cpu]->pc_domain;

	if (args->cpu != NULL)
		error = copyout(&cpu, args->cpu, sizeof(l_int));
	if (args->node != NULL)
		error = copyout(&node, args->node, sizeof(l_int));
	return (error);
}
