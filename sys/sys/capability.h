/*-
 * Copyright (c) 2000-2001 Robert N. M. Watson
 * All rights reserved.
 *
 * Copyright (c) 1999 Ilmar S. Habibulin
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
 *
 * $FreeBSD$
 */
/*
 * Developed by the TrustedBSD Project.
 * Support for POSIX.1e process capabilities.
 */

#ifndef _SYS_CAPABILITY_H
#define	_SYS_CAPABILITY_H

#define	POSIX1E_CAPABILITY_EXTATTR_NAMESPACE	EXTATTR_NAMESPACE_SYSTEM
#define	POSIX1E_CAPABILITY_EXTATTR_NAME		"posix1e.cap"

typedef int	cap_flag_t;
typedef int	cap_flag_value_t;
typedef u_int64_t	cap_value_t;

struct cap {
	u_int64_t	c_effective;
	u_int64_t	c_permitted;
	u_int64_t	c_inheritable;
};
typedef struct cap	*cap_t;

#if defined(_KERNEL) | defined(_CAPABILITY_NEEDMACROS)
#define	SET_CAPABILITY(mask, cap) do { \
	(mask) |= cap; \
	} while (0)

#define	UNSET_CAPABILITY(mask, cap) do { \
	(mask) &= ~(cap); \
	} while (0)

#define	IS_CAP_SET(mask, cap) \
	((mask) & (cap))

/*
 * Is (tcap) a logical subset of (scap)?
 */
#define	CAP_SUBSET(scap,tcap) \
	((((scap).c_permitted | (tcap).c_permitted) == (scap).c_permitted) && \
	(((scap).c_effective | (tcap).c_effective) == (scap).c_effective) && \
	(((scap).c_inheritable | (tcap).c_inheritable) == (scap).c_inheritable))

/*
 * Put the union of the capability sets c1 and c2 into c2.
 */
#define CAP_UNITE(c1, c2) do { \
	(c1).c_permitted |= (c2).c_permitted; \
	(c1).c_effective |= (c2).c_effective; \
	(c1).c_inheritable |= (c2).c_inheritable; \
	} while (0)

/*
 * Test whether any bits in a cap set are set.
 * XXX: due to capability setting constraints, it should actually be
 * sufficient to check c_permitted.
 */

#define CAP_NONZERO(c) \
	((c).c_permitted != 0 || (c).c_effective != 0 || (c).c_inheritable != 0)

#endif

/*
 * Possible flags for a particular capability.
 */
#define	CAP_EFFECTIVE		0x01
#define	CAP_INHERITABLE		0x02
#define	CAP_PERMITTED		0x04

/*
 * Possible values for each capability flag.
 */
#define	CAP_CLEAR		0
#define	CAP_SET			1

/*
 * Possible capability values, both BSD/LINUX and POSIX.1e.
 */
#define	CAP_CHOWN		(0x0000000000000001)
#define	CAP_DAC_EXECUTE		(0x0000000000000002)
#define	CAP_DAC_WRITE		(0x0000000000000004)
#define	CAP_DAC_READ_SEARCH	(0x0000000000000008)
#define	CAP_FOWNER		(0x0000000000000010)
#define	CAP_FSETID		(0x0000000000000020)
#define	CAP_KILL		(0x0000000000000040)
#define	CAP_LINK_DIR		(0x0000000000000080)
#define	CAP_SETFCAP		(0x0000000000000100)
#define	CAP_SETGID		(0x0000000000000200)
#define	CAP_SETUID		(0x0000000000000400)
#define	CAP_MAC_DOWNGRADE	(0x0000000000000800)
#define	CAP_MAC_READ		(0x0000000000001000)
#define	CAP_MAC_RELABEL_SUBJ	(0x0000000000002000)
#define	CAP_MAC_UPGRADE		(0x0000000000004000)
#define	CAP_MAC_WRITE		(0x0000000000008000)
#define	CAP_INF_NOFLOAT_OBJ	(0x0000000000010000)
#define	CAP_INF_NOFLOAT_SUBJ	(0x0000000000020000)
#define	CAP_INF_RELABEL_OBJ	(0x0000000000040000)
#define	CAP_INF_RELABEL_SUBJ	(0x0000000000080000)
#define	CAP_AUDIT_CONTROL	(0x0000000000100000)
#define	CAP_AUDIT_WRITE		(0x0000000000200000)

/*
 * The following is no longer functional.
 * With our capability model, this serves no useful purpose. A process just
 * has all the capabilities it needs, and if it are to be temporarily given
 * up, they can be removed from the effective set.
 * We do not support modifying the capabilities of other processes, as Linux
 * (from which this one originated) does.
 */
#define	CAP_SETPCAP		(0x0000000000400000)
/* This is unallocated: */
#define	CAP_XXX_INVALID1	(0x0000000000800000)
#define	CAP_SYS_SETFFLAG	(0x0000000001000000)
/*
 * The CAP_LINUX_IMMUTABLE flag approximately maps into the
 * general file flag setting capability in BSD.  Therfore, for
 * compatibility, map the constants.
 */
#define	CAP_LINUX_IMMUTABLE	CAP_SYS_SETFFLAG
#define	CAP_NET_BIND_SERVICE	(0x0000000002000000)
#define	CAP_NET_BROADCAST	(0x0000000004000000)
#define	CAP_NET_ADMIN		(0x0000000008000000)
#define	CAP_NET_RAW		(0x0000000010000000)
#define	CAP_IPC_LOCK		(0x0000000020000000)
#define	CAP_IPC_OWNER		(0x0000000040000000)
/*
 * The following capabilities, borrowed from Linux, are unsafe in a
 * secure environment.
 */
