/*-
 * Copyright (c) 1999-2002, 2007 Robert N. M. Watson
 * Copyright (c) 2001-2005 McAfee, Inc.
 * All rights reserved.
 *
 * This software was developed by Robert Watson for the TrustedBSD Project.
 *
 * This software was developed for the FreeBSD Project in part by McAfee
 * Research, the Security Research Division of McAfee, Inc. under
 * DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the DARPA
 * CHATS research program.
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
 *
 * MAC Test policy - tests MAC Framework labeling by assigning object class
 * magic numbers to each label and validates that each time an object label
 * is passed into the policy, it has a consistent object type, catching
 * incorrectly passed labels, labels passed after free, etc.
 */

#include <sys/param.h>
#include <sys/acl.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/ksem.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mount.h>
#include <sys/msg.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sx.h>
#include <sys/sysctl.h>

#include <fs/devfs/devfs.h>

#include <net/bpfdesc.h>
#include <net/if.h>
#include <net/if_types.h>
#include <net/if_var.h>

#include <security/mac/mac_policy.h>

SYSCTL_DECL(_security_mac);

SYSCTL_NODE(_security_mac, OID_AUTO, test, CTLFLAG_RW, 0,
    "TrustedBSD mac_test policy controls");

#define	MAGIC_BPF	0xfe1ad1b6
#define	MAGIC_DEVFS	0x9ee79c32
#define	MAGIC_IFNET	0xc218b120
#define	MAGIC_INPCB	0x4440f7bb
#define	MAGIC_IPQ	0x206188ef
#define	MAGIC_MBUF	0xbbefa5bb
#define	MAGIC_MOUNT	0xc7c46e47
#define	MAGIC_SOCKET	0x9199c6cd
#define	MAGIC_SYSV_MSG	0x8bbba61e
#define	MAGIC_SYSV_MSQ	0xea672391
#define	MAGIC_SYSV_SEM	0x896e8a0b
#define	MAGIC_SYSV_SHM	0x76119ab0
#define	MAGIC_PIPE	0xdc6c9919
#define	MAGIC_POSIX_SEM	0x78ae980c
#define	MAGIC_PROC	0x3b4be98f
#define	MAGIC_CRED	0x9a5a4987
#define	MAGIC_VNODE	0x1a67a45c
#define	MAGIC_FREE	0x849ba1fd

#define	SLOT(x)	mac_label_get((x), test_slot)
#define	SLOT_SET(x, v)	mac_label_set((x), test_slot, (v))

static int	test_slot;
SYSCTL_INT(_security_mac_test, OID_AUTO, slot, CTLFLAG_RD,
    &test_slot, 0, "Slot allocated by framework");

SYSCTL_NODE(_security_mac_test, OID_AUTO, counter, CTLFLAG_RW, 0,
    "TrustedBSD mac_test counters controls");

#define	COUNTER_DECL(variable)						\
	static int counter_##variable;					\
	SYSCTL_INT(_security_mac_test_counter, OID_AUTO, variable,	\
	CTLFLAG_RD, &counter_##variable, 0, #variable)

#define	COUNTER_INC(variable)	atomic_add_int(&counter_##variable, 1)

#ifdef KDB
#define	DEBUGGER(func, string)	kdb_enter((string))
#else
#define	DEBUGGER(func, string)	printf("mac_test: %s: %s\n", (func), (string))
#endif

#define	LABEL_CHECK(label, magic) do {					\
	if (label != NULL) {						\
		KASSERT(SLOT(label) == magic ||	SLOT(label) == 0,	\
		    ("%s: bad %s label", __func__, #magic));		\
	}								\
} while (0)

#define	LABEL_DESTROY(label, magic) do {				\
	if (SLOT(label) == magic || SLOT(label) == 0) {			\
		SLOT_SET(label, MAGIC_FREE);				\
	} else if (SLOT(label) == MAGIC_FREE) {				\
		DEBUGGER("%s: dup destroy", __func__);			\
	} else {							\
		DEBUGGER("%s: corrupted label", __func__);		\
	}								\
} while (0)

#define	LABEL_INIT(label, magic) do {					\
	SLOT_SET(label, magic);						\
} while (0)

#define	LABEL_NOTFREE(label) do {					\
	KASSERT(SLOT(label) != MAGIC_FREE,				\
	    ("%s: destroyed label", __func__));				\
} while (0)

/*
 * Label operations.
 */
COUNTER_DECL(init_bpfdesc_label);
static void
mac_test_init_bpfdesc_label(struct label *label)
{

	LABEL_INIT(label, MAGIC_BPF);
	COUNTER_INC(init_bpfdesc_label);
}

COUNTER_DECL(init_cred_label);
static void
mac_test_init_cred_label(struct label *label)
{

	LABEL_INIT(label, MAGIC_CRED);
	COUNTER_INC(init_cred_label);
}

COUNTER_DECL(init_devfs_label);
static void
mac_test_init_devfs_label(struct label *label)
{

	LABEL_INIT(label, MAGIC_DEVFS);
	COUNTER_INC(init_devfs_label);
}

COUNTER_DECL(init_ifnet_label);
static void
mac_test_init_ifnet_label(struct label *label)
{

	LABEL_INIT(label, MAGIC_IFNET);
	COUNTER_INC(init_ifnet_label);
}

COUNTER_DECL(init_inpcb_label);
static int
mac_test_init_inpcb_label(struct label *label, int flag)
{

	if (flag & M_WAITOK)
		WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL,
		    "mac_test_init_inpcb_label() at %s:%d", __FILE__,
		    __LINE__);

	LABEL_INIT(label, MAGIC_INPCB);
	COUNTER_INC(init_inpcb_label);
	return (0);
}

COUNTER_DECL(init_sysv_msg_label);
static void
mac_test_init_sysv_msgmsg_label(struct label *label)
{
	LABEL_INIT(label, MAGIC_SYSV_MSG);
	COUNTER_INC(init_sysv_msg_label);
}

COUNTER_DECL(init_sysv_msq_label);
static void
mac_test_init_sysv_msgqueue_label(struct label *label)
{
	LABEL_INIT(label, MAGIC_SYSV_MSQ);
	COUNTER_INC(init_sysv_msq_label);
}

COUNTER_DECL(init_sysv_sem_label);
static void
mac_test_init_sysv_sem_label(struct label *label)
{
	LABEL_INIT(label, MAGIC_SYSV_SEM);
	COUNTER_INC(init_sysv_sem_label);
}

COUNTER_DECL(init_sysv_shm_label);
static void
mac_test_init_sysv_shm_label(struct label *label)
{
	LABEL_INIT(label, MAGIC_SYSV_SHM);
	COUNTER_INC(init_sysv_shm_label);
}

COUNTER_DECL(init_ipq_label);
static int
mac_test_init_ipq_label(struct label *label, int flag)
{

	if (flag & M_WAITOK)
		WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL,
		    "mac_test_init_ipq_label() at %s:%d", __FILE__,
		    __LINE__);

	LABEL_INIT(label, MAGIC_IPQ);
	COUNTER_INC(init_ipq_label);
	return (0);
}

COUNTER_DECL(init_mbuf_label);
static int
mac_test_init_mbuf_label(struct label *label, int flag)
{

	if (flag & M_WAITOK)
		WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL,
		    "mac_test_init_mbuf_label() at %s:%d", __FILE__,
		    __LINE__);

	LABEL_INIT(label, MAGIC_MBUF);
	COUNTER_INC(init_mbuf_label);
	return (0);
}

COUNTER_DECL(init_mount_label);
static void
mac_test_init_mount_label(struct label *label)
{

	LABEL_INIT(label, MAGIC_MOUNT);
	COUNTER_INC(init_mount_label);
}

COUNTER_DECL(init_socket_label);
static int
mac_test_init_socket_label(struct label *label, int flag)
{

	if (flag & M_WAITOK)
		WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL,
		    "mac_test_init_socket_label() at %s:%d", __FILE__,
		    __LINE__);

	LABEL_INIT(label, MAGIC_SOCKET);
	COUNTER_INC(init_socket_label);
	return (0);
}

COUNTER_DECL(init_socket_peer_label);
static int
mac_test_init_socket_peer_label(struct label *label, int flag)
{

	if (flag & M_WAITOK)
		WITNESS_WARN(WARN_GIANTOK | WARN_SLEEPOK, NULL,
		    "mac_test_init_socket_peer_label() at %s:%d", __FILE__,
		    __LINE__);

	LABEL_INIT(label, MAGIC_SOCKET);
	COUNTER_INC(init_socket_peer_label);
	return (0);
}

COUNTER_DECL(init_pipe_label);
static void
mac_test_init_pipe_label(struct label *label)
{

	LABEL_INIT(label, MAGIC_PIPE);
	COUNTER_INC(init_pipe_label);
}

COUNTER_DECL(init_posix_sem_label);
static void
mac_test_init_posix_sem_label(struct label *label)
{

	LABEL_INIT(label, MAGIC_POSIX_SEM);
	COUNTER_INC(init_posix_sem_label);
}

COUNTER_DECL(init_proc_label);
static void
mac_test_init_proc_label(struct label *label)
{

	LABEL_INIT(label, MAGIC_PROC);
	COUNTER_INC(init_proc_label);
}

COUNTER_DECL(init_vnode_label);
static void
mac_test_init_vnode_label(struct label *label)
{

	LABEL_INIT(label, MAGIC_VNODE);
	COUNTER_INC(init_vnode_label);
}

COUNTER_DECL(destroy_bpfdesc_label);
static void
mac_test_destroy_bpfdesc_label(struct label *label)
{

	LABEL_DESTROY(label, MAGIC_BPF);
	COUNTER_INC(destroy_bpfdesc_label);
}

COUNTER_DECL(destroy_cred_label);
static void
mac_test_destroy_cred_label(struct label *label)
{

	LABEL_DESTROY(label, MAGIC_CRED);
	COUNTER_INC(destroy_cred_label);
}

