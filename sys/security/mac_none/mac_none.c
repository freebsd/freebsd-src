/*-
 * Copyright (c) 1999, 2000, 2001, 2002 Robert N. M. Watson
 * Copyright (c) 2001, 2002 Networks Associates Technology, Inc.
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
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/sysproto.h>
#include <sys/sysent.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/pipe.h>
#include <sys/sysctl.h>

#include <fs/devfs/devfs.h>

#include <net/bpfdesc.h>
#include <net/if.h>
#include <net/if_types.h>
#include <net/if_var.h>

#include <netinet/in.h>
#include <netinet/ip_var.h>

#include <vm/vm.h>

#include <sys/mac_policy.h>

SYSCTL_DECL(_security_mac);

SYSCTL_NODE(_security_mac, OID_AUTO, none, CTLFLAG_RW, 0,
    "TrustedBSD mac_none policy controls");

static int	mac_none_enabled = 0;
SYSCTL_INT(_security_mac_none, OID_AUTO, enabled, CTLFLAG_RW,
    &mac_none_enabled, 0, "Enforce none policy");

/*
 * Policy module operations.
 */
static void
mac_none_destroy(struct mac_policy_conf *conf)
{

}

static void
mac_none_init(struct mac_policy_conf *conf)
{

}

static int
mac_none_syscall(struct thread *td, int call, void *arg)
{

	return (0);
}

/*
 * Label operations.
 */
static void
mac_none_init_label(struct label *label)
{

}

static int
mac_none_init_label_waitcheck(struct label *label, int flag)
{

	return (0);
}

static void
mac_none_destroy_label(struct label *label)
{

}

static int
mac_none_externalize_label(struct label *label, char *element_name,
    char *element_data, size_t size, size_t *len, int *claimed)
{

	return (0);
}

static int
mac_none_internalize_label(struct label *label, char *element_name,
    char *element_data, int *claimed)
{

	return (0);
}

/*
 * Labeling event operations: file system objects, and things that look
 * a lot like file system objects.
 */
static void
mac_none_associate_vnode_devfs(struct mount *mp, struct label *fslabel,
    struct devfs_dirent *de, struct label *delabel, struct vnode *vp,
    struct label *vlabel)
{

}

static int
mac_none_associate_vnode_extattr(struct mount *mp, struct label *fslabel,
    struct vnode *vp, struct label *vlabel)
{

	return (0);
}

static void
mac_none_associate_vnode_singlelabel(struct mount *mp,
    struct label *fslabel, struct vnode *vp, struct label *vlabel)
{

}

static void
mac_none_create_devfs_device(struct mount *mp, dev_t dev,
    struct devfs_dirent *devfs_dirent, struct label *label)
{

}

static void
mac_none_create_devfs_directory(struct mount *mp, char *dirname,
    int dirnamelen, struct devfs_dirent *devfs_dirent, struct label *label)
{

}

static void
mac_none_create_devfs_symlink(struct ucred *cred, struct mount *mp,
    struct devfs_dirent *dd, struct label *ddlabel, struct devfs_dirent *de,
    struct label *delabel)
{

}

static int
mac_none_create_vnode_extattr(struct ucred *cred, struct mount *mp,
    struct label *fslabel, struct vnode *dvp, struct label *dlabel,
    struct vnode *vp, struct label *vlabel, struct componentname *cnp)
{

	return (0);
}

static void
mac_none_create_mount(struct ucred *cred, struct mount *mp,
    struct label *mntlabel, struct label *fslabel)
{

}

static void
mac_none_create_root_mount(struct ucred *cred, struct mount *mp,
    struct label *mntlabel, struct label *fslabel)
{

}

static void
mac_none_relabel_vnode(struct ucred *cred, struct vnode *vp,
    struct label *vnodelabel, struct label *label)
{

}

static int
mac_none_setlabel_vnode_extattr(struct ucred *cred, struct vnode *vp,
    struct label *vlabel, struct label *intlabel)
{

	return (0);
}

