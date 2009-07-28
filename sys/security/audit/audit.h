/*-
 * Copyright (c) 1999-2005 Apple Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * This header includes function prototypes and type definitions that are
 * necessary for the kernel as a whole to interact with the audit subsystem.
 */

#ifndef _SECURITY_AUDIT_KERNEL_H_
#define	_SECURITY_AUDIT_KERNEL_H_

#ifndef _KERNEL
#error "no user-serviceable parts inside"
#endif

#include <bsm/audit.h>

#include <sys/file.h>
#include <sys/sysctl.h>

/*
 * Audit subsystem condition flags.  The audit_enabled flag is set and
 * removed automatically as a result of configuring log files, and can be
 * observed but should not be directly manipulated.  The audit suspension
 * flag permits audit to be temporarily disabled without reconfiguring the
 * audit target.
 */
extern int	audit_enabled;
extern int	audit_suspended;

/*
 * Define the masks for the audited arguments.
 *
 * XXXRW: These need to remain in audit.h for now because our vnode and name
 * lookup audit calls rely on passing in flags to indicate which name or
 * vnode is being logged.  These should move to audit_private.h when that is
 * fixed.
 */
#define	ARG_EUID		0x0000000000000001ULL
#define	ARG_RUID		0x0000000000000002ULL
#define	ARG_SUID		0x0000000000000004ULL
#define	ARG_EGID		0x0000000000000008ULL
#define	ARG_RGID		0x0000000000000010ULL
#define	ARG_SGID		0x0000000000000020ULL
#define	ARG_PID			0x0000000000000040ULL
#define	ARG_UID			0x0000000000000080ULL
#define	ARG_AUID		0x0000000000000100ULL
#define	ARG_GID			0x0000000000000200ULL
#define	ARG_FD			0x0000000000000400ULL
#define	ARG_POSIX_IPC_PERM	0x0000000000000800ULL
#define	ARG_FFLAGS		0x0000000000001000ULL
#define	ARG_MODE		0x0000000000002000ULL
#define	ARG_DEV			0x0000000000004000ULL
#define	ARG_ADDR		0x0000000000008000ULL
#define	ARG_LEN			0x0000000000010000ULL
#define	ARG_MASK		0x0000000000020000ULL
#define	ARG_SIGNUM		0x0000000000040000ULL
#define	ARG_LOGIN		0x0000000000080000ULL
#define	ARG_SADDRINET		0x0000000000100000ULL
#define	ARG_SADDRINET6		0x0000000000200000ULL
#define	ARG_SADDRUNIX		0x0000000000400000ULL
#define	ARG_TERMID_ADDR		0x0000000000400000ULL
#define	ARG_UNUSED2		0x0000000001000000ULL
#define	ARG_UPATH1		0x0000000002000000ULL
#define	ARG_UPATH2		0x0000000004000000ULL
#define	ARG_TEXT		0x0000000008000000ULL
#define	ARG_VNODE1		0x0000000010000000ULL
#define	ARG_VNODE2		0x0000000020000000ULL
#define	ARG_SVIPC_CMD		0x0000000040000000ULL
#define	ARG_SVIPC_PERM		0x0000000080000000ULL
#define	ARG_SVIPC_ID		0x0000000100000000ULL
#define	ARG_SVIPC_ADDR		0x0000000200000000ULL
#define	ARG_GROUPSET		0x0000000400000000ULL
#define	ARG_CMD			0x0000000800000000ULL
#define	ARG_SOCKINFO		0x0000001000000000ULL
#define	ARG_ASID		0x0000002000000000ULL
#define	ARG_TERMID		0x0000004000000000ULL
#define	ARG_AUDITON		0x0000008000000000ULL
#define	ARG_VALUE		0x0000010000000000ULL
#define	ARG_AMASK		0x0000020000000000ULL
#define	ARG_CTLNAME		0x0000040000000000ULL
#define	ARG_PROCESS		0x0000080000000000ULL
#define	ARG_MACHPORT1		0x0000100000000000ULL
#define	ARG_MACHPORT2		0x0000200000000000ULL
#define	ARG_EXIT		0x0000400000000000ULL
#define	ARG_IOVECSTR		0x0000800000000000ULL
#define	ARG_ARGV		0x0001000000000000ULL
#define	ARG_ENVV		0x0002000000000000ULL
#define	ARG_ATFD1		0x0004000000000000ULL
#define	ARG_ATFD2		0x0008000000000000ULL
#define	ARG_NONE		0x0000000000000000ULL
#define	ARG_ALL			0xFFFFFFFFFFFFFFFFULL