COUNTER_DECL(destroy_devfs_label);
static void
mac_test_destroy_devfs_label(struct label *label)
{

	LABEL_DESTROY(label, MAGIC_DEVFS);
	COUNTER_INC(destroy_devfs_label);
}

COUNTER_DECL(destroy_ifnet_label);
static void
mac_test_destroy_ifnet_label(struct label *label)
{

	LABEL_DESTROY(label, MAGIC_IFNET);
	COUNTER_INC(destroy_ifnet_label);
}

COUNTER_DECL(destroy_inpcb_label);
static void
mac_test_destroy_inpcb_label(struct label *label)
{

	LABEL_DESTROY(label, MAGIC_INPCB);
	COUNTER_INC(destroy_inpcb_label);
}

COUNTER_DECL(destroy_sysv_msg_label);
static void
mac_test_destroy_sysv_msgmsg_label(struct label *label)
{

	LABEL_DESTROY(label, MAGIC_SYSV_MSG);
	COUNTER_INC(destroy_sysv_msg_label);
}

COUNTER_DECL(destroy_sysv_msq_label);
static void
mac_test_destroy_sysv_msgqueue_label(struct label *label)
{

	LABEL_DESTROY(label, MAGIC_SYSV_MSQ);
	COUNTER_INC(destroy_sysv_msq_label);
}

COUNTER_DECL(destroy_sysv_sem_label);
static void
mac_test_destroy_sysv_sem_label(struct label *label)
{

	LABEL_DESTROY(label, MAGIC_SYSV_SEM);
	COUNTER_INC(destroy_sysv_sem_label);
}

COUNTER_DECL(destroy_sysv_shm_label);
static void
mac_test_destroy_sysv_shm_label(struct label *label)
{

	LABEL_DESTROY(label, MAGIC_SYSV_SHM);
	COUNTER_INC(destroy_sysv_shm_label);
}

COUNTER_DECL(destroy_ipq_label);
static void
mac_test_destroy_ipq_label(struct label *label)
{

	LABEL_DESTROY(label, MAGIC_IPQ);
	COUNTER_INC(destroy_ipq_label);
}

COUNTER_DECL(destroy_mbuf_label);
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

	LABEL_DESTROY(label, MAGIC_MBUF);
	COUNTER_INC(destroy_mbuf_label);
}

COUNTER_DECL(destroy_mount_label);
static void
mac_test_destroy_mount_label(struct label *label)
{

	LABEL_DESTROY(label, MAGIC_MOUNT);
	COUNTER_INC(destroy_mount_label);
}

COUNTER_DECL(destroy_socket_label);
static void
mac_test_destroy_socket_label(struct label *label)
{

	LABEL_DESTROY(label, MAGIC_SOCKET);
	COUNTER_INC(destroy_socket_label);
}

COUNTER_DECL(destroy_socket_peer_label);
static void
mac_test_destroy_socket_peer_label(struct label *label)
{

	LABEL_DESTROY(label, MAGIC_SOCKET);
	COUNTER_INC(destroy_socket_peer_label);
}

COUNTER_DECL(destroy_pipe_label);
static void
mac_test_destroy_pipe_label(struct label *label)
{

	LABEL_DESTROY(label, MAGIC_PIPE);
	COUNTER_INC(destroy_pipe_label);
}

COUNTER_DECL(destroy_posix_sem_label);
static void
mac_test_destroy_posix_sem_label(struct label *label)
{

	LABEL_DESTROY(label, MAGIC_POSIX_SEM);
	COUNTER_INC(destroy_posix_sem_label);
}

COUNTER_DECL(destroy_proc_label);
static void
mac_test_destroy_proc_label(struct label *label)
{

	LABEL_DESTROY(label, MAGIC_PROC);
	COUNTER_INC(destroy_proc_label);
}

COUNTER_DECL(destroy_vnode_label);
static void
mac_test_destroy_vnode_label(struct label *label)
{

	LABEL_DESTROY(label, MAGIC_VNODE);
	COUNTER_INC(destroy_vnode_label);
}

COUNTER_DECL(copy_cred_label);
static void
mac_test_copy_cred_label(struct label *src, struct label *dest)
{

	LABEL_CHECK(src, MAGIC_CRED);
	LABEL_CHECK(dest, MAGIC_CRED);
	COUNTER_INC(copy_cred_label);
}

COUNTER_DECL(copy_ifnet_label);
static void
mac_test_copy_ifnet_label(struct label *src, struct label *dest)
{

	LABEL_CHECK(src, MAGIC_IFNET);
	LABEL_CHECK(dest, MAGIC_IFNET);
	COUNTER_INC(copy_ifnet_label);
}

COUNTER_DECL(copy_mbuf_label);
static void
mac_test_copy_mbuf_label(struct label *src, struct label *dest)
{

	LABEL_CHECK(src, MAGIC_MBUF);
	LABEL_CHECK(dest, MAGIC_MBUF);
	COUNTER_INC(copy_mbuf_label);
}

COUNTER_DECL(copy_pipe_label);
static void
mac_test_copy_pipe_label(struct label *src, struct label *dest)
{

	LABEL_CHECK(src, MAGIC_PIPE);
	LABEL_CHECK(dest, MAGIC_PIPE);
	COUNTER_INC(copy_pipe_label);
}

COUNTER_DECL(copy_socket_label);
static void
mac_test_copy_socket_label(struct label *src, struct label *dest)
{

	LABEL_CHECK(src, MAGIC_SOCKET);
	LABEL_CHECK(dest, MAGIC_SOCKET);
	COUNTER_INC(copy_socket_label);
}

COUNTER_DECL(copy_vnode_label);
static void
mac_test_copy_vnode_label(struct label *src, struct label *dest)
{

	LABEL_CHECK(src, MAGIC_VNODE);
	LABEL_CHECK(dest, MAGIC_VNODE);
	COUNTER_INC(copy_vnode_label);
}

COUNTER_DECL(externalize_label);
static int
mac_test_externalize_label(struct label *label, char *element_name,
    struct sbuf *sb, int *claimed)
{

	LABEL_NOTFREE(label);
	COUNTER_INC(externalize_label);

	return (0);
}

COUNTER_DECL(internalize_label);
static int
mac_test_internalize_label(struct label *label, char *element_name,
    char *element_data, int *claimed)
{

	LABEL_NOTFREE(label);
	COUNTER_INC(internalize_label);

	return (0);
}

/*
 * Labeling event operations: file system objects, and things that look
 * a lot like file system objects.
 */
COUNTER_DECL(associate_vnode_devfs);
static void
mac_test_associate_vnode_devfs(struct mount *mp, struct label *mplabel,
    struct devfs_dirent *de, struct label *delabel, struct vnode *vp,
    struct label *vplabel)
{

	LABEL_CHECK(mplabel, MAGIC_MOUNT);
	LABEL_CHECK(delabel, MAGIC_DEVFS);
	LABEL_CHECK(vplabel, MAGIC_VNODE);
	COUNTER_INC(associate_vnode_devfs);
}

COUNTER_DECL(associate_vnode_extattr);
static int
mac_test_associate_vnode_extattr(struct mount *mp, struct label *mplabel,
    struct vnode *vp, struct label *vplabel)
{

	LABEL_CHECK(mplabel, MAGIC_MOUNT);
	LABEL_CHECK(vplabel, MAGIC_VNODE);
	COUNTER_INC(associate_vnode_extattr);

	return (0);
}

COUNTER_DECL(associate_vnode_singlelabel);
static void
mac_test_associate_vnode_singlelabel(struct mount *mp, struct label *mplabel,
    struct vnode *vp, struct label *vplabel)
{

	LABEL_CHECK(mplabel, MAGIC_MOUNT);
	LABEL_CHECK(vplabel, MAGIC_VNODE);
	COUNTER_INC(associate_vnode_singlelabel);
}

COUNTER_DECL(create_devfs_device);
static void
mac_test_create_devfs_device(struct ucred *cred, struct mount *mp,
    struct cdev *dev, struct devfs_dirent *de, struct label *delabel)
{

	if (cred != NULL)
		LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(delabel, MAGIC_DEVFS);
	COUNTER_INC(create_devfs_device);
}

COUNTER_DECL(create_devfs_directory);
static void
mac_test_create_devfs_directory(struct mount *mp, char *dirname,
    int dirnamelen, struct devfs_dirent *de, struct label *delabel)
{

	LABEL_CHECK(delabel, MAGIC_DEVFS);
	COUNTER_INC(create_devfs_directory);
}

COUNTER_DECL(create_devfs_symlink);
static void
mac_test_create_devfs_symlink(struct ucred *cred, struct mount *mp,
    struct devfs_dirent *dd, struct label *ddlabel, struct devfs_dirent *de,
    struct label *delabel)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(ddlabel, MAGIC_DEVFS);
	LABEL_CHECK(delabel, MAGIC_DEVFS);
	COUNTER_INC(create_devfs_symlink);
}

COUNTER_DECL(create_vnode_extattr);
static int
mac_test_create_vnode_extattr(struct ucred *cred, struct mount *mp,
    struct label *mplabel, struct vnode *dvp, struct label *dvplabel,
    struct vnode *vp, struct label *vplabel, struct componentname *cnp)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(mplabel, MAGIC_MOUNT);
	LABEL_CHECK(dvplabel, MAGIC_VNODE);
	COUNTER_INC(create_vnode_extattr);

	return (0);
}

COUNTER_DECL(create_mount);
static void
mac_test_create_mount(struct ucred *cred, struct mount *mp,
    struct label *mplabel)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(mplabel, MAGIC_MOUNT);
	COUNTER_INC(create_mount);
}

COUNTER_DECL(relabel_vnode);
static void
mac_test_relabel_vnode(struct ucred *cred, struct vnode *vp,
    struct label *vplabel, struct label *label)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(vplabel, MAGIC_VNODE);
	LABEL_CHECK(label, MAGIC_VNODE);
	COUNTER_INC(relabel_vnode);
}