static void
mac_none_update_devfsdirent(struct mount *mp,
    struct devfs_dirent *devfs_dirent, struct label *direntlabel,
    struct vnode *vp, struct label *vnodelabel)
{

}

/*
 * Labeling event operations: IPC object.
 */
static void
mac_none_create_mbuf_from_socket(struct socket *so, struct label *socketlabel,
    struct mbuf *m, struct label *mbuflabel)
{

}

static void
mac_none_create_socket(struct ucred *cred, struct socket *socket,
    struct label *socketlabel)
{

}

static void
mac_none_create_pipe(struct ucred *cred, struct pipe *pipe,
    struct label *pipelabel)
{

}

static void
mac_none_create_socket_from_socket(struct socket *oldsocket,
    struct label *oldsocketlabel, struct socket *newsocket,
    struct label *newsocketlabel)
{

}

static void
mac_none_relabel_socket(struct ucred *cred, struct socket *socket,
    struct label *socketlabel, struct label *newlabel)
{

}

static void
mac_none_relabel_pipe(struct ucred *cred, struct pipe *pipe,
    struct label *pipelabel, struct label *newlabel)
{

}

static void
mac_none_set_socket_peer_from_mbuf(struct mbuf *mbuf, struct label *mbuflabel,
    struct socket *socket, struct label *socketpeerlabel)
{

}

static void
mac_none_set_socket_peer_from_socket(struct socket *oldsocket,
    struct label *oldsocketlabel, struct socket *newsocket,
    struct label *newsocketpeerlabel)
{

}

/*
 * Labeling event operations: network objects.
 */
static void
mac_none_create_bpfdesc(struct ucred *cred, struct bpf_d *bpf_d,
    struct label *bpflabel)
{

}

static void
mac_none_create_datagram_from_ipq(struct ipq *ipq, struct label *ipqlabel,
    struct mbuf *datagram, struct label *datagramlabel)
{

}

static void
mac_none_create_fragment(struct mbuf *datagram, struct label *datagramlabel,
    struct mbuf *fragment, struct label *fragmentlabel)
{

}

static void
mac_none_create_ifnet(struct ifnet *ifnet, struct label *ifnetlabel)
{

}

static void
mac_none_create_ipq(struct mbuf *fragment, struct label *fragmentlabel,
    struct ipq *ipq, struct label *ipqlabel)
{

}

static void
mac_none_create_mbuf_from_mbuf(struct mbuf *oldmbuf,
    struct label *oldmbuflabel, struct mbuf *newmbuf,
    struct label *newmbuflabel)
{

}

static void
mac_none_create_mbuf_linklayer(struct ifnet *ifnet, struct label *ifnetlabel,
    struct mbuf *mbuf, struct label *mbuflabel)
{

}

static void
mac_none_create_mbuf_from_bpfdesc(struct bpf_d *bpf_d, struct label *bpflabel,
    struct mbuf *mbuf, struct label *mbuflabel)
{

}

static void
mac_none_create_mbuf_from_ifnet(struct ifnet *ifnet, struct label *ifnetlabel,
    struct mbuf *m, struct label *mbuflabel)
{

}

static void
mac_none_create_mbuf_multicast_encap(struct mbuf *oldmbuf,
    struct label *oldmbuflabel, struct ifnet *ifnet, struct label *ifnetlabel,
    struct mbuf *newmbuf, struct label *newmbuflabel)
{

}

static void
mac_none_create_mbuf_netlayer(struct mbuf *oldmbuf,
    struct label *oldmbuflabel, struct mbuf *newmbuf, struct label *newmbuflabel)
{

}

static int
mac_none_fragment_match(struct mbuf *fragment, struct label *fragmentlabel,
    struct ipq *ipq, struct label *ipqlabel)
{

	return (1);
}

static void
mac_none_relabel_ifnet(struct ucred *cred, struct ifnet *ifnet,
    struct label *ifnetlabel, struct label *newlabel)
{

}

