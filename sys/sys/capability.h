/*-
 * Copyright (c) 2000 Robert N. M. Watson
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

#define	__CAP_MASK_LEN	2

typedef int	cap_flag_t;
typedef int	cap_flag_value_t;
typedef u_int	cap_value_t;

struct cap {
	u_int	c_effective[__CAP_MASK_LEN];
	u_int	c_permitted[__CAP_MASK_LEN];
	u_int	c_inheritable[__CAP_MASK_LEN];
};
typedef struct cap	*cap_t;

#define	CAP_TYPE_MASK		0xff
#define	CAP_MIN_TYPE		POSIX1E_CAPABILITY
#define	POSIX1E_CAPABILITY	0x00
#define	SYSTEM_CAPABILITY	0x01
#define	CAP_MAX_TYPE		SYSTEM_CAPABILITY

#define	SET_CAPABILITY(mask, cap) do { \
	(mask)[(cap) & CAP_TYPE_MASK] |= (cap) & ~CAP_TYPE_MASK; \
	} while (0)

#define	UNSET_CAPABILITY(mask, cap) do { \
	(mask)[(cap) & CAP_TYPE_MASK] &= ~(cap) & ~CAP_TYPE_MASK; \
	} while (0)

#define	IS_CAP_SET(mask, cap) \
	((mask)[(cap) & CAP_TYPE_MASK] & (cap) & ~CAP_TYPE_MASK)

/*
 * Is (tcap) a logical subset of (scap)?
 */
#define	CAP_SUBSET(scap,tcap) \
	((((scap).c_permitted[0] | (tcap).c_permitted[0]) \
	 == (scap).c_permitted[0]) && \
	 (((tcap.c_permitted[0] | (tcap).c_effective[0]) \
	 == (tcap).c_permitted[0]) && \
	 (((scap).c_permitted[1] | (tcap).c_permitted[1]) \
	 == (scap).c_permitted[1])  && \
	 (((tcap).c_permitted[1] | (tcap).c_effective[1]) \
	 == (tcap).c_permitted[1]))

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
#define	CAP_CHOWN		(0x00000100 | POSIX1E_CAPABILITY)
#define	CAP_DAC_EXECUTE		(0x00000200 | POSIX1E_CAPABILITY)
#define	CAP_DAC_WRITE		(0x00000400 | POSIX1E_CAPABILITY)
#define	CAP_DAC_READ_SEARCH	(0x00000800 | POSIX1E_CAPABILITY)
#define	CAP_FOWNER		(0x00001000 | POSIX1E_CAPABILITY)
#define	CAP_FSETID		(0x00002000 | POSIX1E_CAPABILITY)
#define	CAP_KILL		(0x00004000 | POSIX1E_CAPABILITY)
#define	CAP_LINK_DIR		(0x00008000 | POSIX1E_CAPABILITY)
#define	CAP_SETFCAP		(0x00010000 | POSIX1E_CAPABILITY)
#define	CAP_SETGID		(0x00020000 | POSIX1E_CAPABILITY)
#define	CAP_SETUID		(0x00040000 | POSIX1E_CAPABILITY)
#define	CAP_MAC_DOWNGRADE	(0x00080000 | POSIX1E_CAPABILITY)
#define	CAP_MAC_READ		(0x00100000 | POSIX1E_CAPABILITY)
#define	CAP_MAC_RELABEL_SUBJ	(0x00200000 | POSIX1E_CAPABILITY)
#define	CAP_MAC_UPGRADE		(0x00400000 | POSIX1E_CAPABILITY)
#define	CAP_MAC_WRITE		(0x00800000 | POSIX1E_CAPABILITY)
#define	CAP_INF_NOFLOAT_OBJ	(0x01000000 | POSIX1E_CAPABILITY)
#define	CAP_INF_NOFLOAT_SUBJ	(0x02000000 | POSIX1E_CAPABILITY)
#define	CAP_INF_RELABEL_OBJ	(0x04000000 | POSIX1E_CAPABILITY)
#define	CAP_INF_RELABEL_SUBJ	(0x08000000 | POSIX1E_CAPABILITY)
#define	CAP_AUDIT_CONTROL	(0x10000000 | POSIX1E_CAPABILITY)
#define	CAP_AUDIT_WRITE		(0x20000000 | POSIX1E_CAPABILITY)

/*
 * The following capability, borrowed from Linux, is unsafe
 * #define	CAP_SETPCAP		(0x00000100 | SYSTEM_CAPABILITY)
 */
/*
 * The following capability, borrowed from Linux, is not appropriate
 * in the BSD file environment
 * #define	CAP_LINUX_IMMUTABLE	(0x00000200 | SYSTEM_CAPABILITY)
 */
#define	CAP_BSD_SETFFLAG	(0x00000200 | SYSTEM_CAPABILITY)
#define	CAP_NET_BIND_SERVICE	(0x00000400 | SYSTEM_CAPABILITY)
#define	CAP_NET_BROADCAST	(0x00000800 | SYSTEM_CAPABILITY)
#define	CAP_NET_ADMIN		(0x00001000 | SYSTEM_CAPABILITY)
#define	CAP_NET_RAW		(0x00002000 | SYSTEM_CAPABILITY)
#define	CAP_IPC_LOCK		(0x00004000 | SYSTEM_CAPABILITY)
#define	CAP_IPC_OWNER		(0x00008000 | SYSTEM_CAPABILITY)
/*
 * The following capabilities, borrowed from Linux, are unsafe in a
 * secure environment.
 *
 * #define	CAP_SYS_MODULE		(0x00010000 | SYSTEM_CAPABILITY)
 * #define	CAP_SYS_RAWIO		(0x00020000 | SYSTEM_CAPABILITY)
 * #define	CAP_SYS_CHROOT		(0x00040000 | SYSTEM_CAPABILITY)
 * #define	CAP_SYS_PTRACE		(0x00080000 | SYSTEM_CAPABILITY)
 */
#define	CAP_SYS_PACCT		(0x00100000 | SYSTEM_CAPABILITY)
#define	CAP_SYS_ADMIN		(0x00200000 | SYSTEM_CAPABILITY)
#define	CAP_SYS_BOOT		(0x00400000 | SYSTEM_CAPABILITY)
#define	CAP_SYS_NICE		(0x00800000 | SYSTEM_CAPABILITY)
#define	CAP_SYS_RESOURCE	(0x01000000 | SYSTEM_CAPABILITY)
#define	CAP_SYS_TIME		(0x02000000 | SYSTEM_CAPABILITY)
#define	CAP_SYS_TTY_CONFIG	(0x04000000 | SYSTEM_CAPABILITY)

#ifdef _KERNEL

struct proc;
struct ucred;
int	cap_change_on_inherit(struct cap *);
int	cap_check(struct proc *, cap_value_t);
int	cap_check_xxx(struct ucred *, struct proc *, cap_value_t, int);
void	cap_inherit(struct cap *);
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

#endif /* !_KERNEL */

#endif /* !_SYS_CAPABILITY_H */