COUNTER_DECL(setlabel_vnode_extattr);
static int
mac_test_setlabel_vnode_extattr(struct ucred *cred, struct vnode *vp,
    struct label *vplabel, struct label *intlabel)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(vplabel, MAGIC_VNODE);
	LABEL_CHECK(intlabel, MAGIC_VNODE);
	COUNTER_INC(setlabel_vnode_extattr);

	return (0);
}

COUNTER_DECL(update_devfs);
static void
mac_test_update_devfs(struct mount *mp, struct devfs_dirent *devfs_dirent,
    struct label *direntlabel, struct vnode *vp, struct label *vplabel)
{

	LABEL_CHECK(direntlabel, MAGIC_DEVFS);
	LABEL_CHECK(vplabel, MAGIC_VNODE);
	COUNTER_INC(update_devfs);
}

/*
 * Labeling event operations: IPC object.
 */
COUNTER_DECL(create_mbuf_from_socket);
static void
mac_test_create_mbuf_from_socket(struct socket *so, struct label *socketlabel,
    struct mbuf *m, struct label *mbuflabel)
{

	LABEL_CHECK(socketlabel, MAGIC_SOCKET);
	LABEL_CHECK(mbuflabel, MAGIC_MBUF);
	COUNTER_INC(create_mbuf_from_socket);
}

COUNTER_DECL(create_socket);
static void
mac_test_create_socket(struct ucred *cred, struct socket *socket,
   struct label *socketlabel)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(socketlabel, MAGIC_SOCKET);
	COUNTER_INC(create_socket);
}

COUNTER_DECL(create_pipe);
static void
mac_test_create_pipe(struct ucred *cred, struct pipepair *pp,
   struct label *pipelabel)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(pipelabel, MAGIC_PIPE);
	COUNTER_INC(create_pipe);
}

COUNTER_DECL(create_posix_sem);
static void
mac_test_create_posix_sem(struct ucred *cred, struct ksem *ksem,
   struct label *posixlabel)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(posixlabel, MAGIC_POSIX_SEM);
	COUNTER_INC(create_posix_sem);
}

COUNTER_DECL(create_socket_from_socket);
static void
mac_test_create_socket_from_socket(struct socket *oldsocket,
    struct label *oldsocketlabel, struct socket *newsocket,
    struct label *newsocketlabel)
{

	LABEL_CHECK(oldsocketlabel, MAGIC_SOCKET);
	LABEL_CHECK(newsocketlabel, MAGIC_SOCKET);
	COUNTER_INC(create_socket_from_socket);
}

COUNTER_DECL(relabel_socket);
static void
mac_test_relabel_socket(struct ucred *cred, struct socket *socket,
    struct label *socketlabel, struct label *newlabel)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(newlabel, MAGIC_SOCKET);
	COUNTER_INC(relabel_socket);
}

COUNTER_DECL(relabel_pipe);
static void
mac_test_relabel_pipe(struct ucred *cred, struct pipepair *pp,
    struct label *pipelabel, struct label *newlabel)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(pipelabel, MAGIC_PIPE);
	LABEL_CHECK(newlabel, MAGIC_PIPE);
	COUNTER_INC(relabel_pipe);
}

COUNTER_DECL(set_socket_peer_from_mbuf);
static void
mac_test_set_socket_peer_from_mbuf(struct mbuf *mbuf, struct label *mbuflabel,
    struct socket *socket, struct label *socketpeerlabel)
{

	LABEL_CHECK(mbuflabel, MAGIC_MBUF);
	LABEL_CHECK(socketpeerlabel, MAGIC_SOCKET);
	COUNTER_INC(set_socket_peer_from_mbuf);
}

/*
 * Labeling event operations: network objects.
 */
COUNTER_DECL(set_socket_peer_from_socket);
static void
mac_test_set_socket_peer_from_socket(struct socket *oldsocket,
    struct label *oldsocketlabel, struct socket *newsocket,
    struct label *newsocketpeerlabel)
{

	LABEL_CHECK(oldsocketlabel, MAGIC_SOCKET);
	LABEL_CHECK(newsocketpeerlabel, MAGIC_SOCKET);
	COUNTER_INC(set_socket_peer_from_socket);
}

COUNTER_DECL(create_bpfdesc);
static void
mac_test_create_bpfdesc(struct ucred *cred, struct bpf_d *bpf_d,
    struct label *bpflabel)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(bpflabel, MAGIC_BPF);
	COUNTER_INC(create_bpfdesc);
}

COUNTER_DECL(create_datagram_from_ipq);
static void
mac_test_create_datagram_from_ipq(struct ipq *ipq, struct label *ipqlabel,
    struct mbuf *datagram, struct label *datagramlabel)
{

	LABEL_CHECK(ipqlabel, MAGIC_IPQ);
	LABEL_CHECK(datagramlabel, MAGIC_MBUF);
	COUNTER_INC(create_datagram_from_ipq);
}

COUNTER_DECL(create_fragment);
static void
mac_test_create_fragment(struct mbuf *datagram, struct label *datagramlabel,
    struct mbuf *fragment, struct label *fragmentlabel)
{

	LABEL_CHECK(datagramlabel, MAGIC_MBUF);
	LABEL_CHECK(fragmentlabel, MAGIC_MBUF);
	COUNTER_INC(create_fragment);
}

COUNTER_DECL(create_ifnet);
static void
mac_test_create_ifnet(struct ifnet *ifnet, struct label *ifnetlabel)
{

	LABEL_CHECK(ifnetlabel, MAGIC_IFNET);
	COUNTER_INC(create_ifnet);
}

COUNTER_DECL(create_inpcb_from_socket);
static void
mac_test_create_inpcb_from_socket(struct socket *so, struct label *solabel,
    struct inpcb *inp, struct label *inplabel)
{

	LABEL_CHECK(solabel, MAGIC_SOCKET);
	LABEL_CHECK(inplabel, MAGIC_INPCB);
	COUNTER_INC(create_inpcb_from_socket);
}

COUNTER_DECL(create_sysv_msgmsg);
static void
mac_test_create_sysv_msgmsg(struct ucred *cred, struct msqid_kernel *msqkptr,
    struct label *msqlabel, struct msg *msgptr, struct label *msglabel)
{

	LABEL_CHECK(msglabel, MAGIC_SYSV_MSG);
	LABEL_CHECK(msqlabel, MAGIC_SYSV_MSQ);
	COUNTER_INC(create_sysv_msgmsg);
}

COUNTER_DECL(create_sysv_msgqueue);
static void
mac_test_create_sysv_msgqueue(struct ucred *cred,
    struct msqid_kernel *msqkptr, struct label *msqlabel)
{

	LABEL_CHECK(msqlabel, MAGIC_SYSV_MSQ);
	COUNTER_INC(create_sysv_msgqueue);
}

COUNTER_DECL(create_sysv_sem);
static void
mac_test_create_sysv_sem(struct ucred *cred, struct semid_kernel *semakptr,
    struct label *semalabel)
{

	LABEL_CHECK(semalabel, MAGIC_SYSV_SEM);
	COUNTER_INC(create_sysv_sem);
}

COUNTER_DECL(create_sysv_shm);
static void
mac_test_create_sysv_shm(struct ucred *cred, struct shmid_kernel *shmsegptr,
    struct label *shmlabel)
{

	LABEL_CHECK(shmlabel, MAGIC_SYSV_SHM);
	COUNTER_INC(create_sysv_shm);
}

COUNTER_DECL(create_ipq);
static void
mac_test_create_ipq(struct mbuf *fragment, struct label *fragmentlabel,
    struct ipq *ipq, struct label *ipqlabel)
{

	LABEL_CHECK(fragmentlabel, MAGIC_MBUF);
	LABEL_CHECK(ipqlabel, MAGIC_IPQ);
	COUNTER_INC(create_ipq);
}

COUNTER_DECL(create_mbuf_from_inpcb);
static void
mac_test_create_mbuf_from_inpcb(struct inpcb *inp, struct label *inplabel,
    struct mbuf *m, struct label *mlabel)
{

	LABEL_CHECK(inplabel, MAGIC_INPCB);
	LABEL_CHECK(mlabel, MAGIC_MBUF);
	COUNTER_INC(create_mbuf_from_inpcb);
}

COUNTER_DECL(create_mbuf_linklayer);
static void
mac_test_create_mbuf_linklayer(struct ifnet *ifnet, struct label *ifnetlabel,
    struct mbuf *mbuf, struct label *mbuflabel)
{

	LABEL_CHECK(ifnetlabel, MAGIC_IFNET);
	LABEL_CHECK(mbuflabel, MAGIC_MBUF);
	COUNTER_INC(create_mbuf_linklayer);
}

COUNTER_DECL(create_mbuf_from_bpfdesc);
static void
mac_test_create_mbuf_from_bpfdesc(struct bpf_d *bpf_d, struct label *bpflabel,
    struct mbuf *mbuf, struct label *mbuflabel)
{

	LABEL_CHECK(bpflabel, MAGIC_BPF);
	LABEL_CHECK(mbuflabel, MAGIC_MBUF);
	COUNTER_INC(create_mbuf_from_bpfdesc);
}

COUNTER_DECL(create_mbuf_from_ifnet);
static void
mac_test_create_mbuf_from_ifnet(struct ifnet *ifnet, struct label *ifnetlabel,
    struct mbuf *m, struct label *mbuflabel)
{

	LABEL_CHECK(ifnetlabel, MAGIC_IFNET);
	LABEL_CHECK(mbuflabel, MAGIC_MBUF);
	COUNTER_INC(create_mbuf_from_ifnet);
}

COUNTER_DECL(create_mbuf_multicast_encap);
static void
mac_test_create_mbuf_multicast_encap(struct mbuf *oldmbuf,
    struct label *oldmbuflabel, struct ifnet *ifnet, struct label *ifnetlabel,
    struct mbuf *newmbuf, struct label *newmbuflabel)
{