static void
mac_none_update_ipq(struct mbuf *fragment, struct label *fragmentlabel,
    struct ipq *ipq, struct label *ipqlabel)
{

}

/*
 * Labeling event operations: processes.
 */
static void
mac_none_create_cred(struct ucred *cred_parent, struct ucred *cred_child)
{

}

static void
mac_none_execve_transition(struct ucred *old, struct ucred *new,
    struct vnode *vp, struct label *vnodelabel,
    struct label *interpvnodelabel, struct image_params *imgp,
    struct label *execlabel)
{

}

static int
mac_none_execve_will_transition(struct ucred *old, struct vnode *vp,
    struct label *vnodelabel, struct label *interpvnodelabel,
    struct image_params *imgp, struct label *execlabel)
{

	return (0);
}

static void
mac_none_create_proc0(struct ucred *cred)
{

}

static void
mac_none_create_proc1(struct ucred *cred)
{

}

static void
mac_none_relabel_cred(struct ucred *cred, struct label *newlabel)
{

}

/*
 * Access control checks.
 */
static int
mac_none_check_bpfdesc_receive(struct bpf_d *bpf_d, struct label *bpflabel,
    struct ifnet *ifnet, struct label *ifnet_label)
{

        return (0);
}

static int
mac_none_check_cred_relabel(struct ucred *cred, struct label *newlabel)
{

	return (0);
}

static int
mac_none_check_cred_visible(struct ucred *u1, struct ucred *u2)
{

	return (0);
}

static int
mac_none_check_ifnet_relabel(struct ucred *cred, struct ifnet *ifnet,
    struct label *ifnetlabel, struct label *newlabel)
{

	return (0);
}

static int
mac_none_check_ifnet_transmit(struct ifnet *ifnet, struct label *ifnetlabel,
    struct mbuf *m, struct label *mbuflabel)
{

	return (0);
}

static int
mac_none_check_mount_stat(struct ucred *cred, struct mount *mp,
    struct label *mntlabel)
{

	return (0);
}

static int
mac_none_check_pipe_ioctl(struct ucred *cred, struct pipe *pipe,
    struct label *pipelabel, unsigned long cmd, void /* caddr_t */ *data)
{

	return (0);
}

static int
mac_none_check_pipe_poll(struct ucred *cred, struct pipe *pipe,
    struct label *pipelabel)
{

	return (0);
}

static int
mac_none_check_pipe_read(struct ucred *cred, struct pipe *pipe,
    struct label *pipelabel)
{

	return (0);
}

static int
mac_none_check_pipe_relabel(struct ucred *cred, struct pipe *pipe,
    struct label *pipelabel, struct label *newlabel)
{

	return (0);
}

static int
mac_none_check_pipe_stat(struct ucred *cred, struct pipe *pipe,
    struct label *pipelabel)
{

	return (0);
}

static int
mac_none_check_pipe_write(struct ucred *cred, struct pipe *pipe,
    struct label *pipelabel)
{

	return (0);
}

static int
mac_none_check_proc_debug(struct ucred *cred, struct proc *proc)
{

	return (0);
}

static int
mac_none_check_proc_sched(struct ucred *cred, struct proc *proc)
{

	return (0);
}

static int
mac_none_check_proc_signal(struct ucred *cred, struct proc *proc, int signum)
{

	return (0);
}

static int
mac_none_check_socket_bind(struct ucred *cred, struct socket *socket,
    struct label *socketlabel, struct sockaddr *sockaddr)
{

	return (0);
}

static int
mac_none_check_socket_connect(struct ucred *cred, struct socket *socket,
    struct label *socketlabel, struct sockaddr *sockaddr)
{

	return (0);
}

static int
mac_none_check_socket_deliver(struct socket *so, struct label *socketlabel,
    struct mbuf *m, struct label *mbuflabel)
{

	return (0);
}

static int
mac_none_check_socket_listen(struct ucred *cred, struct socket *so,
    struct label *socketlabel)
{

	return (0);
}

static int
mac_none_check_socket_relabel(struct ucred *cred, struct socket *socket,
    struct label *socketlabel, struct label *newlabel)
{

