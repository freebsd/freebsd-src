/*-
 * Copyright (c) 1999, 2000, 2001, 2002 Robert N. M. Watson
 * Copyright (c) 2001, 2002, 2003 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed by Robert Watson for the TrustedBSD Project.
 *
 * This software was developed for the FreeBSD Project in part by Network
 * Associates Laboratories, the Security Research Division of Network
 * Associates, Inc. under DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"),
 * as part of the DARPA CHATS research program.
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
 * Generic mandatory access module that does nothing.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/acl.h>
#include <sys/conf.h>
#include <sys/extattr.h>
#include <sys/kernel.h>
#include <sys/mac.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/sysent.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>

#include <fs/devfs/devfs.h>

#include <net/bpfdesc.h>
#include <net/if.h>
#include <net/if_types.h>
#include <net/if_var.h>

#include <vm/vm.h>

#include <sys/mac_policy.h>

SYSCTL_DECL(_security_mac);

SYSCTL_NODE(_security_mac, OID_AUTO, test, CTLFLAG_RW, 0,
    "TrustedBSD mac_test policy controls");

static int	mac_test_enabled = 1;
SYSCTL_INT(_security_mac_test, OID_AUTO, enabled, CTLFLAG_RW,
    &mac_test_enabled, 0, "Enforce test policy");

#define	BPFMAGIC	0xfe1ad1b6
#define	DEVFSMAGIC	0x9ee79c32
#define	IFNETMAGIC	0xc218b120
#define	INPCBMAGIC	0x4440f7bb
#define	IPQMAGIC	0x206188ef
#define	MBUFMAGIC	0xbbefa5bb
#define	MOUNTMAGIC	0xc7c46e47
#define	SOCKETMAGIC	0x9199c6cd
#define	PIPEMAGIC	0xdc6c9919
#define	PROCMAGIC	0x3b4be98f
#define	CREDMAGIC	0x9a5a4987
#define	VNODEMAGIC	0x1a67a45c
#define	EXMAGIC		0x849ba1fd

#define	SLOT(x)	LABEL_TO_SLOT((x), test_slot).l_long

#define	ASSERT_BPF_LABEL(x)	KASSERT(SLOT(x) == BPFMAGIC ||		\
	SLOT(x) == 0, ("%s: Bad BPF label", __func__ ))
#define	ASSERT_DEVFS_LABEL(x)	KASSERT(SLOT(x) == DEVFSMAGIC ||	\
	SLOT(x) == 0, ("%s: Bad DEVFS label", __func__ ))
#define	ASSERT_IFNET_LABEL(x)	KASSERT(SLOT(x) == IFNETMAGIC ||	\
	SLOT(x) == 0, ("%s: Bad IFNET label", __func__ ))
#define	ASSERT_INPCB_LABEL(x)	KASSERT(SLOT(x) == INPCBMAGIC ||	\
	SLOT(x) == 0, ("%s: Bad INPCB label", __func__ ))
#define	ASSERT_IPQ_LABEL(x)	KASSERT(SLOT(x) == IPQMAGIC ||	\
	SLOT(x) == 0, ("%s: Bad IPQ label", __func__ ))
#define	ASSERT_MBUF_LABEL(x)	KASSERT(SLOT(x) == MBUFMAGIC ||		\
	SLOT(x) == 0, ("%s: Bad MBUF label", __func__ ))
#define	ASSERT_MOUNT_LABEL(x)	KASSERT(SLOT(x) == MOUNTMAGIC ||	\
	SLOT(x) == 0, ("%s: Bad MOUNT label", __func__ ))
#define	ASSERT_SOCKET_LABEL(x)	KASSERT(SLOT(x) == SOCKETMAGIC ||	\
	SLOT(x) == 0, ("%s: Bad SOCKET label", __func__ ))
#define	ASSERT_PIPE_LABEL(x)	KASSERT(SLOT(x) == PIPEMAGIC ||		\
	SLOT(x) == 0, ("%s: Bad PIPE label", __func__ ))
#define	ASSERT_PROC_LABEL(x)	KASSERT(SLOT(x) == PROCMAGIC ||		\
	SLOT(x) == 0, ("%s: Bad PROC label", __func__ ))
#define	ASSERT_CRED_LABEL(x)	KASSERT(SLOT(x) == CREDMAGIC ||		\
	SLOT(x) == 0, ("%s: Bad CRED label", __func__ ))
#define	ASSERT_VNODE_LABEL(x)	KASSERT(SLOT(x) == VNODEMAGIC ||	\
	SLOT(x) == 0, ("%s: Bad VNODE label", __func__ ))

static int	test_slot;
SYSCTL_INT(_security_mac_test, OID_AUTO, slot, CTLFLAG_RD,
    &test_slot, 0, "Slot allocated by framework");

static int	init_count_bpfdesc;
SYSCTL_INT(_security_mac_test, OID_AUTO, init_count_bpfdesc, CTLFLAG_RD,
    &init_count_bpfdesc, 0, "bpfdesc init calls");
static int	init_count_cred;
SYSCTL_INT(_security_mac_test, OID_AUTO, init_count_cred, CTLFLAG_RD,
    &init_count_cred, 0, "cred init calls");
static int	init_count_devfsdirent;
SYSCTL_INT(_security_mac_test, OID_AUTO, init_count_devfsdirent, CTLFLAG_RD,
    &init_count_devfsdirent, 0, "devfsdirent init calls");
static int	init_count_ifnet;
SYSCTL_INT(_security_mac_test, OID_AUTO, init_count_ifnet, CTLFLAG_RD,
    &init_count_ifnet, 0, "ifnet init calls");
static int	init_count_inpcb;
SYSCTL_INT(_security_mac_test, OID_AUTO, init_count_inpcb, CTLFLAG_RD,
    &init_count_inpcb, 0, "inpcb init calls");
static int	init_count_ipq;
SYSCTL_INT(_security_mac_test, OID_AUTO, init_count_ipq, CTLFLAG_RD,
    &init_count_ipq, 0, "ipq init calls");
static int	init_count_mbuf;
SYSCTL_INT(_security_mac_test, OID_AUTO, init_count_mbuf, CTLFLAG_RD,
    &init_count_mbuf, 0, "mbuf init calls");
static int	init_count_mount;
SYSCTL_INT(_security_mac_test, OID_AUTO, init_count_mount, CTLFLAG_RD,
    &init_count_mount, 0, "mount init calls");
static int	init_count_mount_fslabel;
SYSCTL_INT(_security_mac_test, OID_AUTO, init_count_mount_fslabel, CTLFLAG_RD,
    &init_count_mount_fslabel, 0, "mount_fslabel init calls");
static int	init_count_socket;
SYSCTL_INT(_security_mac_test, OID_AUTO, init_count_socket, CTLFLAG_RD,
    &init_count_socket, 0, "socket init calls");
static int	init_count_socket_peerlabel;
SYSCTL_INT(_security_mac_test, OID_AUTO, init_count_socket_peerlabel,
    CTLFLAG_RD, &init_count_socket_peerlabel, 0,
    "socket_peerlabel init calls");
static int	init_count_pipe;
SYSCTL_INT(_security_mac_test, OID_AUTO, init_count_pipe, CTLFLAG_RD,
    &init_count_pipe, 0, "pipe init calls");
static int	init_count_proc;
SYSCTL_INT(_security_mac_test, OID_AUTO, init_count_proc, CTLFLAG_RD,
    &init_count_proc, 0, "proc init calls");
static int	init_count_vnode;
SYSCTL_INT(_security_mac_test, OID_AUTO, init_count_vnode, CTLFLAG_RD,
    &init_count_vnode, 0, "vnode init calls");

static int	destroy_count_bpfdesc;
SYSCTL_INT(_security_mac_test, OID_AUTO, destroy_count_bpfdesc, CTLFLAG_RD,
    &destroy_count_bpfdesc, 0, "bpfdesc destroy calls");
static int	destroy_count_cred;
SYSCTL_INT(_security_mac_test, OID_AUTO, destroy_count_cred, CTLFLAG_RD,
    &destroy_count_cred, 0, "cred destroy calls");
static int	destroy_count_devfsdirent;
SYSCTL_INT(_security_mac_test, OID_AUTO, destroy_count_devfsdirent, CTLFLAG_RD,
    &destroy_count_devfsdirent, 0, "devfsdirent destroy calls");
static int	destroy_count_ifnet;
SYSCTL_INT(_security_mac_test, OID_AUTO, destroy_count_ifnet, CTLFLAG_RD,
    &destroy_count_ifnet, 0, "ifnet destroy calls");
static int	destroy_count_inpcb;
SYSCTL_INT(_security_mac_test, OID_AUTO, destroy_count_inpcb, CTLFLAG_RD,
    &destroy_count_inpcb, 0, "inpcb destroy calls");
static int	destroy_count_ipq;
SYSCTL_INT(_security_mac_test, OID_AUTO, destroy_count_ipq, CTLFLAG_RD,
    &destroy_count_ipq, 0, "ipq destroy calls");
static int      destroy_count_mbuf;
SYSCTL_INT(_security_mac_test, OID_AUTO, destroy_count_mbuf, CTLFLAG_RD,
    &destroy_count_mbuf, 0, "mbuf destroy calls");
static int      destroy_count_mount;
SYSCTL_INT(_security_mac_test, OID_AUTO, destroy_count_mount, CTLFLAG_RD,
    &destroy_count_mount, 0, "mount destroy calls");
static int      destroy_count_mount_fslabel;
SYSCTL_INT(_security_mac_test, OID_AUTO, destroy_count_mount_fslabel,
    CTLFLAG_RD, &destroy_count_mount_fslabel, 0,
    "mount_fslabel destroy calls");
static int      destroy_count_socket;
SYSCTL_INT(_security_mac_test, OID_AUTO, destroy_count_socket, CTLFLAG_RD,
    &destroy_count_socket, 0, "socket destroy calls");
static int      destroy_count_socket_peerlabel;
SYSCTL_INT(_security_mac_test, OID_AUTO, destroy_count_socket_peerlabel,
    CTLFLAG_RD, &destroy_count_socket_peerlabel, 0,
    "socket_peerlabel destroy calls");
static int      destroy_count_pipe;
SYSCTL_INT(_security_mac_test, OID_AUTO, destroy_count_pipe, CTLFLAG_RD,
    &destroy_count_pipe, 0, "pipe destroy calls");
static int      destroy_count_proc;
SYSCTL_INT(_security_mac_test, OID_AUTO, destroy_count_proc, CTLFLAG_RD,
    &destroy_count_proc, 0, "proc destroy calls");
static int      destroy_count_vnode;
SYSCTL_INT(_security_mac_test, OID_AUTO, destroy_count_vnode, CTLFLAG_RD,
    &destroy_count_vnode, 0, "vnode destroy calls");

static int externalize_count;
SYSCTL_INT(_security_mac_test, OID_AUTO, externalize_count, CTLFLAG_RD,
    &externalize_count, 0, "Subject/object externalize calls");
static int internalize_count;
SYSCTL_INT(_security_mac_test, OID_AUTO, internalize_count, CTLFLAG_RD,
    &internalize_count, 0, "Subject/object internalize calls");

/*
 * Policy module operations.
 */