	LABEL_CHECK(oldmbuflabel, MAGIC_MBUF);
	LABEL_CHECK(ifnetlabel, MAGIC_IFNET);
	LABEL_CHECK(newmbuflabel, MAGIC_MBUF);
	COUNTER_INC(create_mbuf_multicast_encap);
}

COUNTER_DECL(create_mbuf_netlayer);
static void
mac_test_create_mbuf_netlayer(struct mbuf *oldmbuf,
    struct label *oldmbuflabel, struct mbuf *newmbuf,
    struct label *newmbuflabel)
{

	LABEL_CHECK(oldmbuflabel, MAGIC_MBUF);
	LABEL_CHECK(newmbuflabel, MAGIC_MBUF);
	COUNTER_INC(create_mbuf_netlayer);
}

COUNTER_DECL(fragment_match);
static int
mac_test_fragment_match(struct mbuf *fragment, struct label *fragmentlabel,
    struct ipq *ipq, struct label *ipqlabel)
{

	LABEL_CHECK(fragmentlabel, MAGIC_MBUF);
	LABEL_CHECK(ipqlabel, MAGIC_IPQ);
	COUNTER_INC(fragment_match);

	return (1);
}

COUNTER_DECL(reflect_mbuf_icmp);
static void
mac_test_reflect_mbuf_icmp(struct mbuf *m, struct label *mlabel)
{

	LABEL_CHECK(mlabel, MAGIC_MBUF);
	COUNTER_INC(reflect_mbuf_icmp);
}

COUNTER_DECL(reflect_mbuf_tcp);
static void
mac_test_reflect_mbuf_tcp(struct mbuf *m, struct label *mlabel)
{

	LABEL_CHECK(mlabel, MAGIC_MBUF);
	COUNTER_INC(reflect_mbuf_tcp);
}

COUNTER_DECL(relabel_ifnet);
static void
mac_test_relabel_ifnet(struct ucred *cred, struct ifnet *ifnet,
    struct label *ifnetlabel, struct label *newlabel)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(ifnetlabel, MAGIC_IFNET);
	LABEL_CHECK(newlabel, MAGIC_IFNET);
	COUNTER_INC(relabel_ifnet);
}

COUNTER_DECL(update_ipq);
static void
mac_test_update_ipq(struct mbuf *fragment, struct label *fragmentlabel,
    struct ipq *ipq, struct label *ipqlabel)
{

	LABEL_CHECK(fragmentlabel, MAGIC_MBUF);
	LABEL_CHECK(ipqlabel, MAGIC_IPQ);
	COUNTER_INC(update_ipq);
}

COUNTER_DECL(inpcb_sosetlabel);
static void
mac_test_inpcb_sosetlabel(struct socket *so, struct label *solabel,
    struct inpcb *inp, struct label *inplabel)
{

	LABEL_CHECK(solabel, MAGIC_SOCKET);
	LABEL_CHECK(inplabel, MAGIC_INPCB);
	COUNTER_INC(inpcb_sosetlabel);
}

/*
 * Labeling event operations: processes.
 */
COUNTER_DECL(execve_transition);
static void
mac_test_execve_transition(struct ucred *old, struct ucred *new,
    struct vnode *vp, struct label *filelabel,
    struct label *interpvplabel, struct image_params *imgp,
    struct label *execlabel)
{

	LABEL_CHECK(old->cr_label, MAGIC_CRED);
	LABEL_CHECK(new->cr_label, MAGIC_CRED);
	LABEL_CHECK(filelabel, MAGIC_VNODE);
	LABEL_CHECK(interpvplabel, MAGIC_VNODE);
	LABEL_CHECK(execlabel, MAGIC_CRED);
	COUNTER_INC(execve_transition);
}

COUNTER_DECL(execve_will_transition);
static int
mac_test_execve_will_transition(struct ucred *old, struct vnode *vp,
    struct label *filelabel, struct label *interpvplabel,
    struct image_params *imgp, struct label *execlabel)
{

	LABEL_CHECK(old->cr_label, MAGIC_CRED);
	LABEL_CHECK(filelabel, MAGIC_VNODE);
	LABEL_CHECK(interpvplabel, MAGIC_VNODE);
	LABEL_CHECK(execlabel, MAGIC_CRED);
	COUNTER_INC(execve_will_transition);

	return (0);
}

COUNTER_DECL(create_proc0);
static void
mac_test_create_proc0(struct ucred *cred)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	COUNTER_INC(create_proc0);
}

COUNTER_DECL(create_proc1);
static void
mac_test_create_proc1(struct ucred *cred)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	COUNTER_INC(create_proc1);
}

COUNTER_DECL(relabel_cred);
static void
mac_test_relabel_cred(struct ucred *cred, struct label *newlabel)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(newlabel, MAGIC_CRED);
	COUNTER_INC(relabel_cred);
}

COUNTER_DECL(thread_userret);
static void
mac_test_thread_userret(struct thread *td)
{

	COUNTER_INC(thread_userret);
}

/*
 * Label cleanup/flush operations
 */
COUNTER_DECL(cleanup_sysv_msgmsg);
static void
mac_test_cleanup_sysv_msgmsg(struct label *msglabel)
{

	LABEL_CHECK(msglabel, MAGIC_SYSV_MSG);
	COUNTER_INC(cleanup_sysv_msgmsg);
}

COUNTER_DECL(cleanup_sysv_msgqueue);
static void
mac_test_cleanup_sysv_msgqueue(struct label *msqlabel)
{

	LABEL_CHECK(msqlabel, MAGIC_SYSV_MSQ);
	COUNTER_INC(cleanup_sysv_msgqueue);
}

COUNTER_DECL(cleanup_sysv_sem);
static void
mac_test_cleanup_sysv_sem(struct label *semalabel)
{

	LABEL_CHECK(semalabel, MAGIC_SYSV_SEM);
	COUNTER_INC(cleanup_sysv_sem);
}

COUNTER_DECL(cleanup_sysv_shm);
static void
mac_test_cleanup_sysv_shm(struct label *shmlabel)
{

	LABEL_CHECK(shmlabel, MAGIC_SYSV_SHM);
	COUNTER_INC(cleanup_sysv_shm);
}

/*
 * Access control checks.
 */
COUNTER_DECL(check_bpfdesc_receive);
static int
mac_test_check_bpfdesc_receive(struct bpf_d *bpf_d, struct label *bpflabel,
    struct ifnet *ifnet, struct label *ifnetlabel)
{

	LABEL_CHECK(bpflabel, MAGIC_BPF);
	LABEL_CHECK(ifnetlabel, MAGIC_IFNET);
	COUNTER_INC(check_bpfdesc_receive);

	return (0);
}

COUNTER_DECL(check_cred_relabel);
static int
mac_test_check_cred_relabel(struct ucred *cred, struct label *newlabel)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(newlabel, MAGIC_CRED);
	COUNTER_INC(check_cred_relabel);

	return (0);
}

COUNTER_DECL(check_cred_visible);
static int
mac_test_check_cred_visible(struct ucred *u1, struct ucred *u2)
{

	LABEL_CHECK(u1->cr_label, MAGIC_CRED);
	LABEL_CHECK(u2->cr_label, MAGIC_CRED);
	COUNTER_INC(check_cred_visible);

	return (0);
}

COUNTER_DECL(check_ifnet_relabel);
static int
mac_test_check_ifnet_relabel(struct ucred *cred, struct ifnet *ifnet,
    struct label *ifnetlabel, struct label *newlabel)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(ifnetlabel, MAGIC_IFNET);
	LABEL_CHECK(newlabel, MAGIC_IFNET);
	COUNTER_INC(check_ifnet_relabel);

	return (0);
}

COUNTER_DECL(check_ifnet_transmit);
static int
mac_test_check_ifnet_transmit(struct ifnet *ifnet, struct label *ifnetlabel,
    struct mbuf *m, struct label *mbuflabel)
{

	LABEL_CHECK(ifnetlabel, MAGIC_IFNET);
	LABEL_CHECK(mbuflabel, MAGIC_MBUF);
	COUNTER_INC(check_ifnet_transmit);

	return (0);
}

COUNTER_DECL(check_inpcb_deliver);
static int
mac_test_check_inpcb_deliver(struct inpcb *inp, struct label *inplabel,
    struct mbuf *m, struct label *mlabel)
{

	LABEL_CHECK(inplabel, MAGIC_INPCB);
	LABEL_CHECK(mlabel, MAGIC_MBUF);
	COUNTER_INC(check_inpcb_deliver);

	return (0);
}

COUNTER_DECL(check_sysv_msgmsq);
static int
mac_test_check_sysv_msgmsq(struct ucred *cred, struct msg *msgptr,
    struct label *msglabel, struct msqid_kernel *msqkptr,
    struct label *msqklabel)
{

	LABEL_CHECK(msqklabel, MAGIC_SYSV_MSQ);
	LABEL_CHECK(msglabel, MAGIC_SYSV_MSG);
	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	COUNTER_INC(check_sysv_msgmsq);

  	return (0);
}

COUNTER_DECL(check_sysv_msgrcv);
static int
mac_test_check_sysv_msgrcv(struct ucred *cred, struct msg *msgptr,
    struct label *msglabel)
{

	LABEL_CHECK(msglabel, MAGIC_SYSV_MSG);
	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	COUNTER_INC(check_sysv_msgrcv);

	return (0);
}

COUNTER_DECL(check_sysv_msgrmid);
static int
mac_test_check_sysv_msgrmid(struct ucred *cred, struct msg *msgptr,
    struct label *msglabel)
{

	LABEL_CHECK(msglabel, MAGIC_SYSV_MSG);
	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	COUNTER_INC(check_sysv_msgrmid);

	return (0);
}