#define	CAP_SYS_MODULE		(0x0000000080000000)
#define	CAP_SYS_RAWIO		(0x0000000100000000)
#define	CAP_SYS_CHROOT		(0x0000000200000000)
#define	CAP_SYS_PTRACE		(0x0000000400000000)
#define	CAP_SYS_PACCT		(0x0000000800000000)
#define	CAP_SYS_ADMIN		(0x0000001000000000)
/*
 * Back to the safe ones, again.
 */
#define	CAP_SYS_BOOT		(0x0000002000000000)
#define	CAP_SYS_NICE		(0x0000004000000000)
#define	CAP_SYS_RESOURCE	(0x0000008000000000)
#define	CAP_SYS_TIME		(0x0000010000000000)
#define	CAP_SYS_TTY_CONFIG	(0x0000020000000000)
#define	CAP_MKNOD		(0x0000040000000000)
#define	CAP_MAX_ID		CAP_MKNOD

#define	CAP_ALL_ON	(CAP_CHOWN | CAP_DAC_EXECUTE | CAP_DAC_WRITE | \
    CAP_DAC_READ_SEARCH | CAP_FOWNER | CAP_FSETID | CAP_KILL | CAP_LINK_DIR | \
    CAP_SETFCAP | CAP_SETGID | CAP_SETUID | CAP_MAC_DOWNGRADE | \
    CAP_MAC_READ | CAP_MAC_RELABEL_SUBJ | CAP_MAC_UPGRADE | \
    CAP_MAC_WRITE | CAP_INF_NOFLOAT_OBJ | CAP_INF_NOFLOAT_SUBJ | \
    CAP_INF_RELABEL_OBJ | CAP_INF_RELABEL_SUBJ | CAP_AUDIT_CONTROL | \
    CAP_AUDIT_WRITE | CAP_SYS_SETFFLAG | CAP_NET_BIND_SERVICE | \
    CAP_NET_BROADCAST | CAP_NET_ADMIN | CAP_NET_RAW | CAP_IPC_LOCK | \
    CAP_IPC_OWNER | CAP_SYS_MODULE | CAP_SYS_RAWIO | CAP_SYS_CHROOT | \
    CAP_SYS_PTRACE | CAP_SYS_PACCT | CAP_SYS_ADMIN | CAP_SYS_BOOT | \
    CAP_SYS_NICE | CAP_SYS_RESOURCE | CAP_SYS_TIME | CAP_SYS_TTY_CONFIG | \
    CAP_MKNOD)
#define	CAP_ALL_OFF	(0)

#ifdef _KERNEL

struct thread;
struct proc;
struct ucred;
struct vnode;
int	cap_check(struct ucred *, struct proc *, cap_value_t, int);
int	cap_check_td(struct ucred *, struct thread *, cap_value_t, int);
int	cap_change_on_inherit(struct cap *cap_p);
int	cap_inherit(struct vnode *vp, struct proc *p);
void	cap_init_proc0(struct cap *);
void	cap_init_proc1(struct cap *);

#else /* !_KERNEL */

#define	_POSIX_CAP

#ifdef	_BSD_SSIZE_T_
typedef	_BSD_SSIZE_T_	ssize_t;
#undef	_BSD_SSIZE_T_
#endif

int	__cap_get_proc(struct cap *);
int	__cap_set_proc(struct cap *);
int	__cap_get_fd(int, struct cap *);
int	__cap_get_file(const char *, struct cap *);
int	__cap_set_fd(int, struct cap *);
int	__cap_set_file(const char *, struct cap *);

int	cap_clear(cap_t);
ssize_t	cap_copy_ext(void *, cap_t, ssize_t);
cap_t	cap_copy_int(const void *);
cap_t	cap_dup(cap_t);
int	cap_free(void *);
cap_t	cap_from_text(const char *);
cap_t	cap_get_fd(int);
cap_t	cap_get_file(const char *);
int	cap_get_flag(cap_t, cap_value_t, cap_flag_t, cap_flag_value_t *);
cap_t	cap_get_proc(void);
cap_t	cap_init(void);
int	cap_set_fd(int, cap_t);
int	cap_set_file(const char *, cap_t);
int	cap_set_flag(cap_t, cap_flag_t, int, cap_value_t[] , cap_flag_value_t);
int	cap_set_proc(cap_t);
ssize_t	cap_size(cap_t);
char	*cap_to_text(cap_t, ssize_t *);

/*
 * Non-POSIX.1e functions
 *
 * Do the two cap_t's represent equal capability sets?
 */
int	cap_equal_np(cap_t, cap_t);

/* Interpret the text relative to an existing cap_t. */
cap_t	cap_from_text2_np(const char *, cap_t);

/* Is the first cap set a subset of the second? */
int	cap_subset_np(cap_t, cap_t);

/*
 * Like cap_to_text, takes an additional flags argument.  Flags are defined
 * below (CTT_*).
 */
char	*cap_to_text2_np(cap_t, ssize_t *, int);

#define	CTT_NOE	1	/* Do not output caps with only E flag set */
#define	CTT_NOI	2	/* Do not output caps with only I flag set */
#define	CTT_NOP	4	/* Do not output caps with only P flag set */
#define	CTT_ALL	8	/* Do output caps with no flags set */

#define	CTT_NOMSK	(CTT_NOE | CTT_NOI | CTT_NOP)

#define	CAP_MAX_BUF_LEN		1024	/* Maximum cap text buffer length */

#endif /* !_KERNEL */

#endif /* !_SYS_CAPABILITY_H */