static void
mac_test_destroy(struct mac_policy_conf *conf)
{

}

static void
mac_test_init(struct mac_policy_conf *conf)
{

}

static int
mac_test_syscall(struct thread *td, int call, void *arg)
{

	return (0);
}

/*
 * Label operations.
 */
static void
mac_test_init_bpfdesc_label(struct label *label)
{

	SLOT(label) = BPFMAGIC;
	atomic_add_int(&init_count_bpfdesc, 1);
}

static void
mac_test_init_cred_label(struct label *label)
{

	SLOT(label) = CREDMAGIC;
	atomic_add_int(&init_count_cred, 1);
}

static void
mac_test_init_devfsdirent_label(struct label *label)
{

	SLOT(label) = DEVFSMAGIC;
	atomic_add_int(&init_count_devfsdirent, 1);
}

static void
mac_test_init_ifnet_label(struct label *label)
{

	SLOT(label) = IFNETMAGIC;
	atomic_add_int(&init_count_ifnet, 1);
}

static int
mac_test_init_inpcb_label(struct label *label, int flag)
{

	if (flag & M_WAITOK)
		WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL,
		    "mac_test_init_inpcb_label() at %s:%d", __FILE__,
		    __LINE__);

	SLOT(label) = INPCBMAGIC;
	atomic_add_int(&init_count_inpcb, 1);
	return (0);
}

static int
mac_test_init_ipq_label(struct label *label, int flag)
{

	if (flag & M_WAITOK)
		WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL,
		    "mac_test_init_ipq_label() at %s:%d", __FILE__,
		    __LINE__);

	SLOT(label) = IPQMAGIC;
	atomic_add_int(&init_count_ipq, 1);
	return (0);
}

static int
mac_test_init_mbuf_label(struct label *label, int flag)
{

	if (flag & M_WAITOK)
		WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL,
		    "mac_test_init_mbuf_label() at %s:%d", __FILE__,
		    __LINE__);

	SLOT(label) = MBUFMAGIC;
	atomic_add_int(&init_count_mbuf, 1);
	return (0);
}

static void
mac_test_init_mount_label(struct label *label)
{

	SLOT(label) = MOUNTMAGIC;
	atomic_add_int(&init_count_mount, 1);
}

static void
mac_test_init_mount_fs_label(struct label *label)
{

	SLOT(label) = MOUNTMAGIC;
	atomic_add_int(&init_count_mount_fslabel, 1);
}

static int
mac_test_init_socket_label(struct label *label, int flag)
{

	if (flag & M_WAITOK)
		WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL,
		    "mac_test_init_socket_label() at %s:%d", __FILE__,
		    __LINE__);

	SLOT(label) = SOCKETMAGIC;
	atomic_add_int(&init_count_socket, 1);
	return (0);
}

static int
mac_test_init_socket_peer_label(struct label *label, int flag)
{

	if (flag & M_WAITOK)
		WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL,
		    "mac_test_init_socket_peer_label() at %s:%d", __FILE__,
		    __LINE__);

	SLOT(label) = SOCKETMAGIC;
	atomic_add_int(&init_count_socket_peerlabel, 1);
	return (0);
}

static void
mac_test_init_pipe_label(struct label *label)
{

	SLOT(label) = PIPEMAGIC;
	atomic_add_int(&init_count_pipe, 1);
}

static void
mac_test_init_proc_label(struct label *label)
{

	SLOT(label) = PROCMAGIC;
	atomic_add_int(&init_count_proc, 1);
}

static void
mac_test_init_vnode_label(struct label *label)
{

	SLOT(label) = VNODEMAGIC;
	atomic_add_int(&init_count_vnode, 1);
}

static void
mac_test_destroy_bpfdesc_label(struct label *label)
{

	if (SLOT(label) == BPFMAGIC || SLOT(label) == 0) {
		atomic_add_int(&destroy_count_bpfdesc, 1);
		SLOT(label) = EXMAGIC;
	} else if (SLOT(label) == EXMAGIC) {
		Debugger("mac_test_destroy_bpfdesc: dup destroy");
	} else {
		Debugger("mac_test_destroy_bpfdesc: corrupted label");
	}
}

static void
mac_test_destroy_cred_label(struct label *label)
{

	if (SLOT(label) == CREDMAGIC || SLOT(label) == 0) {
		atomic_add_int(&destroy_count_cred, 1);
		SLOT(label) = EXMAGIC;
	} else if (SLOT(label) == EXMAGIC) {
		Debugger("mac_test_destroy_cred: dup destroy");
	} else {
		Debugger("mac_test_destroy_cred: corrupted label");
	}
}