COUNTER_DECL(check_sysv_msqget);
static int
mac_test_check_sysv_msqget(struct ucred *cred, struct msqid_kernel *msqkptr,
    struct label *msqklabel)
{

	LABEL_CHECK(msqklabel, MAGIC_SYSV_MSQ);
	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	COUNTER_INC(check_sysv_msqget);

	return (0);
}

COUNTER_DECL(check_sysv_msqsnd);
static int
mac_test_check_sysv_msqsnd(struct ucred *cred, struct msqid_kernel *msqkptr,
    struct label *msqklabel)
{

	LABEL_CHECK(msqklabel, MAGIC_SYSV_MSQ);
	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	COUNTER_INC(check_sysv_msqsnd);

	return (0);
}

COUNTER_DECL(check_sysv_msqrcv);
static int
mac_test_check_sysv_msqrcv(struct ucred *cred, struct msqid_kernel *msqkptr,
    struct label *msqklabel)
{

	LABEL_CHECK(msqklabel, MAGIC_SYSV_MSQ);
	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	COUNTER_INC(check_sysv_msqrcv);

	return (0);
}

COUNTER_DECL(check_sysv_msqctl);
static int
mac_test_check_sysv_msqctl(struct ucred *cred, struct msqid_kernel *msqkptr,
    struct label *msqklabel, int cmd)
{

	LABEL_CHECK(msqklabel, MAGIC_SYSV_MSQ);
	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	COUNTER_INC(check_sysv_msqctl);

	return (0);
}

COUNTER_DECL(check_sysv_semctl);
static int
mac_test_check_sysv_semctl(struct ucred *cred, struct semid_kernel *semakptr,
    struct label *semaklabel, int cmd)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(semaklabel, MAGIC_SYSV_SEM);
	COUNTER_INC(check_sysv_semctl);

  	return (0);
}

COUNTER_DECL(check_sysv_semget);
static int
mac_test_check_sysv_semget(struct ucred *cred, struct semid_kernel *semakptr,
    struct label *semaklabel)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(semaklabel, MAGIC_SYSV_SEM);
	COUNTER_INC(check_sysv_semget);

	return (0);
}

COUNTER_DECL(check_sysv_semop);
static int
mac_test_check_sysv_semop(struct ucred *cred, struct semid_kernel *semakptr,
    struct label *semaklabel, size_t accesstype)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(semaklabel, MAGIC_SYSV_SEM);
	COUNTER_INC(check_sysv_semop);

	return (0);
}

COUNTER_DECL(check_sysv_shmat);
static int
mac_test_check_sysv_shmat(struct ucred *cred, struct shmid_kernel *shmsegptr,
    struct label *shmseglabel, int shmflg)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(shmseglabel, MAGIC_SYSV_SHM);
	COUNTER_INC(check_sysv_shmat);

  	return (0);
}

COUNTER_DECL(check_sysv_shmctl);
static int
mac_test_check_sysv_shmctl(struct ucred *cred, struct shmid_kernel *shmsegptr,
    struct label *shmseglabel, int cmd)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(shmseglabel, MAGIC_SYSV_SHM);
	COUNTER_INC(check_sysv_shmctl);

  	return (0);
}

COUNTER_DECL(check_sysv_shmdt);
static int
mac_test_check_sysv_shmdt(struct ucred *cred, struct shmid_kernel *shmsegptr,
    struct label *shmseglabel)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(shmseglabel, MAGIC_SYSV_SHM);
	COUNTER_INC(check_sysv_shmdt);

	return (0);
}

COUNTER_DECL(check_sysv_shmget);
static int
mac_test_check_sysv_shmget(struct ucred *cred, struct shmid_kernel *shmsegptr,
    struct label *shmseglabel, int shmflg)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(shmseglabel, MAGIC_SYSV_SHM);
	COUNTER_INC(check_sysv_shmget);

	return (0);
}

COUNTER_DECL(check_kenv_dump);
static int
mac_test_check_kenv_dump(struct ucred *cred)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	COUNTER_INC(check_kenv_dump);

	return (0);
}

COUNTER_DECL(check_kenv_get);
static int
mac_test_check_kenv_get(struct ucred *cred, char *name)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	COUNTER_INC(check_kenv_get);

	return (0);
}

COUNTER_DECL(check_kenv_set);
static int
mac_test_check_kenv_set(struct ucred *cred, char *name, char *value)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	COUNTER_INC(check_kenv_set);

	return (0);
}

COUNTER_DECL(check_kenv_unset);
static int
mac_test_check_kenv_unset(struct ucred *cred, char *name)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	COUNTER_INC(check_kenv_unset);

	return (0);
}

COUNTER_DECL(check_kld_load);
static int
mac_test_check_kld_load(struct ucred *cred, struct vnode *vp,
    struct label *label)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(label, MAGIC_VNODE);
	COUNTER_INC(check_kld_load);

	return (0);
}

COUNTER_DECL(check_kld_stat);
static int
mac_test_check_kld_stat(struct ucred *cred)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	COUNTER_INC(check_kld_stat);

	return (0);
}

COUNTER_DECL(check_mount_stat);
static int
mac_test_check_mount_stat(struct ucred *cred, struct mount *mp,
    struct label *mplabel)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(mplabel, MAGIC_MOUNT);
	COUNTER_INC(check_mount_stat);

	return (0);
}

COUNTER_DECL(check_pipe_ioctl);
static int
mac_test_check_pipe_ioctl(struct ucred *cred, struct pipepair *pp,
    struct label *pipelabel, unsigned long cmd, void /* caddr_t */ *data)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(pipelabel, MAGIC_PIPE);
	COUNTER_INC(check_pipe_ioctl);

	return (0);
}

COUNTER_DECL(check_pipe_poll);
static int
mac_test_check_pipe_poll(struct ucred *cred, struct pipepair *pp,
    struct label *pipelabel)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(pipelabel, MAGIC_PIPE);
	COUNTER_INC(check_pipe_poll);

	return (0);
}

COUNTER_DECL(check_pipe_read);
static int
mac_test_check_pipe_read(struct ucred *cred, struct pipepair *pp,
    struct label *pipelabel)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(pipelabel, MAGIC_PIPE);
	COUNTER_INC(check_pipe_read);

	return (0);
}

COUNTER_DECL(check_pipe_relabel);
static int
mac_test_check_pipe_relabel(struct ucred *cred, struct pipepair *pp,
    struct label *pipelabel, struct label *newlabel)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(pipelabel, MAGIC_PIPE);
	LABEL_CHECK(newlabel, MAGIC_PIPE);
	COUNTER_INC(check_pipe_relabel);

	return (0);
}

COUNTER_DECL(check_pipe_stat);
static int
mac_test_check_pipe_stat(struct ucred *cred, struct pipepair *pp,
    struct label *pipelabel)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(pipelabel, MAGIC_PIPE);
	COUNTER_INC(check_pipe_stat);

	return (0);
}

COUNTER_DECL(check_pipe_write);
static int
mac_test_check_pipe_write(struct ucred *cred, struct pipepair *pp,
    struct label *pipelabel)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(pipelabel, MAGIC_PIPE);
	COUNTER_INC(check_pipe_write);

	return (0);
}

COUNTER_DECL(check_posix_sem);
static int
mac_test_check_posix_sem(struct ucred *cred, struct ksem *ksemptr,
    struct label *ks_label)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(ks_label, MAGIC_POSIX_SEM);
	COUNTER_INC(check_posix_sem);

	return (0);
}

COUNTER_DECL(check_proc_debug);
static int
mac_test_check_proc_debug(struct ucred *cred, struct proc *p)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(p->p_ucred->cr_label, MAGIC_CRED);
	COUNTER_INC(check_proc_debug);

	return (0);
}

COUNTER_DECL(check_proc_sched);
static int
mac_test_check_proc_sched(struct ucred *cred, struct proc *p)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(p->p_ucred->cr_label, MAGIC_CRED);
	COUNTER_INC(check_proc_sched);

	return (0);
}

COUNTER_DECL(check_proc_signal);
static int
mac_test_check_proc_signal(struct ucred *cred, struct proc *p, int signum)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(p->p_ucred->cr_label, MAGIC_CRED);
	COUNTER_INC(check_proc_signal);

	return (0);
}

COUNTER_DECL(check_proc_setaudit);
static int
mac_test_check_proc_setaudit(struct ucred *cred, struct auditinfo *ai)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	COUNTER_INC(check_proc_setaudit);

	return (0);
}

COUNTER_DECL(check_proc_setaudit_addr);
static int
mac_test_check_proc_setaudit_addr(struct ucred *cred,
    struct auditinfo_addr *aia)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	COUNTER_INC(check_proc_setaudit_addr);

	return (0);
}

COUNTER_DECL(check_proc_setauid);
static int
mac_test_check_proc_setauid(struct ucred *cred, uid_t auid)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	COUNTER_INC(check_proc_setauid);

	return (0);
}

COUNTER_DECL(check_proc_setuid);
static int
mac_test_check_proc_setuid(struct ucred *cred, uid_t uid)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	COUNTER_INC(check_proc_setuid);

	return (0);
}

COUNTER_DECL(check_proc_euid);
static int
mac_test_check_proc_seteuid(struct ucred *cred, uid_t euid)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	COUNTER_INC(check_proc_euid);

	return (0);
}

COUNTER_DECL(check_proc_setgid);
static int
mac_test_check_proc_setgid(struct ucred *cred, gid_t gid)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	COUNTER_INC(check_proc_setgid);

	return (0);
}

COUNTER_DECL(check_proc_setegid);
static int
mac_test_check_proc_setegid(struct ucred *cred, gid_t egid)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	COUNTER_INC(check_proc_setegid);

	return (0);
}

COUNTER_DECL(check_proc_setgroups);
static int
mac_test_check_proc_setgroups(struct ucred *cred, int ngroups,
	gid_t *gidset)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	COUNTER_INC(check_proc_setgroups);

	return (0);
}