void	 audit_syscall_enter(unsigned short code, struct thread *td);
void	 audit_syscall_exit(int error, struct thread *td);

/*
 * The remaining kernel functions are conditionally compiled in as they are
 * wrapped by a macro, and the macro should be the only place in the source
 * tree where these functions are referenced.
 */
#ifdef AUDIT
struct ipc_perm;
struct sockaddr;
union auditon_udata;
void	 audit_arg_addr(void * addr);
void	 audit_arg_exit(int status, int retval);
void	 audit_arg_len(int len);
void	 audit_arg_atfd1(int atfd);
void	 audit_arg_atfd2(int atfd);
void	 audit_arg_fd(int fd);
void	 audit_arg_fflags(int fflags);
void	 audit_arg_gid(gid_t gid);
void	 audit_arg_uid(uid_t uid);
void	 audit_arg_egid(gid_t egid);
void	 audit_arg_euid(uid_t euid);
void	 audit_arg_rgid(gid_t rgid);
void	 audit_arg_ruid(uid_t ruid);
void	 audit_arg_sgid(gid_t sgid);
void	 audit_arg_suid(uid_t suid);
void	 audit_arg_groupset(gid_t *gidset, u_int gidset_size);
void	 audit_arg_login(char *login);
void	 audit_arg_ctlname(int *name, int namelen);
void	 audit_arg_mask(int mask);
void	 audit_arg_mode(mode_t mode);
void	 audit_arg_dev(int dev);
void	 audit_arg_value(long value);
void	 audit_arg_owner(uid_t uid, gid_t gid);
void	 audit_arg_pid(pid_t pid);
void	 audit_arg_process(struct proc *p);
void	 audit_arg_signum(u_int signum);
void	 audit_arg_socket(int sodomain, int sotype, int soprotocol);
void	 audit_arg_sockaddr(struct thread *td, struct sockaddr *sa);
void	 audit_arg_auid(uid_t auid);
void	 audit_arg_auditinfo(struct auditinfo *au_info);
void	 audit_arg_auditinfo_addr(struct auditinfo_addr *au_info);
void	 audit_arg_upath(struct thread *td, char *upath, u_int64_t flags);
void	 audit_arg_vnode1(struct vnode *vp);
void	 audit_arg_vnode2(struct vnode *vp);
void	 audit_arg_text(char *text);
void	 audit_arg_cmd(int cmd);
void	 audit_arg_svipc_cmd(int cmd);
void	 audit_arg_svipc_perm(struct ipc_perm *perm);
void	 audit_arg_svipc_id(int id);
void	 audit_arg_svipc_addr(void *addr);
void	 audit_arg_posix_ipc_perm(uid_t uid, gid_t gid, mode_t mode);
void	 audit_arg_auditon(union auditon_udata *udata);
void	 audit_arg_file(struct proc *p, struct file *fp);
void	 audit_arg_argv(char *argv, int argc, int length);
void	 audit_arg_envv(char *envv, int envc, int length);
void	 audit_sysclose(struct thread *td, int fd);
void	 audit_cred_copy(struct ucred *src, struct ucred *dest);
void	 audit_cred_destroy(struct ucred *cred);
void	 audit_cred_init(struct ucred *cred);
void	 audit_cred_kproc0(struct ucred *cred);
void	 audit_cred_proc1(struct ucred *cred);
void	 audit_proc_coredump(struct thread *td, char *path, int errcode);
void	 audit_thread_alloc(struct thread *td);
void	 audit_thread_free(struct thread *td);

/*
 * Define macros to wrap the audit_arg_* calls by checking the global
 * audit_enabled flag before performing the actual call.
 */
#define	AUDITING_TD(td)		((td)->td_pflags & TDP_AUDITREC)

#define	AUDIT_ARG_ADDR(addr) do {					\
	if (AUDITING_TD(curthread))					\
		audit_arg_addr((addr));					\
} while (0)