static void
mac_test_destroy_devfsdirent_label(struct label *label)
{

	if (SLOT(label) == DEVFSMAGIC || SLOT(label) == 0) {
		atomic_add_int(&destroy_count_devfsdirent, 1);
		SLOT(label) = EXMAGIC;
	} else if (SLOT(label) == EXMAGIC) {
		Debugger("mac_test_destroy_devfsdirent: dup destroy");
	} else {
		Debugger("mac_test_destroy_devfsdirent: corrupted label");
	}
}

static void
mac_test_destroy_ifnet_label(struct label *label)
{

	if (SLOT(label) == IFNETMAGIC || SLOT(label) == 0) {
		atomic_add_int(&destroy_count_ifnet, 1);
		SLOT(label) = EXMAGIC;
	} else if (SLOT(label) == EXMAGIC) {
		Debugger("mac_test_destroy_ifnet: dup destroy");
	} else {
		Debugger("mac_test_destroy_ifnet: corrupted label");
	}
}

static void
mac_test_destroy_inpcb_label(struct label *label)
{

	if (SLOT(label) == INPCBMAGIC || SLOT(label) == 0) {
		atomic_add_int(&destroy_count_inpcb, 1);
		SLOT(label) = EXMAGIC;
	} else if (SLOT(label) == EXMAGIC) {
		Debugger("mac_test_destroy_inpcb: dup destroy");
	} else {
		Debugger("mac_test_destroy_inpcb: corrupted label");
	}
}

static void
mac_test_destroy_ipq_label(struct label *label)
{

	if (SLOT(label) == IPQMAGIC || SLOT(label) == 0) {
		atomic_add_int(&destroy_count_ipq, 1);
		SLOT(label) = EXMAGIC;
	} else if (SLOT(label) == EXMAGIC) {
		Debugger("mac_test_destroy_ipq: dup destroy");
	} else {
		Debugger("mac_test_destroy_ipq: corrupted label");
	}
}

static void
mac_test_destroy_mbuf_label(struct label *label)
{

	/*
	 * If we're loaded dynamically, there may be mbufs in flight that
	 * didn't have label storage allocated for them.  Handle this
	 * gracefully.
	 */
	if (label == NULL)
		return;

	if (SLOT(label) == MBUFMAGIC || SLOT(label) == 0) {
		atomic_add_int(&destroy_count_mbuf, 1);
		SLOT(label) = EXMAGIC;
	} else if (SLOT(label) == EXMAGIC) {
		Debugger("mac_test_destroy_mbuf: dup destroy");
	} else {
		Debugger("mac_test_destroy_mbuf: corrupted label");
	}
}

static void
mac_test_destroy_mount_label(struct label *label)
{

	if ((SLOT(label) == MOUNTMAGIC || SLOT(label) == 0)) {
		atomic_add_int(&destroy_count_mount, 1);
		SLOT(label) = EXMAGIC;
	} else if (SLOT(label) == EXMAGIC) {
		Debugger("mac_test_destroy_mount: dup destroy");
	} else {
		Debugger("mac_test_destroy_mount: corrupted label");
	}
}

static void
mac_test_destroy_mount_fs_label(struct label *label)
{

	if ((SLOT(label) == MOUNTMAGIC || SLOT(label) == 0)) {
		atomic_add_int(&destroy_count_mount_fslabel, 1);
		SLOT(label) = EXMAGIC;
	} else if (SLOT(label) == EXMAGIC) {
		Debugger("mac_test_destroy_mount_fslabel: dup destroy");
	} else {
		Debugger("mac_test_destroy_mount_fslabel: corrupted label");
	}
}

static void
mac_test_destroy_socket_label(struct label *label)
{

	if ((SLOT(label) == SOCKETMAGIC || SLOT(label) == 0)) {
		atomic_add_int(&destroy_count_socket, 1);
		SLOT(label) = EXMAGIC;
	} else if (SLOT(label) == EXMAGIC) {
		Debugger("mac_test_destroy_socket: dup destroy");
	} else {
		Debugger("mac_test_destroy_socket: corrupted label");
	}
}

static void
mac_test_destroy_socket_peer_label(struct label *label)
{

	if ((SLOT(label) == SOCKETMAGIC || SLOT(label) == 0)) {
		atomic_add_int(&destroy_count_socket_peerlabel, 1);
		SLOT(label) = EXMAGIC;
	} else if (SLOT(label) == EXMAGIC) {
		Debugger("mac_test_destroy_socket_peerlabel: dup destroy");
	} else {
		Debugger("mac_test_destroy_socket_peerlabel: corrupted label");
	}
}

static void
mac_test_destroy_pipe_label(struct label *label)
{

	if ((SLOT(label) == PIPEMAGIC || SLOT(label) == 0)) {
		atomic_add_int(&destroy_count_pipe, 1);
		SLOT(label) = EXMAGIC;
	} else if (SLOT(label) == EXMAGIC) {
		Debugger("mac_test_destroy_pipe: dup destroy");
	} else {
		Debugger("mac_test_destroy_pipe: corrupted label");
	}
}

static void
mac_test_destroy_proc_label(struct label *label)
{

	if ((SLOT(label) == PROCMAGIC || SLOT(label) == 0)) {
		atomic_add_int(&destroy_count_proc, 1);
		SLOT(label) = EXMAGIC;
	} else if (SLOT(label) == EXMAGIC) {
		Debugger("mac_test_destroy_proc: dup destroy");
	} else {
		Debugger("mac_test_destroy_proc: corrupted label");
	}
}

static void
mac_test_destroy_vnode_label(struct label *label)
{

	if (SLOT(label) == VNODEMAGIC || SLOT(label) == 0) {
		atomic_add_int(&destroy_count_vnode, 1);
		SLOT(label) = EXMAGIC;
	} else if (SLOT(label) == EXMAGIC) {
		Debugger("mac_test_destroy_vnode: dup destroy");
	} else {
		Debugger("mac_test_destroy_vnode: corrupted label");
	}
}

static void
mac_test_copy_cred_label(struct label *src, struct label *dest)
{

	ASSERT_CRED_LABEL(src);
	ASSERT_CRED_LABEL(dest);
}

static void
mac_test_copy_mbuf_label(struct label *src, struct label *dest)
{

	ASSERT_MBUF_LABEL(src);
	ASSERT_MBUF_LABEL(dest);
}

static void
mac_test_copy_pipe_label(struct label *src, struct label *dest)
{

	ASSERT_PIPE_LABEL(src);
	ASSERT_PIPE_LABEL(dest);
}

static void
mac_test_copy_socket_label(struct label *src, struct label *dest)
{

	ASSERT_SOCKET_LABEL(src);
	ASSERT_SOCKET_LABEL(dest);
}

static void
mac_test_copy_vnode_label(struct label *src, struct label *dest)
{

	ASSERT_VNODE_LABEL(src);
	ASSERT_VNODE_LABEL(dest);
}

static int
mac_test_externalize_label(struct label *label, char *element_name,
    struct sbuf *sb, int *claimed)
{

	atomic_add_int(&externalize_count, 1);

	KASSERT(SLOT(label) != EXMAGIC,
	    ("mac_test_externalize_label: destroyed label"));

	return (0);
}

static int
mac_test_internalize_label(struct label *label, char *element_name,
    char *element_data, int *claimed)
{

	atomic_add_int(&internalize_count, 1);

	KASSERT(SLOT(label) != EXMAGIC,
	    ("mac_test_internalize_label: destroyed label"));

	return (0);
}

/*
 * Labeling event operations: file system objects, and things that look
 * a lot like file system objects.
 */
static void
mac_test_associate_vnode_devfs(struct mount *mp, struct label *fslabel,
    struct devfs_dirent *de, struct label *delabel, struct vnode *vp,
    struct label *vlabel)
{

