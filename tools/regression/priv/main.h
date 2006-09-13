/*-
 * Copyright (c) 2006 nCircle Network Security, Inc.
 * All rights reserved.
 *
 * This software was developed by Robert N. M. Watson for the TrustedBSD
 * Project under contract to nCircle Network Security, Inc.
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR, NCIRCLE NETWORK SECURITY,
 * INC., OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#define	UID_ROOT	0
#define	UID_OWNER	100
#define	UID_OTHER	200
#define	UID_THIRD	300

#define	GID_WHEEL	0
#define	GID_OWNER	100
#define	GID_OTHER	200

#define	KENV_VAR_NAME	"test"
#define	KENV_VAR_VALUE	"test"

/*
 * Library routines used by many tests.
 */
void	assert_root(void);
void	setup_file(char *fpathp, uid_t uid, gid_t gid, mode_t mode);
void	set_creds(uid_t uid, gid_t gid);
void	set_euid(uid_t uid);
void	restore_creds(void);

/*
 * Tests for specific privileges.
 */
void	priv_acct(void);
void	priv_adjtime(void);
void	priv_clock_settime(void);
void	priv_io(void);
void	priv_kenv_set(void);
void	priv_kenv_unset(void);
void	priv_proc_setlogin(void);
void	priv_proc_setrlimit(void);
void	priv_sched_rtprio(void);
void	priv_sched_setpriority(void);
void	priv_settimeofday(void);
void	priv_sysctl_write(void);
void	priv_vfs_admin(void);
void	priv_vfs_chown(void);
void	priv_vfs_chroot(void);
void	priv_vfs_clearsugid(void);
void	priv_vfs_extattr_system(void);
void	priv_vfs_fhopen(void);
void	priv_vfs_fhstat(void);
void	priv_vfs_fhstatfs(void);
void	priv_vfs_generation(void);
void	priv_vfs_getfh(void);
void	priv_vfs_read(void);
void	priv_vfs_setgid(void);
void	priv_vfs_stickyfile(void);
void	priv_vfs_write(void);
void	priv_vm_madv_protect(void);
void	priv_vm_mlock(void);
void	priv_vm_munlock(void);

/*
 * Tests for more complex access control logic involving more than one
 * privilege, or privilege combined with DAC.
 */
void	test_utimes(void);