#define	AUDIT_ARG_ARGV(argv, argc, length) do {				\
	if (AUDITING_TD(curthread))					\
		audit_arg_argv((argv), (argc), (length));		\
} while (0)

#define	AUDIT_ARG_ATFD1(atfd) do {					\
	if (AUDITING_TD(curthread))					\
		audit_arg_atfd1((atfd));				\
} while (0)

#define	AUDIT_ARG_ATFD2(atfd) do {					\
	if (AUDITING_TD(curthread))					\
		audit_arg_atfd2((atfd));				\
} while (0)

#define	AUDIT_ARG_AUDITON(udata) do {					\
	if (AUDITING_TD(curthread))					\
		audit_arg_auditon((udata));				\
} while (0)

#define	AUDIT_ARG_CMD(cmd) do {						\
	if (AUDITING_TD(curthread))					\
		audit_arg_cmd((cmd));					\
} while (0)

#define	AUDIT_ARG_DEV(dev) do {						\
	if (AUDITING_TD(curthread))					\
		audit_arg_dev((dev));					\
} while (0)

#define	AUDIT_ARG_EGID(egid) do {					\
	if (AUDITING_TD(curthread))					\
		audit_arg_egid((egid));					\
} while (0)

#define	AUDIT_ARG_ENVV(envv, envc, length) do {				\
	if (AUDITING_TD(curthread))					\
		audit_arg_envv((envv), (envc), (length));		\
} while (0)

#define	AUDIT_ARG_EXIT(status, retval) do {				\
	if (AUDITING_TD(curthread))					\
		audit_arg_exit((status), (retval));			\
} while (0)

#define	AUDIT_ARG_EUID(euid) do {					\
	if (AUDITING_TD(curthread))					\
		audit_arg_euid((euid));					\
} while (0)

#define	AUDIT_ARG_FD(fd) do {						\
	if (AUDITING_TD(curthread))					\
		audit_arg_fd((fd));					\
} while (0)

#define	AUDIT_ARG_FILE(p, fp) do {					\
	if (AUDITING_TD(curthread))					\
		audit_arg_file((p), (fp));				\
} while (0)

#define	AUDIT_ARG_FFLAGS(fflags) do {					\
	if (AUDITING_TD(curthread))					\
		audit_arg_fflags((fflags));				\
} while (0)

#define	AUDIT_ARG_GID(gid) do {						\
	if (AUDITING_TD(curthread))					\
		audit_arg_gid((gid));					\
} while (0)

#define	AUDIT_ARG_GROUPSET(gidset, gidset_size) do {			\
	if (AUDITING_TD(curthread))					\
		audit_arg_groupset((gidset), (gidset_size));		\
} while (0)

#define	AUDIT_ARG_MODE(mode) do {					\
	if (AUDITING_TD(curthread))					\
		audit_arg_mode((mode));					\
} while (0)

#define	AUDIT_ARG_OWNER(uid, gid) do {					\
	if (AUDITING_TD(curthread))					\
		audit_arg_owner((uid), (gid));				\
} while (0)

#define	AUDIT_ARG_PID(pid) do {						\
	if (AUDITING_TD(curthread))					\
		audit_arg_pid((pid));					\
} while (0)

#define	AUDIT_ARG_PROCESS(p) do {					\
	if (AUDITING_TD(curthread))					\
		audit_arg_process((p));					\
} while (0)

#define	AUDIT_ARG_RGID(rgid) do {					\
	if (AUDITING_TD(curthread))					\
		audit_arg_gid((rgid));					\
} while (0)

#define	AUDIT_ARG_RUID(ruid) do {					\
	if (AUDITING_TD(curthread))					\
		audit_arg_ruid((ruid));					\
} while (0)

#define	AUDIT_ARG_SIGNUM(signum) do {					\
	if (AUDITING_TD(curthread))					\
		audit_arg_signum((signum));				\
} while (0)

#define	AUDIT_ARG_SGID(sgid) do {					\
	if (AUDITING_TD(curthread))					\
		audit_arg_sgid((sgid));					\
} while (0)

#define	AUDIT_ARG_SOCKET(sodomain, sotype, soprotocol) do {		\
	if (AUDITING_TD(curthread))					\
		audit_arg_socket((sodomain), (sotype), (soprotocol));	\
} while (0)