	ASSERT_MOUNT_LABEL(fslabel);
	ASSERT_DEVFS_LABEL(delabel);
	ASSERT_VNODE_LABEL(vlabel);
}

static int
mac_test_associate_vnode_extattr(struct mount *mp, struct label *fslabel,
    struct vnode *vp, struct label *vlabel)
{

	ASSERT_MOUNT_LABEL(fslabel);
	ASSERT_VNODE_LABEL(vlabel);
	return (0);
}

static void
mac_test_associate_vnode_singlelabel(struct mount *mp,
    struct label *fslabel, struct vnode *vp, struct label *vlabel)
{

	ASSERT_MOUNT_LABEL(fslabel);
	ASSERT_VNODE_LABEL(vlabel);
}

static void
mac_test_create_devfs_device(struct mount *mp, dev_t dev,
    struct devfs_dirent *devfs_dirent, struct label *label)
{

	ASSERT_DEVFS_LABEL(label);
}

static void
mac_test_create_devfs_directory(struct mount *mp, char *dirname,
    int dirnamelen, struct devfs_dirent *devfs_dirent, struct label *label)
{

	ASSERT_DEVFS_LABEL(label);
}

static void
mac_test_create_devfs_symlink(struct ucred *cred, struct mount *mp,
    struct devfs_dirent *dd, struct label *ddlabel, struct devfs_dirent *de,
    struct label *delabel)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_DEVFS_LABEL(ddlabel);
	ASSERT_DEVFS_LABEL(delabel);
}

static int
mac_test_create_vnode_extattr(struct ucred *cred, struct mount *mp,
    struct label *fslabel, struct vnode *dvp, struct label *dlabel,
    struct vnode *vp, struct label *vlabel, struct componentname *cnp)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_MOUNT_LABEL(fslabel);
	ASSERT_VNODE_LABEL(dlabel);

	return (0);
}

static void
mac_test_create_mount(struct ucred *cred, struct mount *mp,
    struct label *mntlabel, struct label *fslabel)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_MOUNT_LABEL(mntlabel);
	ASSERT_MOUNT_LABEL(fslabel);
}

static void
mac_test_create_root_mount(struct ucred *cred, struct mount *mp,
    struct label *mntlabel, struct label *fslabel)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_MOUNT_LABEL(mntlabel);
	ASSERT_MOUNT_LABEL(fslabel);
}

static void
mac_test_relabel_vnode(struct ucred *cred, struct vnode *vp,
    struct label *vnodelabel, struct label *label)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_VNODE_LABEL(vnodelabel);
	ASSERT_VNODE_LABEL(label);
}

static int
mac_test_setlabel_vnode_extattr(struct ucred *cred, struct vnode *vp,
    struct label *vlabel, struct label *intlabel)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_VNODE_LABEL(vlabel);
	ASSERT_VNODE_LABEL(intlabel);
	return (0);
}

static void
mac_test_update_devfsdirent(struct mount *mp,
    struct devfs_dirent *devfs_dirent, struct label *direntlabel,
    struct vnode *vp, struct label *vnodelabel)
{

	ASSERT_DEVFS_LABEL(direntlabel);
	ASSERT_VNODE_LABEL(vnodelabel);
}

/*
 * Labeling event operations: IPC object.
 */
static void
mac_test_create_mbuf_from_socket(struct socket *so, struct label *socketlabel,
    struct mbuf *m, struct label *mbuflabel)
{

	ASSERT_SOCKET_LABEL(socketlabel);
	ASSERT_MBUF_LABEL(mbuflabel);
}

static void
mac_test_create_socket(struct ucred *cred, struct socket *socket,
   struct label *socketlabel)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_SOCKET_LABEL(socketlabel);
}

static void
mac_test_create_pipe(struct ucred *cred, struct pipe *pipe,
   struct label *pipelabel)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_PIPE_LABEL(pipelabel);
}

static void
mac_test_create_socket_from_socket(struct socket *oldsocket,
    struct label *oldsocketlabel, struct socket *newsocket,
    struct label *newsocketlabel)
{

	ASSERT_SOCKET_LABEL(oldsocketlabel);
	ASSERT_SOCKET_LABEL(newsocketlabel);
}

static void
mac_test_relabel_socket(struct ucred *cred, struct socket *socket,
    struct label *socketlabel, struct label *newlabel)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_SOCKET_LABEL(newlabel);
}

static void
mac_test_relabel_pipe(struct ucred *cred, struct pipe *pipe,
    struct label *pipelabel, struct label *newlabel)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_PIPE_LABEL(pipelabel);
	ASSERT_PIPE_LABEL(newlabel);
}

static void
mac_test_set_socket_peer_from_mbuf(struct mbuf *mbuf, struct label *mbuflabel,
    struct socket *socket, struct label *socketpeerlabel)
{

	ASSERT_MBUF_LABEL(mbuflabel);
	ASSERT_SOCKET_LABEL(socketpeerlabel);
}

/*
 * Labeling event operations: network objects.
 */
static void
mac_test_set_socket_peer_from_socket(struct socket *oldsocket,
    struct label *oldsocketlabel, struct socket *newsocket,
    struct label *newsocketpeerlabel)
{

	ASSERT_SOCKET_LABEL(oldsocketlabel);
	ASSERT_SOCKET_LABEL(newsocketpeerlabel);
}

static void
mac_test_create_bpfdesc(struct ucred *cred, struct bpf_d *bpf_d,
    struct label *bpflabel)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_BPF_LABEL(bpflabel);
}

static void
mac_test_create_datagram_from_ipq(struct ipq *ipq, struct label *ipqlabel,
    struct mbuf *datagram, struct label *datagramlabel)
{

	ASSERT_IPQ_LABEL(ipqlabel);
	ASSERT_MBUF_LABEL(datagramlabel);
}

static void
mac_test_create_fragment(struct mbuf *datagram, struct label *datagramlabel,
    struct mbuf *fragment, struct label *fragmentlabel)
{

	ASSERT_MBUF_LABEL(datagramlabel);
	ASSERT_MBUF_LABEL(fragmentlabel);
}

static void
mac_test_create_ifnet(struct ifnet *ifnet, struct label *ifnetlabel)
{

	ASSERT_IFNET_LABEL(ifnetlabel);
}

static void
mac_test_create_inpcb_from_socket(struct socket *so, struct label *solabel,
    struct inpcb *inp, struct label *inplabel)
{

	ASSERT_SOCKET_LABEL(solabel);
	ASSERT_INPCB_LABEL(inplabel);
}

static void
mac_test_create_ipq(struct mbuf *fragment, struct label *fragmentlabel,
    struct ipq *ipq, struct label *ipqlabel)
{

	ASSERT_MBUF_LABEL(fragmentlabel);
	ASSERT_IPQ_LABEL(ipqlabel);
}

static void
mac_test_create_mbuf_from_mbuf(struct mbuf *oldmbuf,
    struct label *oldmbuflabel, struct mbuf *newmbuf,
    struct label *newmbuflabel)
{

	ASSERT_MBUF_LABEL(oldmbuflabel);
	ASSERT_MBUF_LABEL(newmbuflabel);
}

static void
mac_test_create_mbuf_linklayer(struct ifnet *ifnet, struct label *ifnetlabel,
    struct mbuf *mbuf, struct label *mbuflabel)
{

	ASSERT_IFNET_LABEL(ifnetlabel);
	ASSERT_MBUF_LABEL(mbuflabel);
}

static void
mac_test_create_mbuf_from_bpfdesc(struct bpf_d *bpf_d, struct label *bpflabel,
    struct mbuf *mbuf, struct label *mbuflabel)
{

	ASSERT_BPF_LABEL(bpflabel);
	ASSERT_MBUF_LABEL(mbuflabel);
}

static void
mac_test_create_mbuf_from_ifnet(struct ifnet *ifnet, struct label *ifnetlabel,
    struct mbuf *m, struct label *mbuflabel)
{