	return (0);
}

static int
mac_none_check_socket_visible(struct ucred *cred, struct socket *socket,
   struct label *socketlabel)
{

	return (0);
}

static int
mac_none_check_system_reboot(struct ucred *cred, int how)
{

	return (0);
}

static int
mac_none_check_system_swapon(struct ucred *cred, struct vnode *vp,
    struct label *label)
{

	return (0);
}

static int
mac_none_check_system_sysctl(struct ucred *cred, int *name, u_int namelen,
    void *old, size_t *oldlenp, int inkernel, void *new, size_t newlen)
{

	return (0);
}

static int
mac_none_check_vnode_access(struct ucred *cred, struct vnode *vp,
    struct label *label, int acc_mode)
{

	return (0);
}

static int
mac_none_check_vnode_chdir(struct ucred *cred, struct vnode *dvp,
    struct label *dlabel)
{

	return (0);
}

static int
mac_none_check_vnode_chroot(struct ucred *cred, struct vnode *dvp,
    struct label *dlabel)
{

	return (0);
}

static int
mac_none_check_vnode_create(struct ucred *cred, struct vnode *dvp,
    struct label *dlabel, struct componentname *cnp, struct vattr *vap)
{

	return (0);
}

static int
mac_none_check_vnode_delete(struct ucred *cred, struct vnode *dvp,
    struct label *dlabel, struct vnode *vp, struct label *label,
    struct componentname *cnp)
{

	return (0);
}

static int
mac_none_check_vnode_deleteacl(struct ucred *cred, struct vnode *vp,
    struct label *label, acl_type_t type)
{

	return (0);
}

static int
mac_none_check_vnode_exec(struct ucred *cred, struct vnode *vp,
    struct label *label, struct image_params *imgp,
    struct label *execlabel)
{

	return (0);
}

static int
mac_none_check_vnode_getacl(struct ucred *cred, struct vnode *vp,
    struct label *label, acl_type_t type)
{

	return (0);
}

static int
mac_none_check_vnode_getextattr(struct ucred *cred, struct vnode *vp,
    struct label *label, int attrnamespace, const char *name, struct uio *uio)
{

	return (0);
}

static int
mac_none_check_vnode_link(struct ucred *cred, struct vnode *dvp,
    struct label *dlabel, struct vnode *vp, struct label *label,
    struct componentname *cnp)
{

	return (0);
}

static int
mac_none_check_vnode_lookup(struct ucred *cred, struct vnode *dvp,
    struct label *dlabel, struct componentname *cnp)
{

	return (0);
}

static int
mac_none_check_vnode_mmap(struct ucred *cred, struct vnode *vp,
    struct label *label, int prot)
{

	return (0);
}

static int
mac_none_check_vnode_mprotect(struct ucred *cred, struct vnode *vp,
    struct label *label, int prot)
{

	return (0);
}

static int
mac_none_check_vnode_open(struct ucred *cred, struct vnode *vp,
    struct label *filelabel, int acc_mode)
{

	return (0);
}

static int
mac_none_check_vnode_poll(struct ucred *active_cred, struct ucred *file_cred,
    struct vnode *vp, struct label *label)
{

	return (0);
}

static int
mac_none_check_vnode_read(struct ucred *active_cred, struct ucred *file_cred,
    struct vnode *vp, struct label *label)
{

	return (0);
}

static int
mac_none_check_vnode_readdir(struct ucred *cred, struct vnode *vp,
    struct label *dlabel)
{

	return (0);
}

static int
mac_none_check_vnode_readlink(struct ucred *cred, struct vnode *vp,
    struct label *vnodelabel)
{

	return (0);
}

static int
mac_none_check_vnode_relabel(struct ucred *cred, struct vnode *vp,
    struct label *vnodelabel, struct label *newlabel)
{

	return (0);
}

static int
mac_none_check_vnode_rename_from(struct ucred *cred, struct vnode *dvp,
    struct label *dlabel, struct vnode *vp, struct label *label,
    struct componentname *cnp)
{