COUNTER_DECL(check_proc_setreuid);
static int
mac_test_check_proc_setreuid(struct ucred *cred, uid_t ruid, uid_t euid)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	COUNTER_INC(check_proc_setreuid);

	return (0);
}

COUNTER_DECL(check_proc_setregid);
static int
mac_test_check_proc_setregid(struct ucred *cred, gid_t rgid, gid_t egid)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	COUNTER_INC(check_proc_setregid);

	return (0);
}

COUNTER_DECL(check_proc_setresuid);
static int
mac_test_check_proc_setresuid(struct ucred *cred, uid_t ruid, uid_t euid,
	uid_t suid)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	COUNTER_INC(check_proc_setresuid);

	return (0);
}

COUNTER_DECL(check_proc_setresgid);
static int
mac_test_check_proc_setresgid(struct ucred *cred, gid_t rgid, gid_t egid,
	gid_t sgid)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	COUNTER_INC(check_proc_setresgid);

	return (0);
}

COUNTER_DECL(check_proc_wait);
static int
mac_test_check_proc_wait(struct ucred *cred, struct proc *p)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(p->p_ucred->cr_label, MAGIC_CRED);
	COUNTER_INC(check_proc_wait);

	return (0);
}

COUNTER_DECL(check_socket_accept);
static int
mac_test_check_socket_accept(struct ucred *cred, struct socket *so,
    struct label *solabel)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(solabel, MAGIC_SOCKET);
	COUNTER_INC(check_socket_accept);

	return (0);
}

COUNTER_DECL(check_socket_bind);
static int
mac_test_check_socket_bind(struct ucred *cred, struct socket *so,
    struct label *solabel, struct sockaddr *sa)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(solabel, MAGIC_SOCKET);
	COUNTER_INC(check_socket_bind);

	return (0);
}

COUNTER_DECL(check_socket_connect);
static int
mac_test_check_socket_connect(struct ucred *cred, struct socket *so,
    struct label *solabel, struct sockaddr *sa)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(solabel, MAGIC_SOCKET);
	COUNTER_INC(check_socket_connect);

	return (0);
}

COUNTER_DECL(check_socket_deliver);
static int
mac_test_check_socket_deliver(struct socket *so, struct label *solabel,
    struct mbuf *m, struct label *mlabel)
{

	LABEL_CHECK(solabel, MAGIC_SOCKET);
	LABEL_CHECK(mlabel, MAGIC_MBUF);
	COUNTER_INC(check_socket_deliver);

	return (0);
}

COUNTER_DECL(check_socket_listen);
static int
mac_test_check_socket_listen(struct ucred *cred, struct socket *so,
    struct label *solabel)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(solabel, MAGIC_SOCKET);
	COUNTER_INC(check_socket_listen);

	return (0);
}

COUNTER_DECL(check_socket_poll);
static int
mac_test_check_socket_poll(struct ucred *cred, struct socket *so,
    struct label *solabel)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(solabel, MAGIC_SOCKET);
	COUNTER_INC(check_socket_poll);

	return (0);
}

COUNTER_DECL(check_socket_receive);
static int
mac_test_check_socket_receive(struct ucred *cred, struct socket *so,
    struct label *solabel)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(solabel, MAGIC_SOCKET);
	COUNTER_INC(check_socket_receive);

	return (0);
}

COUNTER_DECL(check_socket_relabel);
static int
mac_test_check_socket_relabel(struct ucred *cred, struct socket *so,
    struct label *solabel, struct label *newlabel)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(solabel, MAGIC_SOCKET);
	LABEL_CHECK(newlabel, MAGIC_SOCKET);
	COUNTER_INC(check_socket_relabel);

	return (0);
}

COUNTER_DECL(check_socket_send);
static int
mac_test_check_socket_send(struct ucred *cred, struct socket *so,
    struct label *solabel)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(solabel, MAGIC_SOCKET);
	COUNTER_INC(check_socket_send);

	return (0);
}

COUNTER_DECL(check_socket_stat);
static int
mac_test_check_socket_stat(struct ucred *cred, struct socket *so,
    struct label *solabel)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(solabel, MAGIC_SOCKET);
	COUNTER_INC(check_socket_stat);

	return (0);
}

COUNTER_DECL(check_socket_visible);
static int
mac_test_check_socket_visible(struct ucred *cred, struct socket *so,
    struct label *solabel)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(solabel, MAGIC_SOCKET);
	COUNTER_INC(check_socket_visible);

	return (0);
}

COUNTER_DECL(check_system_acct);
static int
mac_test_check_system_acct(struct ucred *cred, struct vnode *vp,
    struct label *vplabel)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(vplabel, MAGIC_VNODE);
	COUNTER_INC(check_system_acct);

	return (0);
}

COUNTER_DECL(check_system_audit);
static int
mac_test_check_system_audit(struct ucred *cred, void *record, int length)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	COUNTER_INC(check_system_audit);

	return (0);
}

COUNTER_DECL(check_system_auditctl);
static int
mac_test_check_system_auditctl(struct ucred *cred, struct vnode *vp,
    struct label *vplabel)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(vplabel, MAGIC_VNODE);
	COUNTER_INC(check_system_auditctl);

	return (0);
}

COUNTER_DECL(check_system_auditon);
static int
mac_test_check_system_auditon(struct ucred *cred, int cmd)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	COUNTER_INC(check_system_auditon);

	return (0);
}

COUNTER_DECL(check_system_reboot);
static int
mac_test_check_system_reboot(struct ucred *cred, int how)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	COUNTER_INC(check_system_reboot);

	return (0);
}

COUNTER_DECL(check_system_swapoff);
static int
mac_test_check_system_swapoff(struct ucred *cred, struct vnode *vp,
    struct label *vplabel)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(vplabel, MAGIC_VNODE);
	COUNTER_INC(check_system_swapoff);

	return (0);
}

COUNTER_DECL(check_system_swapon);
static int
mac_test_check_system_swapon(struct ucred *cred, struct vnode *vp,
    struct label *vplabel)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(vplabel, MAGIC_VNODE);
	COUNTER_INC(check_system_swapon);

	return (0);
}

COUNTER_DECL(check_system_sysctl);
static int
mac_test_check_system_sysctl(struct ucred *cred, struct sysctl_oid *oidp,
    void *arg1, int arg2, struct sysctl_req *req)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	COUNTER_INC(check_system_sysctl);

	return (0);
}

COUNTER_DECL(check_vnode_access);
static int
mac_test_check_vnode_access(struct ucred *cred, struct vnode *vp,
    struct label *vplabel, int acc_mode)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(vplabel, MAGIC_VNODE);
	COUNTER_INC(check_vnode_access);

	return (0);
}

COUNTER_DECL(check_vnode_chdir);
static int
mac_test_check_vnode_chdir(struct ucred *cred, struct vnode *dvp,
    struct label *dvplabel)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(dvplabel, MAGIC_VNODE);
	COUNTER_INC(check_vnode_chdir);

	return (0);
}

COUNTER_DECL(check_vnode_chroot);
static int
mac_test_check_vnode_chroot(struct ucred *cred, struct vnode *dvp,
    struct label *dvplabel)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(dvplabel, MAGIC_VNODE);
	COUNTER_INC(check_vnode_chroot);

	return (0);
}

COUNTER_DECL(check_vnode_create);
static int
mac_test_check_vnode_create(struct ucred *cred, struct vnode *dvp,
    struct label *dvplabel, struct componentname *cnp, struct vattr *vap)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(dvplabel, MAGIC_VNODE);
	COUNTER_INC(check_vnode_create);

	return (0);
}

COUNTER_DECL(check_vnode_deleteacl);
static int
mac_test_check_vnode_deleteacl(struct ucred *cred, struct vnode *vp,
    struct label *vplabel, acl_type_t type)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(vplabel, MAGIC_VNODE);
	COUNTER_INC(check_vnode_deleteacl);

	return (0);
}

COUNTER_DECL(check_vnode_deleteextattr);
static int
mac_test_check_vnode_deleteextattr(struct ucred *cred, struct vnode *vp,
    struct label *vplabel, int attrnamespace, const char *name)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(vplabel, MAGIC_VNODE);
	COUNTER_INC(check_vnode_deleteextattr);

	return (0);
}

COUNTER_DECL(check_vnode_exec);
static int
mac_test_check_vnode_exec(struct ucred *cred, struct vnode *vp,
    struct label *vplabel, struct image_params *imgp,
    struct label *execlabel)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(vplabel, MAGIC_VNODE);
	LABEL_CHECK(execlabel, MAGIC_CRED);
	COUNTER_INC(check_vnode_exec);

	return (0);
}

COUNTER_DECL(check_vnode_getacl);
static int
mac_test_check_vnode_getacl(struct ucred *cred, struct vnode *vp,
    struct label *vplabel, acl_type_t type)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(vplabel, MAGIC_VNODE);
	COUNTER_INC(check_vnode_getacl);

	return (0);
}

COUNTER_DECL(check_vnode_getextattr);
static int
mac_test_check_vnode_getextattr(struct ucred *cred, struct vnode *vp,
    struct label *vplabel, int attrnamespace, const char *name,
    struct uio *uio)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(vplabel, MAGIC_VNODE);
	COUNTER_INC(check_vnode_getextattr);

	return (0);
}

COUNTER_DECL(check_vnode_link);
static int
mac_test_check_vnode_link(struct ucred *cred, struct vnode *dvp,
    struct label *dvplabel, struct vnode *vp, struct label *vplabel,
    struct componentname *cnp)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(dvplabel, MAGIC_VNODE);
	LABEL_CHECK(vplabel, MAGIC_VNODE);
	COUNTER_INC(check_vnode_link);

	return (0);
}

COUNTER_DECL(check_vnode_listextattr);
static int
mac_test_check_vnode_listextattr(struct ucred *cred, struct vnode *vp,
    struct label *vplabel, int attrnamespace)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(vplabel, MAGIC_VNODE);
	COUNTER_INC(check_vnode_listextattr);

	return (0);
}