	ASSERT_IFNET_LABEL(ifnetlabel);
	ASSERT_MBUF_LABEL(mbuflabel);
}

static void
mac_test_create_mbuf_multicast_encap(struct mbuf *oldmbuf,
    struct label *oldmbuflabel, struct ifnet *ifnet, struct label *ifnetlabel,
    struct mbuf *newmbuf, struct label *newmbuflabel)
{

	ASSERT_MBUF_LABEL(oldmbuflabel);
	ASSERT_IFNET_LABEL(ifnetlabel);
	ASSERT_MBUF_LABEL(newmbuflabel);
}

static void
mac_test_create_mbuf_netlayer(struct mbuf *oldmbuf,
    struct label *oldmbuflabel, struct mbuf *newmbuf,
    struct label *newmbuflabel)
{

	ASSERT_MBUF_LABEL(oldmbuflabel);
	ASSERT_MBUF_LABEL(newmbuflabel);
}

static int
mac_test_fragment_match(struct mbuf *fragment, struct label *fragmentlabel,
    struct ipq *ipq, struct label *ipqlabel)
{

	ASSERT_MBUF_LABEL(fragmentlabel);
	ASSERT_IPQ_LABEL(ipqlabel);

	return (1);
}

static void
mac_test_reflect_mbuf_icmp(struct mbuf *m, struct label *mlabel)
{

	ASSERT_MBUF_LABEL(mlabel);
}

static void
mac_test_reflect_mbuf_tcp(struct mbuf *m, struct label *mlabel)
{

	ASSERT_MBUF_LABEL(mlabel);
}

static void
mac_test_relabel_ifnet(struct ucred *cred, struct ifnet *ifnet,
    struct label *ifnetlabel, struct label *newlabel)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_IFNET_LABEL(ifnetlabel);
	ASSERT_IFNET_LABEL(newlabel);
}

static void
mac_test_update_ipq(struct mbuf *fragment, struct label *fragmentlabel,
    struct ipq *ipq, struct label *ipqlabel)
{

	ASSERT_MBUF_LABEL(fragmentlabel);
	ASSERT_IPQ_LABEL(ipqlabel);
}

static void
mac_test_inpcb_sosetlabel(struct socket *so, struct label *solabel,
    struct inpcb *inp, struct label *inplabel)
{

	ASSERT_SOCKET_LABEL(solabel);
	ASSERT_INPCB_LABEL(inplabel);
}

/*
 * Labeling event operations: processes.
 */
static void
mac_test_execve_transition(struct ucred *old, struct ucred *new,
    struct vnode *vp, struct label *filelabel,
    struct label *interpvnodelabel, struct image_params *imgp,
    struct label *execlabel)
{

	ASSERT_CRED_LABEL(old->cr_label);
	ASSERT_CRED_LABEL(new->cr_label);
	ASSERT_VNODE_LABEL(filelabel);
	ASSERT_VNODE_LABEL(interpvnodelabel);
	if (execlabel != NULL) {
		ASSERT_CRED_LABEL(execlabel);
	}
}

static int
mac_test_execve_will_transition(struct ucred *old, struct vnode *vp,
    struct label *filelabel, struct label *interpvnodelabel,
    struct image_params *imgp, struct label *execlabel)
{

	ASSERT_CRED_LABEL(old->cr_label);
	ASSERT_VNODE_LABEL(filelabel);
	if (interpvnodelabel != NULL) {
		ASSERT_VNODE_LABEL(interpvnodelabel);
	}
	if (execlabel != NULL) {
		ASSERT_CRED_LABEL(execlabel);
	}

	return (0);
}

static void
mac_test_create_proc0(struct ucred *cred)
{

	ASSERT_CRED_LABEL(cred->cr_label);
}

static void
mac_test_create_proc1(struct ucred *cred)
{

	ASSERT_CRED_LABEL(cred->cr_label);
}

static void
mac_test_relabel_cred(struct ucred *cred, struct label *newlabel)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_CRED_LABEL(newlabel);
}

static void
mac_test_thread_userret(struct thread *td)
{

	printf("mac_test_thread_userret(process = %d)\n",
	    curthread->td_proc->p_pid);
}

/*
 * Access control checks.
 */
static int
mac_test_check_bpfdesc_receive(struct bpf_d *bpf_d, struct label *bpflabel,
    struct ifnet *ifnet, struct label *ifnetlabel)
{

	ASSERT_BPF_LABEL(bpflabel);
	ASSERT_IFNET_LABEL(ifnetlabel);

	return (0);
}

static int
mac_test_check_cred_relabel(struct ucred *cred, struct label *newlabel)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_CRED_LABEL(newlabel);

	return (0);
}

static int
mac_test_check_cred_visible(struct ucred *u1, struct ucred *u2)
{

	ASSERT_CRED_LABEL(u1->cr_label);
	ASSERT_CRED_LABEL(u2->cr_label);

	return (0);
}

static int
mac_test_check_ifnet_relabel(struct ucred *cred, struct ifnet *ifnet,
    struct label *ifnetlabel, struct label *newlabel)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_IFNET_LABEL(ifnetlabel);
	ASSERT_IFNET_LABEL(newlabel);
	return (0);
}

static int
mac_test_check_ifnet_transmit(struct ifnet *ifnet, struct label *ifnetlabel,
    struct mbuf *m, struct label *mbuflabel)
{

	ASSERT_IFNET_LABEL(ifnetlabel);
	ASSERT_MBUF_LABEL(mbuflabel);

	return (0);
}

static int
mac_test_check_inpcb_deliver(struct inpcb *inp, struct label *inplabel,
    struct mbuf *m, struct label *mlabel)
{

	ASSERT_INPCB_LABEL(inplabel);
	ASSERT_MBUF_LABEL(mlabel);

	return (0);
}

static int
mac_test_check_kenv_dump(struct ucred *cred)
{

	ASSERT_CRED_LABEL(cred->cr_label);

	return (0);
}

static int
mac_test_check_kenv_get(struct ucred *cred, char *name)
{

	ASSERT_CRED_LABEL(cred->cr_label);

	return (0);
}

static int
mac_test_check_kenv_set(struct ucred *cred, char *name, char *value)
{

	ASSERT_CRED_LABEL(cred->cr_label);

	return (0);
}

static int
mac_test_check_kenv_unset(struct ucred *cred, char *name)
{

	ASSERT_CRED_LABEL(cred->cr_label);

	return (0);
}

static int
mac_test_check_kld_load(struct ucred *cred, struct vnode *vp,
    struct label *label)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_VNODE_LABEL(label);

	return (0);
}

static int
mac_test_check_kld_stat(struct ucred *cred)
{

	ASSERT_CRED_LABEL(cred->cr_label);

	return (0);
}

static int
mac_test_check_kld_unload(struct ucred *cred)
{

	ASSERT_CRED_LABEL(cred->cr_label);

	return (0);
}

static int
mac_test_check_mount_stat(struct ucred *cred, struct mount *mp,
    struct label *mntlabel)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_MOUNT_LABEL(mntlabel);

	return (0);
}

static int
mac_test_check_pipe_ioctl(struct ucred *cred, struct pipe *pipe,
    struct label *pipelabel, unsigned long cmd, void /* caddr_t */ *data)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_PIPE_LABEL(pipelabel);

	return (0);
}

static int
mac_test_check_pipe_poll(struct ucred *cred, struct pipe *pipe,
    struct label *pipelabel)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_PIPE_LABEL(pipelabel);

	return (0);
}

static int
mac_test_check_pipe_read(struct ucred *cred, struct pipe *pipe,
    struct label *pipelabel)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_PIPE_LABEL(pipelabel);

	return (0);
}

static int
mac_test_check_pipe_relabel(struct ucred *cred, struct pipe *pipe,
    struct label *pipelabel, struct label *newlabel)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_PIPE_LABEL(pipelabel);
	ASSERT_PIPE_LABEL(newlabel);

	return (0);
}

