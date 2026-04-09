/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 UOS Project Contributors
 * Security enhancements adapted from OpenBSD's security model.
 *
 * UOS OpenBSD-Inspired Security Framework
 * =========================================
 *
 * OpenBSD is the most security-focused OS in the BSD family. This header
 * bridges OpenBSD concepts into UOS (FreeBSD-based), covering:
 *
 * 1. PLEDGE (OpenBSD) -> CAPSICUM (FreeBSD)
 *    pledge(2) restricts a process to a subset of syscalls.
 *    FreeBSD's capsicum(4) provides capability mode - equivalent.
 *
 * 2. UNVEIL (OpenBSD)
 *    unveil(2) restricts filesystem visibility per-process.
 *    Implemented here as a MAC policy + VFS hook.
 *
 * 3. W^X (Write XOR Execute)
 *    OpenBSD enforces W^X globally. UOS enforces via:
 *    - Qualcomm SCM XPU for Snapdragon hardware
 *    - ARM64 PAN (Privileged Access Never) always on
 *    - BTI (Branch Target Identification) for ROP mitigation
 *    - PAC (Pointer Authentication) for CFI
 *
 * 4. ASLR
 *    OpenBSD has the strongest ASLR. UOS adds:
 *    - Full kernel ASLR (KASLR via std.arm64)
 *    - User ASLR with 64-bit entropy (ARM64 gives full 48-bit VA)
 *    - Stack gap randomization
 *
 * 5. Kernel Hardening
 *    OpenBSD's malloc randomization, guard pages, page-level isolation.
 *    UOS equivalent: MALLOC_DEBUG, INVARIANTS in debug, guard pages.
 *
 * 6. Hardware-backed Cryptography
 *    Qualcomm QCE (Crypto Engine) / Inline Crypto Engine (ICE)
 *    for storage encryption (UFS inline encryption).
 *
 * 7. Secure Boot Chain
 *    Qualcomm QSEECOM + SCM for verified boot, attestation.
 */

#ifndef _UOS_SECURITY_H_
#define _UOS_SECURITY_H_

/* ================================================================
 * 1. UNVEIL - Filesystem visibility restriction
 *    Implemented as a MAC policy (see uos_mac_unveil.c)
 * ================================================================ */

#ifdef _KERNEL

#include <sys/types.h>
#include <sys/mac.h>
#include <sys/vnode.h>
#include <sys/proc.h>

/* Unveil permission bits (matching OpenBSD semantics) */
#define UOS_UNVEIL_READ		0x01	/* 'r' - open for reading */
#define UOS_UNVEIL_WRITE	0x02	/* 'w' - open for writing */
#define UOS_UNVEIL_EXEC		0x04	/* 'x' - execute */
#define UOS_UNVEIL_CREATE	0x08	/* 'c' - create files */
#define UOS_UNVEIL_INHERIT	0x10	/* path is inherited by child procs */

/* Max unveil entries per process (OpenBSD uses 128) */
#define UOS_UNVEIL_MAX		128

struct uos_unveil_entry {
	char		 path[MAXPATHLEN];
	uint32_t	 perms;
	struct vnode	*vp;	/* cached vnode for fast matching */
};

struct uos_unveil_state {
	struct uos_unveil_entry	 entries[UOS_UNVEIL_MAX];
	int			 count;
	bool			 frozen;  /* no more unveil() after first syscall past it */
};

/* Kernel API */
int	uos_unveil_add(struct proc *p, const char *path, uint32_t perms);
int	uos_unveil_check(struct proc *p, struct vnode *vp, accmode_t accmode);
void	uos_unveil_exec(struct proc *p);   /* Called on execve - resets unveil */
void	uos_unveil_fork(struct proc *parent, struct proc *child);

/* ================================================================
 * 2. W^X and Memory Protection enforcement
 *    Uses ARM64 hardware: BTI, PAC, PAN
 * ================================================================ */

/* ARM64 SCTLR_EL1 bits for UOS security hardening */
#define SCTLR_EL1_BT0		(1UL << 35)  /* BTI for EL0 (user space) */
#define SCTLR_EL1_BT1		(1UL << 36)  /* BTI for EL1 (kernel) */
#define SCTLR_EL1_ITFSB	(1UL << 37)  /* MTE tag fault synchronization */
#define SCTLR_EL1_ATA0		(1UL << 42)  /* MTE tag access EL0 */