COUNTER_DECL(check_vnode_lookup);
static int
mac_test_check_vnode_lookup(struct ucred *cred, struct vnode *dvp,
    struct label *dvplabel, struct componentname *cnp)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(dvplabel, MAGIC_VNODE);
	COUNTER_INC(check_vnode_lookup);

	return (0);
}

COUNTER_DECL(check_vnode_mmap);
static int
mac_test_check_vnode_mmap(struct ucred *cred, struct vnode *vp,
    struct label *vplabel, int prot, int flags)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(vplabel, MAGIC_VNODE);
	COUNTER_INC(check_vnode_mmap);

	return (0);
}

COUNTER_DECL(check_vnode_open);
static int
mac_test_check_vnode_open(struct ucred *cred, struct vnode *vp,
    struct label *vplabel, int acc_mode)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(vplabel, MAGIC_VNODE);
	COUNTER_INC(check_vnode_open);

	return (0);
}

COUNTER_DECL(check_vnode_poll);
static int
mac_test_check_vnode_poll(struct ucred *active_cred, struct ucred *file_cred,
    struct vnode *vp, struct label *vplabel)
{

	LABEL_CHECK(active_cred->cr_label, MAGIC_CRED);
	if (file_cred != NULL)
		LABEL_CHECK(file_cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(vplabel, MAGIC_VNODE);
	COUNTER_INC(check_vnode_poll);

	return (0);
}

COUNTER_DECL(check_vnode_read);
static int
mac_test_check_vnode_read(struct ucred *active_cred, struct ucred *file_cred,
    struct vnode *vp, struct label *vplabel)
{

	LABEL_CHECK(active_cred->cr_label, MAGIC_CRED);
	if (file_cred != NULL)
		LABEL_CHECK(file_cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(vplabel, MAGIC_VNODE);
	COUNTER_INC(check_vnode_read);

	return (0);
}

COUNTER_DECL(check_vnode_readdir);
static int
mac_test_check_vnode_readdir(struct ucred *cred, struct vnode *dvp,
    struct label *dvplabel)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(dvplabel, MAGIC_VNODE);
	COUNTER_INC(check_vnode_readdir);

	return (0);
}

COUNTER_DECL(check_vnode_readlink);
static int
mac_test_check_vnode_readlink(struct ucred *cred, struct vnode *vp,
    struct label *vplabel)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(vplabel, MAGIC_VNODE);
	COUNTER_INC(check_vnode_readlink);

	return (0);
}

COUNTER_DECL(check_vnode_relabel);
static int
mac_test_check_vnode_relabel(struct ucred *cred, struct vnode *vp,
    struct label *vplabel, struct label *newlabel)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(vplabel, MAGIC_VNODE);
	LABEL_CHECK(newlabel, MAGIC_VNODE);
	COUNTER_INC(check_vnode_relabel);

	return (0);
}

COUNTER_DECL(check_vnode_rename_from);
static int
mac_test_check_vnode_rename_from(struct ucred *cred, struct vnode *dvp,
    struct label *dvplabel, struct vnode *vp, struct label *vplabel,
    struct componentname *cnp)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(dvplabel, MAGIC_VNODE);
	LABEL_CHECK(vplabel, MAGIC_VNODE);
	COUNTER_INC(check_vnode_rename_from);

	return (0);
}

COUNTER_DECL(check_vnode_rename_to);
static int
mac_test_check_vnode_rename_to(struct ucred *cred, struct vnode *dvp,
    struct label *dvplabel, struct vnode *vp, struct label *vplabel,
    int samedir, struct componentname *cnp)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(dvplabel, MAGIC_VNODE);
	LABEL_CHECK(vplabel, MAGIC_VNODE);
	COUNTER_INC(check_vnode_rename_to);

	return (0);
}

COUNTER_DECL(check_vnode_revoke);
static int
mac_test_check_vnode_revoke(struct ucred *cred, struct vnode *vp,
    struct label *vplabel)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(vplabel, MAGIC_VNODE);
	COUNTER_INC(check_vnode_revoke);

	return (0);
}

COUNTER_DECL(check_vnode_setacl);
static int
mac_test_check_vnode_setacl(struct ucred *cred, struct vnode *vp,
    struct label *vplabel, acl_type_t type, struct acl *acl)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(vplabel, MAGIC_VNODE);
	COUNTER_INC(check_vnode_setacl);

	return (0);
}

COUNTER_DECL(check_vnode_setextattr);
static int
mac_test_check_vnode_setextattr(struct ucred *cred, struct vnode *vp,
    struct label *vplabel, int attrnamespace, const char *name,
    struct uio *uio)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(vplabel, MAGIC_VNODE);
	COUNTER_INC(check_vnode_setextattr);

	return (0);
}

COUNTER_DECL(check_vnode_setflags);
static int
mac_test_check_vnode_setflags(struct ucred *cred, struct vnode *vp,
    struct label *vplabel, u_long flags)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(vplabel, MAGIC_VNODE);
	COUNTER_INC(check_vnode_setflags);

	return (0);
}

COUNTER_DECL(check_vnode_setmode);
static int
mac_test_check_vnode_setmode(struct ucred *cred, struct vnode *vp,
    struct label *vplabel, mode_t mode)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(vplabel, MAGIC_VNODE);
	COUNTER_INC(check_vnode_setmode);

	return (0);
}

COUNTER_DECL(check_vnode_setowner);
static int
mac_test_check_vnode_setowner(struct ucred *cred, struct vnode *vp,
    struct label *vplabel, uid_t uid, gid_t gid)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(vplabel, MAGIC_VNODE);
	COUNTER_INC(check_vnode_setowner);

	return (0);
}

COUNTER_DECL(check_vnode_setutimes);
static int
mac_test_check_vnode_setutimes(struct ucred *cred, struct vnode *vp,
    struct label *vplabel, struct timespec atime, struct timespec mtime)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(vplabel, MAGIC_VNODE);
	COUNTER_INC(check_vnode_setutimes);

	return (0);
}

COUNTER_DECL(check_vnode_stat);
static int
mac_test_check_vnode_stat(struct ucred *active_cred, struct ucred *file_cred,
    struct vnode *vp, struct label *vplabel)
{

	LABEL_CHECK(active_cred->cr_label, MAGIC_CRED);
	if (file_cred != NULL)
		LABEL_CHECK(file_cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(vplabel, MAGIC_VNODE);
	COUNTER_INC(check_vnode_stat);

	return (0);
}

COUNTER_DECL(check_vnode_unlink);
static int
mac_test_check_vnode_unlink(struct ucred *cred, struct vnode *dvp,
    struct label *dvplabel, struct vnode *vp, struct label *vplabel,
    struct componentname *cnp)
{

	LABEL_CHECK(cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(dvplabel, MAGIC_VNODE);
	LABEL_CHECK(vplabel, MAGIC_VNODE);
	COUNTER_INC(check_vnode_unlink);

	return (0);
}

COUNTER_DECL(check_vnode_write);
static int
mac_test_check_vnode_write(struct ucred *active_cred,
    struct ucred *file_cred, struct vnode *vp, struct label *vplabel)
{

	LABEL_CHECK(active_cred->cr_label, MAGIC_CRED);
	if (file_cred != NULL)
		LABEL_CHECK(file_cred->cr_label, MAGIC_CRED);
	LABEL_CHECK(vplabel, MAGIC_VNODE);
	COUNTER_INC(check_vnode_write);