static int
mac_test_check_pipe_stat(struct ucred *cred, struct pipe *pipe,
    struct label *pipelabel)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_PIPE_LABEL(pipelabel);

	return (0);
}

static int
mac_test_check_pipe_write(struct ucred *cred, struct pipe *pipe,
    struct label *pipelabel)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_PIPE_LABEL(pipelabel);

	return (0);
}

static int
mac_test_check_proc_debug(struct ucred *cred, struct proc *proc)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_CRED_LABEL(proc->p_ucred->cr_label);

	return (0);
}

static int
mac_test_check_proc_sched(struct ucred *cred, struct proc *proc)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_CRED_LABEL(proc->p_ucred->cr_label);

	return (0);
}

static int
mac_test_check_proc_signal(struct ucred *cred, struct proc *proc, int signum)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_CRED_LABEL(proc->p_ucred->cr_label);

	return (0);
}

static int
mac_test_check_socket_bind(struct ucred *cred, struct socket *socket,
    struct label *socketlabel, struct sockaddr *sockaddr)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_SOCKET_LABEL(socketlabel);

	return (0);
}

static int
mac_test_check_socket_connect(struct ucred *cred, struct socket *socket,
    struct label *socketlabel, struct sockaddr *sockaddr)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_SOCKET_LABEL(socketlabel);

	return (0);
}

static int
mac_test_check_socket_deliver(struct socket *socket, struct label *socketlabel,
    struct mbuf *m, struct label *mbuflabel)
{

	ASSERT_SOCKET_LABEL(socketlabel);
	ASSERT_MBUF_LABEL(mbuflabel);

	return (0);
}

static int
mac_test_check_socket_listen(struct ucred *cred, struct socket *socket,
    struct label *socketlabel)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_SOCKET_LABEL(socketlabel);

	return (0);
}

static int
mac_test_check_socket_visible(struct ucred *cred, struct socket *socket,
    struct label *socketlabel)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_SOCKET_LABEL(socketlabel);

	return (0);
}

static int
mac_test_check_socket_relabel(struct ucred *cred, struct socket *socket,
    struct label *socketlabel, struct label *newlabel)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_SOCKET_LABEL(socketlabel);
	ASSERT_SOCKET_LABEL(newlabel);

	return (0);
}

static int
mac_test_check_sysarch_ioperm(struct ucred *cred)
{

	ASSERT_CRED_LABEL(cred->cr_label);

	return (0);
}

static int
mac_test_check_system_acct(struct ucred *cred, struct vnode *vp,
    struct label *label)
{

	ASSERT_CRED_LABEL(cred->cr_label);

	return (0);
}

static int
mac_test_check_system_reboot(struct ucred *cred, int how)
{

	ASSERT_CRED_LABEL(cred->cr_label);

	return (0);
}

static int
mac_test_check_system_settime(struct ucred *cred)
{

	ASSERT_CRED_LABEL(cred->cr_label);

	return (0);
}

static int
mac_test_check_system_swapon(struct ucred *cred, struct vnode *vp,
    struct label *label)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_VNODE_LABEL(label);

	return (0);
}

static int
mac_test_check_system_swapoff(struct ucred *cred, struct vnode *vp,
    struct label *label)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_VNODE_LABEL(label);

	return (0);
}

static int
mac_test_check_system_sysctl(struct ucred *cred, int *name, u_int namelen,
    void *old, size_t *oldlenp, int inkernel, void *new, size_t newlen)
{

	ASSERT_CRED_LABEL(cred->cr_label);

	return (0);
}

static int
mac_test_check_vnode_access(struct ucred *cred, struct vnode *vp,
    struct label *label, int acc_mode)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_VNODE_LABEL(label);

	return (0);
}

static int
mac_test_check_vnode_chdir(struct ucred *cred, struct vnode *dvp,
    struct label *dlabel)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_VNODE_LABEL(dlabel);

	return (0);
}

static int
mac_test_check_vnode_chroot(struct ucred *cred, struct vnode *dvp,
    struct label *dlabel)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_VNODE_LABEL(dlabel);

	return (0);
}

static int
mac_test_check_vnode_create(struct ucred *cred, struct vnode *dvp,
    struct label *dlabel, struct componentname *cnp, struct vattr *vap)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_VNODE_LABEL(dlabel);

	return (0);
}

static int
mac_test_check_vnode_delete(struct ucred *cred, struct vnode *dvp,
    struct label *dlabel, struct vnode *vp, struct label *label,
    struct componentname *cnp)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_VNODE_LABEL(dlabel);
	ASSERT_VNODE_LABEL(label);

	return (0);
}

static int
mac_test_check_vnode_deleteacl(struct ucred *cred, struct vnode *vp,
    struct label *label, acl_type_t type)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_VNODE_LABEL(label);

	return (0);
}

static int
mac_test_check_vnode_deleteextattr(struct ucred *cred, struct vnode *vp,
    struct label *label, int attrnamespace, const char *name)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_VNODE_LABEL(label);

	return (0);
}

static int
mac_test_check_vnode_exec(struct ucred *cred, struct vnode *vp,
    struct label *label, struct image_params *imgp,
    struct label *execlabel)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_VNODE_LABEL(label);
	if (execlabel != NULL) {
		ASSERT_CRED_LABEL(execlabel);
	}

	return (0);
}

static int
mac_test_check_vnode_getacl(struct ucred *cred, struct vnode *vp,
    struct label *label, acl_type_t type)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_VNODE_LABEL(label);

	return (0);
}

static int
mac_test_check_vnode_getextattr(struct ucred *cred, struct vnode *vp,
    struct label *label, int attrnamespace, const char *name, struct uio *uio)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_VNODE_LABEL(label);

	return (0);
}

static int
mac_test_check_vnode_link(struct ucred *cred, struct vnode *dvp,
    struct label *dlabel, struct vnode *vp, struct label *label,
    struct componentname *cnp)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_VNODE_LABEL(dlabel);
	ASSERT_VNODE_LABEL(label);

	return (0);
}

static int
mac_test_check_vnode_listextattr(struct ucred *cred, struct vnode *vp,
    struct label *label, int attrnamespace)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_VNODE_LABEL(label);

	return (0);
}

static int
mac_test_check_vnode_lookup(struct ucred *cred, struct vnode *dvp,
    struct label *dlabel, struct componentname *cnp)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_VNODE_LABEL(dlabel);

	return (0);
}

static int
mac_test_check_vnode_mmap(struct ucred *cred, struct vnode *vp,
    struct label *label, int prot)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_VNODE_LABEL(label);

	return (0);
}

static int
mac_test_check_vnode_mprotect(struct ucred *cred, struct vnode *vp,
    struct label *label, int prot)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_VNODE_LABEL(label);

	return (0);
}

static int
mac_test_check_vnode_open(struct ucred *cred, struct vnode *vp,
    struct label *filelabel, int acc_mode)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_VNODE_LABEL(filelabel);

	return (0);
}

static int
mac_test_check_vnode_poll(struct ucred *active_cred, struct ucred *file_cred,
    struct vnode *vp, struct label *label)
{

	ASSERT_CRED_LABEL(active_cred->cr_label);
	ASSERT_CRED_LABEL(file_cred->cr_label);
	ASSERT_VNODE_LABEL(label);

	return (0);
}

static int
mac_test_check_vnode_read(struct ucred *active_cred, struct ucred *file_cred,
    struct vnode *vp, struct label *label)
{

	ASSERT_CRED_LABEL(active_cred->cr_label);
	if (file_cred != NULL) {
		ASSERT_CRED_LABEL(file_cred->cr_label);
	}
	ASSERT_VNODE_LABEL(label);

	return (0);
}

static int
mac_test_check_vnode_readdir(struct ucred *cred, struct vnode *dvp,
    struct label *dlabel)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_VNODE_LABEL(dlabel);

	return (0);
}