	return (0);
}

static int
mac_none_check_vnode_rename_to(struct ucred *cred, struct vnode *dvp,
    struct label *dlabel, struct vnode *vp, struct label *label, int samedir,
    struct componentname *cnp)
{

	return (0);
}

static int
mac_none_check_vnode_revoke(struct ucred *cred, struct vnode *vp,
    struct label *label)
{

	return (0);
}

static int
mac_none_check_vnode_setacl(struct ucred *cred, struct vnode *vp,
    struct label *label, acl_type_t type, struct acl *acl)
{

	return (0);
}

static int
mac_none_check_vnode_setextattr(struct ucred *cred, struct vnode *vp,
    struct label *label, int attrnamespace, const char *name, struct uio *uio)
{

	return (0);
}

static int
mac_none_check_vnode_setflags(struct ucred *cred, struct vnode *vp,
    struct label *label, u_long flags)
{

	return (0);
}

static int
mac_none_check_vnode_setmode(struct ucred *cred, struct vnode *vp,
    struct label *label, mode_t mode)
{

	return (0);
}

static int
mac_none_check_vnode_setowner(struct ucred *cred, struct vnode *vp,
    struct label *label, uid_t uid, gid_t gid)
{

	return (0);
}

static int
mac_none_check_vnode_setutimes(struct ucred *cred, struct vnode *vp,
    struct label *label, struct timespec atime, struct timespec mtime)
{

	return (0);
}

static int
mac_none_check_vnode_stat(struct ucred *active_cred, struct ucred *file_cred,
    struct vnode *vp, struct label *label)
{

	return (0);
}

static int
mac_none_check_vnode_write(struct ucred *active_cred,
    struct ucred *file_cred, struct vnode *vp, struct label *label)
{

	return (0);
}