/* UOS security policy flags (stored in proc->p_flag2 extension) */
#define UOS_SEC_WX_STRICT	0x0001	/* Deny any W+X mappings */
#define UOS_SEC_UNVEIL_ACTIVE	0x0002	/* unveil list is active */
#define UOS_SEC_CAPSICUM	0x0004	/* in capsicum capability mode */
#define UOS_SEC_PLEDGE_ACTIVE	0x0008	/* pledge-equivalent restrictions */
#define UOS_SEC_NO_PTRACE	0x0010	/* deny ptrace (hardened app) */
#define UOS_SEC_NO_PROC_DEBUG	0x0020	/* deny /proc/*/mem writes */

/*
 * uos_wx_check - Called from vm_mmap_vnode() to enforce W^X.
 * Returns EACCES if the mapping would violate W^X policy.
 */
int	uos_wx_check(struct proc *p, vm_prot_t prot, vm_prot_t maxprot);

/* ================================================================
 * 3. Random subsystem hardening (OpenBSD arc4random style)
 *    FreeBSD uses arc4random already - we add QCPR hardware backing
 * ================================================================ */

/* Request entropy from Qualcomm QRNG (True RNG) */
int	qcom_rng_read(void *buf, size_t len);

/* ================================================================
 * 4. Sensitive data scrubbing (OpenBSD explicit_bzero analog)
 *    FreeBSD has explicit_bzero() - we add scrub on kfree
 * ================================================================ */

/*
 * UOS_SCRUB_FREE - Scrub memory before returning to allocator.
 * Equivalent to OpenBSD's pool_init(..., PR_ZERO) for sensitive objects.
 */
#define UOS_SCRUB_FREE(ptr, len)	explicit_bzero((ptr), (len))

/* ================================================================
 * 5. Stack protection macros (OpenBSD SSP always-on model)
 *    FreeBSD compiles with -fstack-protector-strong by default.
 *    UOS forces it for all kernel code.
 * ================================================================ */

/*
 * UOS_STACK_SENTINEL - Compile-time reminder to ensure SSP is active.
 * Force-include in sensitive subsystems.
 */
#ifndef __SSP_FORTIFY_LEVEL
#warning "UOS Security: stack protector not active - enable -fstack-protector-strong"
#endif

/* ================================================================
 * 6. ARM Pointer Authentication (PAC) for CFI
 * ================================================================ */

/* Enable PAC for UOS kernel compilation.
 * Add to Makefile: CFLAGS += -mbranch-protection=pac-ret+leaf+bti
 */

/* UOS security initialization - called during boot */
void	uos_security_init(void);
void	uos_security_cpu_init(void);   /* per-CPU: enable BTI, PAC, PAN */
bool	uos_security_wx_enforced(void); /* returns true after full init */

#endif /* _KERNEL */

/* ================================================================
 * Userspace-visible security constants (for UOS libuos_sec.so)
 * ================================================================ */

/* Equivalent to OpenBSD's pledge promises */
#define UOS_PROMISE_STDIO	"stdio"
#define UOS_PROMISE_RPATH	"rpath"
#define UOS_PROMISE_WPATH	"wpath"
#define UOS_PROMISE_CPATH	"cpath"
#define UOS_PROMISE_DPATH	"dpath"
#define UOS_PROMISE_INET	"inet"
#define UOS_PROMISE_UNIX	"unix"
#define UOS_PROMISE_DNS		"dns"
#define UOS_PROMISE_EXEC	"exec"
#define UOS_PROMISE_PROC	"proc"
#define UOS_PROMISE_THREAD	"thread"
#define UOS_PROMISE_ID		"id"
#define UOS_PROMISE_AUDIO	"audio"
#define UOS_PROMISE_VIDEO	"video"
#define UOS_PROMISE_CAMERA	"camera"
#define UOS_PROMISE_SENSORS	"sensors"

/* Syscall number for uos_pledge() (maps to FreeBSD capsicum enter) */
#define SYS_uos_pledge		559
#define SYS_uos_unveil		560

#endif /* _UOS_SECURITY_H_ */