static int
mac_test_check_vnode_readlink(struct ucred *cred, struct vnode *vp,
    struct label *vnodelabel)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_VNODE_LABEL(vnodelabel);

	return (0);
}

static int
mac_test_check_vnode_relabel(struct ucred *cred, struct vnode *vp,
    struct label *vnodelabel, struct label *newlabel)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_VNODE_LABEL(vnodelabel);
	ASSERT_VNODE_LABEL(newlabel);

	return (0);
}

static int
mac_test_check_vnode_rename_from(struct ucred *cred, struct vnode *dvp,
    struct label *dlabel, struct vnode *vp, struct label *label,
    struct componentname *cnp)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_VNODE_LABEL(dlabel);
	ASSERT_VNODE_LABEL(label);

	return (0);
}

static int
mac_test_check_vnode_rename_to(struct ucred *cred, struct vnode *dvp,
    struct label *dlabel, struct vnode *vp, struct label *label, int samedir,
    struct componentname *cnp)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_VNODE_LABEL(dlabel);

	if (vp != NULL) {
		ASSERT_VNODE_LABEL(label);
	}

	return (0);
}

static int
mac_test_check_vnode_revoke(struct ucred *cred, struct vnode *vp,
    struct label *label)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_VNODE_LABEL(label);

	return (0);
}

static int
mac_test_check_vnode_setacl(struct ucred *cred, struct vnode *vp,
    struct label *label, acl_type_t type, struct acl *acl)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_VNODE_LABEL(label);

	return (0);
}

static int
mac_test_check_vnode_setextattr(struct ucred *cred, struct vnode *vp,
    struct label *label, int attrnamespace, const char *name, struct uio *uio)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_VNODE_LABEL(label);

	return (0);
}

static int
mac_test_check_vnode_setflags(struct ucred *cred, struct vnode *vp,
    struct label *label, u_long flags)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_VNODE_LABEL(label);

	return (0);
}

static int
mac_test_check_vnode_setmode(struct ucred *cred, struct vnode *vp,
    struct label *label, mode_t mode)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_VNODE_LABEL(label);

	return (0);
}

static int
mac_test_check_vnode_setowner(struct ucred *cred, struct vnode *vp,
    struct label *label, uid_t uid, gid_t gid)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_VNODE_LABEL(label);

	return (0);
}

static int
mac_test_check_vnode_setutimes(struct ucred *cred, struct vnode *vp,
    struct label *label, struct timespec atime, struct timespec mtime)
{

	ASSERT_CRED_LABEL(cred->cr_label);
	ASSERT_VNODE_LABEL(label);

	return (0);
}

static int
mac_test_check_vnode_stat(struct ucred *active_cred, struct ucred *file_cred,
    struct vnode *vp, struct label *label)
{

	ASSERT_CRED_LABEL(active_cred->cr_label);
	if (file_cred != NULL) {
		ASSERT_CRED_LABEL(file_cred->cr_label);
	}
	ASSERT_VNODE_LABEL(label);

	return (0);
}

static int
mac_test_check_vnode_write(struct ucred *active_cred,
    struct ucred *file_cred, struct vnode *vp, struct label *label)
{

	ASSERT_CRED_LABEL(active_cred->cr_label);
	if (file_cred != NULL) {
		ASSERT_CRED_LABEL(file_cred->cr_label);
	}
	ASSERT_VNODE_LABEL(label);

	return (0);
}