static struct mac_policy_ops mac_none_ops =
{
	.mpo_destroy = mac_none_destroy,
	.mpo_init = mac_none_init,
	.mpo_syscall = mac_none_syscall,
	.mpo_init_bpfdesc_label = mac_none_init_label,
	.mpo_init_cred_label = mac_none_init_label,
	.mpo_init_devfsdirent_label = mac_none_init_label,
	.mpo_init_ifnet_label = mac_none_init_label,
	.mpo_init_ipq_label = mac_none_init_label,
	.mpo_init_mbuf_label = mac_none_init_label_waitcheck,
	.mpo_init_mount_label = mac_none_init_label,
	.mpo_init_mount_fs_label = mac_none_init_label,
	.mpo_init_pipe_label = mac_none_init_label,
	.mpo_init_socket_label = mac_none_init_label_waitcheck,
	.mpo_init_socket_peer_label = mac_none_init_label_waitcheck,
	.mpo_init_vnode_label = mac_none_init_label,
	.mpo_destroy_bpfdesc_label = mac_none_destroy_label,
	.mpo_destroy_cred_label = mac_none_destroy_label,
	.mpo_destroy_devfsdirent_label = mac_none_destroy_label,
	.mpo_destroy_ifnet_label = mac_none_destroy_label,
	.mpo_destroy_ipq_label = mac_none_destroy_label,
	.mpo_destroy_mbuf_label = mac_none_destroy_label,
	.mpo_destroy_mount_label = mac_none_destroy_label,
	.mpo_destroy_mount_fs_label = mac_none_destroy_label,
	.mpo_destroy_pipe_label = mac_none_destroy_label,
	.mpo_destroy_socket_label = mac_none_destroy_label,
	.mpo_destroy_socket_peer_label = mac_none_destroy_label,
	.mpo_destroy_vnode_label = mac_none_destroy_label,
	.mpo_externalize_cred_label = mac_none_externalize_label,
	.mpo_externalize_ifnet_label = mac_none_externalize_label,
	.mpo_externalize_pipe_label = mac_none_externalize_label,
	.mpo_externalize_socket_label = mac_none_externalize_label,
	.mpo_externalize_socket_peer_label = mac_none_externalize_label,
	.mpo_externalize_vnode_label = mac_none_externalize_label,
	.mpo_internalize_cred_label = mac_none_internalize_label,
	.mpo_internalize_ifnet_label = mac_none_internalize_label,
	.mpo_internalize_pipe_label = mac_none_internalize_label,
	.mpo_internalize_socket_label = mac_none_internalize_label,
	.mpo_internalize_vnode_label = mac_none_internalize_label,
	.mpo_associate_vnode_devfs = mac_none_associate_vnode_devfs,
	.mpo_associate_vnode_extattr = mac_none_associate_vnode_extattr,
	.mpo_associate_vnode_singlelabel = mac_none_associate_vnode_singlelabel,
	.mpo_create_devfs_device = mac_none_create_devfs_device,
	.mpo_create_devfs_directory = mac_none_create_devfs_directory,
	.mpo_create_devfs_symlink = mac_none_create_devfs_symlink,
	.mpo_create_vnode_extattr = mac_none_create_vnode_extattr,
	.mpo_create_mount = mac_none_create_mount,
	.mpo_create_root_mount = mac_none_create_root_mount,
	.mpo_relabel_vnode = mac_none_relabel_vnode,
	.mpo_setlabel_vnode_extattr = mac_none_setlabel_vnode_extattr,
	.mpo_update_devfsdirent = mac_none_update_devfsdirent,
	.mpo_create_mbuf_from_socket = mac_none_create_mbuf_from_socket,
	.mpo_create_pipe = mac_none_create_pipe,
	.mpo_create_socket = mac_none_create_socket,
	.mpo_create_socket_from_socket = mac_none_create_socket_from_socket,
	.mpo_relabel_pipe = mac_none_relabel_pipe,
	.mpo_relabel_socket = mac_none_relabel_socket,
	.mpo_set_socket_peer_from_mbuf = mac_none_set_socket_peer_from_mbuf,
	.mpo_set_socket_peer_from_socket = mac_none_set_socket_peer_from_socket,
	.mpo_create_bpfdesc = mac_none_create_bpfdesc,
	.mpo_create_ifnet = mac_none_create_ifnet,
	.mpo_create_ipq = mac_none_create_ipq,
	.mpo_create_datagram_from_ipq = mac_none_create_datagram_from_ipq,
	.mpo_create_fragment = mac_none_create_fragment,
	.mpo_create_ipq = mac_none_create_ipq,
	.mpo_create_mbuf_from_mbuf = mac_none_create_mbuf_from_mbuf,
	.mpo_create_mbuf_linklayer = mac_none_create_mbuf_linklayer,
	.mpo_create_mbuf_from_bpfdesc = mac_none_create_mbuf_from_bpfdesc,
	.mpo_create_mbuf_from_ifnet = mac_none_create_mbuf_from_ifnet,
	.mpo_create_mbuf_multicast_encap = mac_none_create_mbuf_multicast_encap,
	.mpo_create_mbuf_netlayer = mac_none_create_mbuf_netlayer,
	.mpo_fragment_match = mac_none_fragment_match,
	.mpo_relabel_ifnet = mac_none_relabel_ifnet,
	.mpo_update_ipq = mac_none_update_ipq,
	.mpo_create_cred = mac_none_create_cred,
	.mpo_execve_transition = mac_none_execve_transition,
	.mpo_execve_will_transition = mac_none_execve_will_transition,
	.mpo_create_proc0 = mac_none_create_proc0,
	.mpo_create_proc1 = mac_none_create_proc1,
	.mpo_relabel_cred = mac_none_relabel_cred,
	.mpo_check_bpfdesc_receive = mac_none_check_bpfdesc_receive,
	.mpo_check_cred_relabel = mac_none_check_cred_relabel,
	.mpo_check_cred_visible = mac_none_check_cred_visible,
	.mpo_check_ifnet_relabel = mac_none_check_ifnet_relabel,
	.mpo_check_ifnet_transmit = mac_none_check_ifnet_transmit,
	.mpo_check_mount_stat = mac_none_check_mount_stat,
	.mpo_check_pipe_ioctl = mac_none_check_pipe_ioctl,
	.mpo_check_pipe_poll = mac_none_check_pipe_poll,
	.mpo_check_pipe_read = mac_none_check_pipe_read,
	.mpo_check_pipe_relabel = mac_none_check_pipe_relabel,
	.mpo_check_pipe_stat = mac_none_check_pipe_stat,
	.mpo_check_pipe_write = mac_none_check_pipe_write,
	.mpo_check_proc_debug = mac_none_check_proc_debug,
	.mpo_check_proc_sched = mac_none_check_proc_sched,
	.mpo_check_proc_signal = mac_none_check_proc_signal,
	.mpo_check_socket_bind = mac_none_check_socket_bind,
	.mpo_check_socket_connect = mac_none_check_socket_connect,
	.mpo_check_socket_deliver = mac_none_check_socket_deliver,
	.mpo_check_socket_listen = mac_none_check_socket_listen,
	.mpo_check_socket_relabel = mac_none_check_socket_relabel,
	.mpo_check_socket_visible = mac_none_check_socket_visible,
	.mpo_check_system_reboot = mac_none_check_system_reboot,
	.mpo_check_system_swapon = mac_none_check_system_swapon,
	.mpo_check_system_sysctl = mac_none_check_system_sysctl,
	.mpo_check_vnode_access = mac_none_check_vnode_access,
	.mpo_check_vnode_chdir = mac_none_check_vnode_chdir,
	.mpo_check_vnode_chroot = mac_none_check_vnode_chroot,
	.mpo_check_vnode_create = mac_none_check_vnode_create,
	.mpo_check_vnode_delete = mac_none_check_vnode_delete,
	.mpo_check_vnode_deleteacl = mac_none_check_vnode_deleteacl,
	.mpo_check_vnode_exec = mac_none_check_vnode_exec,
	.mpo_check_vnode_getacl = mac_none_check_vnode_getacl,
	.mpo_check_vnode_getextattr = mac_none_check_vnode_getextattr,
	.mpo_check_vnode_link = mac_none_check_vnode_link,
	.mpo_check_vnode_lookup = mac_none_check_vnode_lookup,
	.mpo_check_vnode_mmap = mac_none_check_vnode_mmap,
	.mpo_check_vnode_mprotect = mac_none_check_vnode_mprotect,
	.mpo_check_vnode_open = mac_none_check_vnode_open,
	.mpo_check_vnode_poll = mac_none_check_vnode_poll,
	.mpo_check_vnode_read = mac_none_check_vnode_read,
	.mpo_check_vnode_readdir = mac_none_check_vnode_readdir,
	.mpo_check_vnode_readlink = mac_none_check_vnode_readlink,
	.mpo_check_vnode_relabel = mac_none_check_vnode_relabel,
	.mpo_check_vnode_rename_from = mac_none_check_vnode_rename_from,
	.mpo_check_vnode_rename_to = mac_none_check_vnode_rename_to,
	.mpo_check_vnode_revoke = mac_none_check_vnode_revoke,
	.mpo_check_vnode_setacl = mac_none_check_vnode_setacl,
	.mpo_check_vnode_setextattr = mac_none_check_vnode_setextattr,
	.mpo_check_vnode_setflags = mac_none_check_vnode_setflags,
	.mpo_check_vnode_setmode = mac_none_check_vnode_setmode,
	.mpo_check_vnode_setowner = mac_none_check_vnode_setowner,
	.mpo_check_vnode_setutimes = mac_none_check_vnode_setutimes,
	.mpo_check_vnode_stat = mac_none_check_vnode_stat,
	.mpo_check_vnode_write = mac_none_check_vnode_write,
};

MAC_POLICY_SET(&mac_none_ops, trustedbsd_mac_none, "TrustedBSD MAC/None",
    MPC_LOADTIME_FLAG_UNLOADOK, NULL);