	return (0);
}

static struct mac_policy_ops mac_test_ops =
{
	.mpo_init_bpfdesc_label = mac_test_init_bpfdesc_label,
	.mpo_init_cred_label = mac_test_init_cred_label,
	.mpo_init_devfs_label = mac_test_init_devfs_label,
	.mpo_init_ifnet_label = mac_test_init_ifnet_label,
	.mpo_init_sysv_msgmsg_label = mac_test_init_sysv_msgmsg_label,
	.mpo_init_sysv_msgqueue_label = mac_test_init_sysv_msgqueue_label,
	.mpo_init_sysv_sem_label = mac_test_init_sysv_sem_label,
	.mpo_init_sysv_shm_label = mac_test_init_sysv_shm_label,
	.mpo_init_inpcb_label = mac_test_init_inpcb_label,
	.mpo_init_ipq_label = mac_test_init_ipq_label,
	.mpo_init_mbuf_label = mac_test_init_mbuf_label,
	.mpo_init_mount_label = mac_test_init_mount_label,
	.mpo_init_pipe_label = mac_test_init_pipe_label,
	.mpo_init_posix_sem_label = mac_test_init_posix_sem_label,
	.mpo_init_proc_label = mac_test_init_proc_label,
	.mpo_init_socket_label = mac_test_init_socket_label,
	.mpo_init_socket_peer_label = mac_test_init_socket_peer_label,
	.mpo_init_vnode_label = mac_test_init_vnode_label,
	.mpo_destroy_bpfdesc_label = mac_test_destroy_bpfdesc_label,
	.mpo_destroy_cred_label = mac_test_destroy_cred_label,
	.mpo_destroy_devfs_label = mac_test_destroy_devfs_label,
	.mpo_destroy_ifnet_label = mac_test_destroy_ifnet_label,
	.mpo_destroy_sysv_msgmsg_label = mac_test_destroy_sysv_msgmsg_label,
	.mpo_destroy_sysv_msgqueue_label =
	    mac_test_destroy_sysv_msgqueue_label,
	.mpo_destroy_sysv_sem_label = mac_test_destroy_sysv_sem_label,
	.mpo_destroy_sysv_shm_label = mac_test_destroy_sysv_shm_label,
	.mpo_destroy_inpcb_label = mac_test_destroy_inpcb_label,
	.mpo_destroy_ipq_label = mac_test_destroy_ipq_label,
	.mpo_destroy_mbuf_label = mac_test_destroy_mbuf_label,
	.mpo_destroy_mount_label = mac_test_destroy_mount_label,
	.mpo_destroy_pipe_label = mac_test_destroy_pipe_label,
	.mpo_destroy_posix_sem_label = mac_test_destroy_posix_sem_label,
	.mpo_destroy_proc_label = mac_test_destroy_proc_label,
	.mpo_destroy_socket_label = mac_test_destroy_socket_label,
	.mpo_destroy_socket_peer_label = mac_test_destroy_socket_peer_label,
	.mpo_destroy_vnode_label = mac_test_destroy_vnode_label,
	.mpo_copy_cred_label = mac_test_copy_cred_label,
	.mpo_copy_ifnet_label = mac_test_copy_ifnet_label,
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
	.mpo_relabel_vnode = mac_test_relabel_vnode,
	.mpo_setlabel_vnode_extattr = mac_test_setlabel_vnode_extattr,
	.mpo_update_devfs = mac_test_update_devfs,
	.mpo_create_mbuf_from_socket = mac_test_create_mbuf_from_socket,
	.mpo_create_pipe = mac_test_create_pipe,
	.mpo_create_posix_sem = mac_test_create_posix_sem,
	.mpo_create_socket = mac_test_create_socket,
	.mpo_create_socket_from_socket = mac_test_create_socket_from_socket,
	.mpo_relabel_pipe = mac_test_relabel_pipe,
	.mpo_relabel_socket = mac_test_relabel_socket,
	.mpo_set_socket_peer_from_mbuf = mac_test_set_socket_peer_from_mbuf,
	.mpo_set_socket_peer_from_socket = mac_test_set_socket_peer_from_socket,
	.mpo_create_bpfdesc = mac_test_create_bpfdesc,
	.mpo_create_ifnet = mac_test_create_ifnet,
	.mpo_create_inpcb_from_socket = mac_test_create_inpcb_from_socket,
	.mpo_create_sysv_msgmsg = mac_test_create_sysv_msgmsg,
	.mpo_create_sysv_msgqueue = mac_test_create_sysv_msgqueue,
	.mpo_create_sysv_sem = mac_test_create_sysv_sem,
	.mpo_create_sysv_shm = mac_test_create_sysv_shm,
	.mpo_create_datagram_from_ipq = mac_test_create_datagram_from_ipq,
	.mpo_create_fragment = mac_test_create_fragment,
	.mpo_create_ipq = mac_test_create_ipq,
	.mpo_create_mbuf_from_inpcb = mac_test_create_mbuf_from_inpcb,
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
	.mpo_cleanup_sysv_msgmsg = mac_test_cleanup_sysv_msgmsg,
	.mpo_cleanup_sysv_msgqueue = mac_test_cleanup_sysv_msgqueue,
	.mpo_cleanup_sysv_sem = mac_test_cleanup_sysv_sem,
	.mpo_cleanup_sysv_shm = mac_test_cleanup_sysv_shm,
	.mpo_check_bpfdesc_receive = mac_test_check_bpfdesc_receive,
	.mpo_check_cred_relabel = mac_test_check_cred_relabel,
	.mpo_check_cred_visible = mac_test_check_cred_visible,
	.mpo_check_ifnet_relabel = mac_test_check_ifnet_relabel,
	.mpo_check_ifnet_transmit = mac_test_check_ifnet_transmit,
	.mpo_check_inpcb_deliver = mac_test_check_inpcb_deliver,
	.mpo_check_sysv_msgmsq = mac_test_check_sysv_msgmsq,
	.mpo_check_sysv_msgrcv = mac_test_check_sysv_msgrcv,
	.mpo_check_sysv_msgrmid = mac_test_check_sysv_msgrmid,
	.mpo_check_sysv_msqget = mac_test_check_sysv_msqget,
	.mpo_check_sysv_msqsnd = mac_test_check_sysv_msqsnd,
	.mpo_check_sysv_msqrcv = mac_test_check_sysv_msqrcv,
	.mpo_check_sysv_msqctl = mac_test_check_sysv_msqctl,
	.mpo_check_sysv_semctl = mac_test_check_sysv_semctl,
	.mpo_check_sysv_semget = mac_test_check_sysv_semget,
	.mpo_check_sysv_semop = mac_test_check_sysv_semop,
	.mpo_check_sysv_shmat = mac_test_check_sysv_shmat,
	.mpo_check_sysv_shmctl = mac_test_check_sysv_shmctl,
	.mpo_check_sysv_shmdt = mac_test_check_sysv_shmdt,
	.mpo_check_sysv_shmget = mac_test_check_sysv_shmget,
	.mpo_check_kenv_dump = mac_test_check_kenv_dump,
	.mpo_check_kenv_get = mac_test_check_kenv_get,
	.mpo_check_kenv_set = mac_test_check_kenv_set,
	.mpo_check_kenv_unset = mac_test_check_kenv_unset,
	.mpo_check_kld_load = mac_test_check_kld_load,
	.mpo_check_kld_stat = mac_test_check_kld_stat,
	.mpo_check_mount_stat = mac_test_check_mount_stat,
	.mpo_check_pipe_ioctl = mac_test_check_pipe_ioctl,
	.mpo_check_pipe_poll = mac_test_check_pipe_poll,
	.mpo_check_pipe_read = mac_test_check_pipe_read,
	.mpo_check_pipe_relabel = mac_test_check_pipe_relabel,
	.mpo_check_pipe_stat = mac_test_check_pipe_stat,
	.mpo_check_pipe_write = mac_test_check_pipe_write,
	.mpo_check_posix_sem_destroy = mac_test_check_posix_sem,
	.mpo_check_posix_sem_getvalue = mac_test_check_posix_sem,
	.mpo_check_posix_sem_open = mac_test_check_posix_sem,
	.mpo_check_posix_sem_post = mac_test_check_posix_sem,
	.mpo_check_posix_sem_unlink = mac_test_check_posix_sem,
	.mpo_check_posix_sem_wait = mac_test_check_posix_sem,
	.mpo_check_proc_debug = mac_test_check_proc_debug,
	.mpo_check_proc_sched = mac_test_check_proc_sched,
	.mpo_check_proc_setaudit = mac_test_check_proc_setaudit,
	.mpo_check_proc_setaudit_addr = mac_test_check_proc_setaudit_addr,
	.mpo_check_proc_setauid = mac_test_check_proc_setauid,
	.mpo_check_proc_setuid = mac_test_check_proc_setuid,
	.mpo_check_proc_seteuid = mac_test_check_proc_seteuid,
	.mpo_check_proc_setgid = mac_test_check_proc_setgid,
	.mpo_check_proc_setegid = mac_test_check_proc_setegid,
	.mpo_check_proc_setgroups = mac_test_check_proc_setgroups,
	.mpo_check_proc_setreuid = mac_test_check_proc_setreuid,
	.mpo_check_proc_setregid = mac_test_check_proc_setregid,
	.mpo_check_proc_setresuid = mac_test_check_proc_setresuid,
	.mpo_check_proc_setresgid = mac_test_check_proc_setresgid,
	.mpo_check_proc_signal = mac_test_check_proc_signal,
	.mpo_check_proc_wait = mac_test_check_proc_wait,
	.mpo_check_socket_accept = mac_test_check_socket_accept,
	.mpo_check_socket_bind = mac_test_check_socket_bind,
	.mpo_check_socket_connect = mac_test_check_socket_connect,
	.mpo_check_socket_deliver = mac_test_check_socket_deliver,
	.mpo_check_socket_listen = mac_test_check_socket_listen,
	.mpo_check_socket_poll = mac_test_check_socket_poll,
	.mpo_check_socket_receive = mac_test_check_socket_receive,
	.mpo_check_socket_relabel = mac_test_check_socket_relabel,
	.mpo_check_socket_send = mac_test_check_socket_send,
	.mpo_check_socket_stat = mac_test_check_socket_stat,
	.mpo_check_socket_visible = mac_test_check_socket_visible,
	.mpo_check_system_acct = mac_test_check_system_acct,
	.mpo_check_system_audit = mac_test_check_system_audit,
	.mpo_check_system_auditctl = mac_test_check_system_auditctl,
	.mpo_check_system_auditon = mac_test_check_system_auditon,
	.mpo_check_system_reboot = mac_test_check_system_reboot,
	.mpo_check_system_swapoff = mac_test_check_system_swapoff,
	.mpo_check_system_swapon = mac_test_check_system_swapon,
	.mpo_check_system_sysctl = mac_test_check_system_sysctl,
	.mpo_check_vnode_access = mac_test_check_vnode_access,
	.mpo_check_vnode_chdir = mac_test_check_vnode_chdir,
	.mpo_check_vnode_chroot = mac_test_check_vnode_chroot,
	.mpo_check_vnode_create = mac_test_check_vnode_create,
	.mpo_check_vnode_deleteacl = mac_test_check_vnode_deleteacl,
	.mpo_check_vnode_deleteextattr = mac_test_check_vnode_deleteextattr,
	.mpo_check_vnode_exec = mac_test_check_vnode_exec,
	.mpo_check_vnode_getacl = mac_test_check_vnode_getacl,
	.mpo_check_vnode_getextattr = mac_test_check_vnode_getextattr,
	.mpo_check_vnode_link = mac_test_check_vnode_link,
	.mpo_check_vnode_listextattr = mac_test_check_vnode_listextattr,
	.mpo_check_vnode_lookup = mac_test_check_vnode_lookup,
	.mpo_check_vnode_mmap = mac_test_check_vnode_mmap,
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
	.mpo_check_vnode_unlink = mac_test_check_vnode_unlink,
	.mpo_check_vnode_write = mac_test_check_vnode_write,
};

MAC_POLICY_SET(&mac_test_ops, mac_test, "TrustedBSD MAC/Test",
    MPC_LOADTIME_FLAG_UNLOADOK | MPC_LOADTIME_FLAG_LABELMBUFS, &test_slot);