static struct mac_policy_ops mac_test_ops =
{
	.mpo_destroy = mac_test_destroy,
	.mpo_init = mac_test_init,
	.mpo_syscall = mac_test_syscall,
	.mpo_init_bpfdesc_label = mac_test_init_bpfdesc_label,
	.mpo_init_cred_label = mac_test_init_cred_label,
	.mpo_init_devfsdirent_label = mac_test_init_devfsdirent_label,
	.mpo_init_ifnet_label = mac_test_init_ifnet_label,
	.mpo_init_inpcb_label = mac_test_init_inpcb_label,
	.mpo_init_ipq_label = mac_test_init_ipq_label,
	.mpo_init_mbuf_label = mac_test_init_mbuf_label,
	.mpo_init_mount_label = mac_test_init_mount_label,
	.mpo_init_mount_fs_label = mac_test_init_mount_fs_label,
	.mpo_init_pipe_label = mac_test_init_pipe_label,
	.mpo_init_proc_label = mac_test_init_proc_label,
	.mpo_init_socket_label = mac_test_init_socket_label,
	.mpo_init_socket_peer_label = mac_test_init_socket_peer_label,
	.mpo_init_vnode_label = mac_test_init_vnode_label,
	.mpo_destroy_bpfdesc_label = mac_test_destroy_bpfdesc_label,
	.mpo_destroy_cred_label = mac_test_destroy_cred_label,
	.mpo_destroy_devfsdirent_label = mac_test_destroy_devfsdirent_label,
	.mpo_destroy_ifnet_label = mac_test_destroy_ifnet_label,
	.mpo_destroy_inpcb_label = mac_test_destroy_inpcb_label,
	.mpo_destroy_ipq_label = mac_test_destroy_ipq_label,
	.mpo_destroy_mbuf_label = mac_test_destroy_mbuf_label,
	.mpo_destroy_mount_label = mac_test_destroy_mount_label,
	.mpo_destroy_mount_fs_label = mac_test_destroy_mount_fs_label,
	.mpo_destroy_pipe_label = mac_test_destroy_pipe_label,
	.mpo_destroy_proc_label = mac_test_destroy_proc_label,
	.mpo_destroy_socket_label = mac_test_destroy_socket_label,
	.mpo_destroy_socket_peer_label = mac_test_destroy_socket_peer_label,
	.mpo_destroy_vnode_label = mac_test_destroy_vnode_label,
	.mpo_copy_cred_label = mac_test_copy_cred_label,
	.mpo_copy_mbuf_label = mac_test_copy_mbuf_label,
	.mpo_copy_pipe_label = mac_test_copy_pipe_label,
	.mpo_copy_socket_label = mac_test_copy_socket_label,
	.mpo_copy_vnode_label = mac_test_copy_vnode_label,
	.mpo_externalize_cred_label = mac_test_externalize_label,
	.mpo_externalize_ifnet_label = mac_test_externalize_label,
	.mpo_externalize_pipe_label = mac_test_externalize_label,
	.mpo_externalize_socket_label = mac_test_externalize_label,
	.mpo_externalize_socket_peer_label = mac_test_externalize_label,
	.mpo_externalize_vnode_label = mac_test_externalize_label,
	.mpo_internalize_cred_label = mac_test_internalize_label,
	.mpo_internalize_ifnet_label = mac_test_internalize_label,
	.mpo_internalize_pipe_label = mac_test_internalize_label,
	.mpo_internalize_socket_label = mac_test_internalize_label,
	.mpo_internalize_vnode_label = mac_test_internalize_label,
	.mpo_associate_vnode_devfs = mac_test_associate_vnode_devfs,
	.mpo_associate_vnode_extattr = mac_test_associate_vnode_extattr,
	.mpo_associate_vnode_singlelabel = mac_test_associate_vnode_singlelabel,
	.mpo_create_devfs_device = mac_test_create_devfs_device,
	.mpo_create_devfs_directory = mac_test_create_devfs_directory,
	.mpo_create_devfs_symlink = mac_test_create_devfs_symlink,
	.mpo_create_vnode_extattr = mac_test_create_vnode_extattr,
	.mpo_create_mount = mac_test_create_mount,
	.mpo_create_root_mount = mac_test_create_root_mount,
	.mpo_relabel_vnode = mac_test_relabel_vnode,
	.mpo_setlabel_vnode_extattr = mac_test_setlabel_vnode_extattr,
	.mpo_update_devfsdirent = mac_test_update_devfsdirent,
	.mpo_create_mbuf_from_socket = mac_test_create_mbuf_from_socket,
	.mpo_create_pipe = mac_test_create_pipe,
	.mpo_create_socket = mac_test_create_socket,
	.mpo_create_socket_from_socket = mac_test_create_socket_from_socket,
	.mpo_relabel_pipe = mac_test_relabel_pipe,
	.mpo_relabel_socket = mac_test_relabel_socket,
	.mpo_set_socket_peer_from_mbuf = mac_test_set_socket_peer_from_mbuf,
	.mpo_set_socket_peer_from_socket = mac_test_set_socket_peer_from_socket,
	.mpo_create_bpfdesc = mac_test_create_bpfdesc,
	.mpo_create_ifnet = mac_test_create_ifnet,
	.mpo_create_inpcb_from_socket = mac_test_create_inpcb_from_socket,
	.mpo_create_datagram_from_ipq = mac_test_create_datagram_from_ipq,
	.mpo_create_fragment = mac_test_create_fragment,
	.mpo_create_ipq = mac_test_create_ipq,
	.mpo_create_mbuf_from_mbuf = mac_test_create_mbuf_from_mbuf,
	.mpo_create_mbuf_linklayer = mac_test_create_mbuf_linklayer,
	.mpo_create_mbuf_from_bpfdesc = mac_test_create_mbuf_from_bpfdesc,
	.mpo_create_mbuf_from_ifnet = mac_test_create_mbuf_from_ifnet,
	.mpo_create_mbuf_multicast_encap = mac_test_create_mbuf_multicast_encap,
	.mpo_create_mbuf_netlayer = mac_test_create_mbuf_netlayer,
	.mpo_fragment_match = mac_test_fragment_match,
	.mpo_reflect_mbuf_icmp = mac_test_reflect_mbuf_icmp,
	.mpo_reflect_mbuf_tcp = mac_test_reflect_mbuf_tcp,
	.mpo_relabel_ifnet = mac_test_relabel_ifnet,
	.mpo_update_ipq = mac_test_update_ipq,
	.mpo_inpcb_sosetlabel = mac_test_inpcb_sosetlabel,
	.mpo_execve_transition = mac_test_execve_transition,
	.mpo_execve_will_transition = mac_test_execve_will_transition,
	.mpo_create_proc0 = mac_test_create_proc0,
	.mpo_create_proc1 = mac_test_create_proc1,
	.mpo_relabel_cred = mac_test_relabel_cred,
	.mpo_thread_userret = mac_test_thread_userret,
	.mpo_check_bpfdesc_receive = mac_test_check_bpfdesc_receive,
	.mpo_check_cred_relabel = mac_test_check_cred_relabel,
	.mpo_check_cred_visible = mac_test_check_cred_visible,
	.mpo_check_ifnet_relabel = mac_test_check_ifnet_relabel,
	.mpo_check_ifnet_transmit = mac_test_check_ifnet_transmit,
	.mpo_check_inpcb_deliver = mac_test_check_inpcb_deliver,
	.mpo_check_kenv_dump = mac_test_check_kenv_dump,
	.mpo_check_kenv_get = mac_test_check_kenv_get,
	.mpo_check_kenv_set = mac_test_check_kenv_set,
	.mpo_check_kenv_unset = mac_test_check_kenv_unset,
	.mpo_check_kld_load = mac_test_check_kld_load,
	.mpo_check_kld_stat = mac_test_check_kld_stat,
	.mpo_check_kld_unload = mac_test_check_kld_unload,
	.mpo_check_mount_stat = mac_test_check_mount_stat,
	.mpo_check_pipe_ioctl = mac_test_check_pipe_ioctl,
	.mpo_check_pipe_poll = mac_test_check_pipe_poll,
	.mpo_check_pipe_read = mac_test_check_pipe_read,
	.mpo_check_pipe_relabel = mac_test_check_pipe_relabel,
	.mpo_check_pipe_stat = mac_test_check_pipe_stat,
	.mpo_check_pipe_write = mac_test_check_pipe_write,
	.mpo_check_proc_debug = mac_test_check_proc_debug,
	.mpo_check_proc_sched = mac_test_check_proc_sched,
	.mpo_check_proc_signal = mac_test_check_proc_signal,
	.mpo_check_socket_bind = mac_test_check_socket_bind,
	.mpo_check_socket_connect = mac_test_check_socket_connect,
	.mpo_check_socket_deliver = mac_test_check_socket_deliver,
	.mpo_check_socket_listen = mac_test_check_socket_listen,
	.mpo_check_socket_relabel = mac_test_check_socket_relabel,
	.mpo_check_socket_visible = mac_test_check_socket_visible,
	.mpo_check_sysarch_ioperm = mac_test_check_sysarch_ioperm,
	.mpo_check_system_acct = mac_test_check_system_acct,
	.mpo_check_system_reboot = mac_test_check_system_reboot,
	.mpo_check_system_settime = mac_test_check_system_settime,
	.mpo_check_system_swapon = mac_test_check_system_swapon,
	.mpo_check_system_swapoff = mac_test_check_system_swapoff,
	.mpo_check_system_sysctl = mac_test_check_system_sysctl,
	.mpo_check_vnode_access = mac_test_check_vnode_access,
	.mpo_check_vnode_chdir = mac_test_check_vnode_chdir,
	.mpo_check_vnode_chroot = mac_test_check_vnode_chroot,
	.mpo_check_vnode_create = mac_test_check_vnode_create,
	.mpo_check_vnode_delete = mac_test_check_vnode_delete,
	.mpo_check_vnode_deleteacl = mac_test_check_vnode_deleteacl,
	.mpo_check_vnode_deleteextattr = mac_test_check_vnode_deleteextattr,
	.mpo_check_vnode_exec = mac_test_check_vnode_exec,
	.mpo_check_vnode_getacl = mac_test_check_vnode_getacl,
	.mpo_check_vnode_getextattr = mac_test_check_vnode_getextattr,
	.mpo_check_vnode_link = mac_test_check_vnode_link,
	.mpo_check_vnode_listextattr = mac_test_check_vnode_listextattr,
	.mpo_check_vnode_lookup = mac_test_check_vnode_lookup,
	.mpo_check_vnode_mmap = mac_test_check_vnode_mmap,
	.mpo_check_vnode_mprotect = mac_test_check_vnode_mprotect,
	.mpo_check_vnode_open = mac_test_check_vnode_open,
	.mpo_check_vnode_poll = mac_test_check_vnode_poll,
	.mpo_check_vnode_read = mac_test_check_vnode_read,
	.mpo_check_vnode_readdir = mac_test_check_vnode_readdir,
	.mpo_check_vnode_readlink = mac_test_check_vnode_readlink,
	.mpo_check_vnode_relabel = mac_test_check_vnode_relabel,
	.mpo_check_vnode_rename_from = mac_test_check_vnode_rename_from,
	.mpo_check_vnode_rename_to = mac_test_check_vnode_rename_to,
	.mpo_check_vnode_revoke = mac_test_check_vnode_revoke,
	.mpo_check_vnode_setacl = mac_test_check_vnode_setacl,
	.mpo_check_vnode_setextattr = mac_test_check_vnode_setextattr,
	.mpo_check_vnode_setflags = mac_test_check_vnode_setflags,
	.mpo_check_vnode_setmode = mac_test_check_vnode_setmode,
	.mpo_check_vnode_setowner = mac_test_check_vnode_setowner,
	.mpo_check_vnode_setutimes = mac_test_check_vnode_setutimes,
	.mpo_check_vnode_stat = mac_test_check_vnode_stat,
	.mpo_check_vnode_write = mac_test_check_vnode_write,
};

MAC_POLICY_SET(&mac_test_ops, mac_test, "TrustedBSD MAC/Test",
    MPC_LOADTIME_FLAG_UNLOADOK | MPC_LOADTIME_FLAG_LABELMBUFS, &test_slot);