#define	AUDIT_ARG_SUID(suid) do {					\
	if (AUDITING_TD(curthread))					\
		audit_arg_suid((suid));					\
} while (0)

#define	AUDIT_ARG_TEXT(text) do {					\
	if (AUDITING_TD(curthread))					\
		audit_arg_text((text));					\
} while (0)

#define	AUDIT_ARG_UID(uid) do {						\
	if (AUDITING_TD(curthread))					\
		audit_arg_uid((uid));					\
} while (0)

#define	AUDIT_ARG_UPATH(td, upath, flags) do {				\
	if (AUDITING_TD(curthread))					\
		audit_arg_upath((td), (upath), (flags));		\
} while (0)

#define	AUDIT_ARG_VALUE(value) do {					\
	if (AUDITING_TD(curthread))					\
		audit_arg_value((value));				\
} while (0)

#define	AUDIT_ARG_VNODE1(vp) do {					\
	if (AUDITING_TD(curthread))					\
		audit_arg_vnode1((vp));					\
} while (0)

#define	AUDIT_ARG_VNODE2(vp) do {					\
	if (AUDITING_TD(curthread))					\
		audit_arg_vnode2((vp));					\
} while (0)

#define	AUDIT_SYSCALL_ENTER(code, td)	do {				\
	if (audit_enabled) {						\
		audit_syscall_enter(code, td);				\
	}								\
} while (0)

/*
 * Wrap the audit_syscall_exit() function so that it is called only when
 * we have a audit record on the thread.  Audit records can persist after
 * auditing is disabled, so we don't just check audit_enabled here.
 */
#define	AUDIT_SYSCALL_EXIT(error, td)	do {				\
	if (td->td_pflags & TDP_AUDITREC)				\
		audit_syscall_exit(error, td);				\
} while (0)

/*
 * A Macro to wrap the audit_sysclose() function.
 */
#define	AUDIT_SYSCLOSE(td, fd)	do {					\
	if (td->td_pflags & TDP_AUDITREC)				\
		audit_sysclose(td, fd);					\
} while (0)

#else /* !AUDIT */

#define	AUDIT_ARG_ADDR(addr)
#define	AUDIT_ARG_ARGV(argv, argc, length)
#define	AUDIT_ARG_ATFD1(atfd)
#define	AUDIT_ARG_ATFD2(atfd)
#define	AUDIT_ARG_AUDITON(udata)
#define	AUDIT_ARG_CMD(cmd)
#define	AUDIT_ARG_DEV(dev)
#define	AUDIT_ARG_EGID(egid)
#define	AUDIT_ARG_ENVV(envv, envc, length)
#define	AUDIT_ARG_EXIT(status, retval)
#define	AUDIT_ARG_EUID(euid)
#define	AUDIT_ARG_FD(fd)
#define	AUDIT_ARG_FILE(p, fp)
#define	AUDIT_ARG_FFLAGS(fflags)
#define	AUDIT_ARG_GID(gid)
#define	AUDIT_ARG_GROUPSET(gidset, gidset_size)
#define	AUDIT_ARG_MODE(mode)
#define	AUDIT_ARG_OWNER(uid, gid)
#define	AUDIT_ARG_PID(pid)
#define	AUDIT_ARG_PROCESS(p)
#define	AUDIT_ARG_RGID(rgid)
#define	AUDIT_ARG_RUID(ruid)
#define	AUDIT_ARG_SIGNUM(signum)
#define	AUDIT_ARG_SGID(sgid)
#define	AUDIT_ARG_SOCKET(sodomain, sotype, soprotocol)
#define	AUDIT_ARG_SUID(suid)
#define	AUDIT_ARG_TEXT(text)
#define	AUDIT_ARG_UID(uid)
#define	AUDIT_ARG_UPATH(td, upath, flags)
#define	AUDIT_ARG_VALUE(value)
#define	AUDIT_ARG_VNODE1(vp)
#define	AUDIT_ARG_VNODE2(vp)

#define	AUDIT_SYSCALL_ENTER(code, td)
#define	AUDIT_SYSCALL_EXIT(error, td)

#define	AUDIT_SYSCLOSE(p, fd)

#endif /* AUDIT */

#endif /* !_SECURITY_AUDIT_KERNEL_H_ */
