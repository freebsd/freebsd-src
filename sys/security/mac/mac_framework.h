/*-
 * Copyright (c) 1999, 2000, 2001, 2002 Robert N. M. Watson
 * Copyright (c) 2001, 2002 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed by Robert Watson for the TrustedBSD Project.
 *
 * This software was developed for the FreeBSD Project in part by NAI Labs,
 * the Security Research Division of Network Associates, Inc. under
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
 * 3. The names of the authors may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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
 * Userland/kernel interface for Mandatory Access Control.
 *
 * The POSIX.1e implementation page may be reached at:
 * http://www.trustedbsd.org/
 */
#ifndef _SYS_MAC_H
#define	_SYS_MAC_H

#include <sys/_label.h>

#ifndef _POSIX_MAC
#define	_POSIX_MAC
#endif

/*
 * XXXMAC: The single MAC extended attribute will be deprecated once
 * compound EA writes on a single target file can be performed cleanly
 * with UFS2.
 */
#define	FREEBSD_MAC_EXTATTR_NAME	"freebsd.mac"
#define	FREEBSD_MAC_EXTATTR_NAMESPACE	EXTATTR_NAMESPACE_SYSTEM

/*
 * MAC framework-related constants and limits.
 */
#define	MAC_MAX_POLICY_NAME	32

/*
 * XXXMAC: Per-policy structures will be moved from mac.h to per-policy
 * include files once the revised user interface is available.
 */

/*
 * Structures and constants associated with a Biba Integrity policy.
 * mac_biba represents a Biba label, with mb_type determining its properties,
 * and mb_grade represents the hierarchal grade if valid for the current
 * mb_type.  These structures will move to mac_biba.h once we have dymamic
 * labels exposed to userland.
 */
struct mac_biba_element {
	u_short	mbe_type;
	u_short	mbe_grade;
};

/*
 * Biba labels consist of two components: a single label, and a label
 * range.  Depending on the context, one or both may be used; the mb_flags
 * field permits the provider to indicate what fields are intended for
 * use.
 */
struct mac_biba {
	int			mb_flags;
	struct mac_biba_element	mb_single;
	struct mac_biba_element mb_rangelow, mb_rangehigh;
};

/*
 * Structures and constants associated with a Multi-Level Security policy.
 * mac_mls represents an MLS label, with mm_type determining its properties,
 * and mm_level represents the hierarchal sensitivity level if valid for the
 * current mm_type.  These structures will move to mac_mls.h once we have
 * dynamic labels exposed to userland.
 */
struct mac_mls_element {
	u_short	mme_type;
	u_short	mme_level;
};

/*
 * MLS labels consist of two components: a single label, and a label
 * range.  Depending on the context, one or both may be used; the mb_flags
 * field permits the provider to indicate what fields are intended for
 * use.
 */
struct mac_mls {
	int			mm_flags;
	struct mac_mls_element	mm_single;
	struct mac_mls_element	mm_rangelow, mm_rangehigh;
};

/*
 * Structures and constants associated with a Type Enforcement policy.
 * mac_te represents a Type Enforcement label.
 */
#define	MAC_TE_TYPE_MAXLEN	32
struct mac_te {
	char	mt_type[MAC_TE_TYPE_MAXLEN+1];	/* TE type */
};

struct mac_sebsd {
	uint32_t	ms_psid;	/* persistent sid storage */
};

/*
 * Composite structures and constants which combine the various policy
 * elements into common structures to be associated with subjects and
 * objects.
 */
struct mac {
	int		m_macflags;
	struct mac_biba	m_biba;
	struct mac_mls	m_mls;
	struct mac_te	m_te;
	struct mac_sebsd m_sebsd;
};
typedef struct mac	*mac_t;

#define	MAC_FLAG_INITIALIZED		0x00000001	/* Is initialized. */

#ifndef _KERNEL

/*
 * POSIX.1e functions visible in the application namespace.
 */
int	mac_dominate(const mac_t _labela, const mac_t _labelb);
int	mac_equal(const mac_t labela, const mac_t _labelb);
int	mac_free(void *_buf_p);
mac_t	mac_from_text(const char *_text_p);
mac_t	mac_get_fd(int _fildes);
mac_t	mac_get_file(const char *_path_p);
mac_t	mac_get_proc(void);
mac_t	mac_glb(const mac_t _labela, const mac_t _labelb);
mac_t	mac_lub(const mac_t _labela, const mac_t _labelb);
int	mac_set_fd(int _fildes, const mac_t _label);
int	mac_set_file(const char *_path_p, mac_t _label);
int	mac_set_proc(const mac_t _label);
ssize_t	mac_size(mac_t _label);
char *	mac_to_text(const mac_t _label, size_t *_len_p);
int	mac_valid(const mac_t _label);

/*
 * Extensions to POSIX.1e visible in the application namespace.
 */
int	mac_is_present_np(const char *_policyname);
int	mac_syscall(const char *_policyname, int call, void *arg);

/*
 * System calls wrapped by some POSIX.1e functions.
 */
int	__mac_get_fd(int _fd, struct mac *_mac_p);
int	__mac_get_file(const char *_path_p, struct mac *_mac_p);
int	__mac_get_proc(struct mac *_mac_p);
int	__mac_set_fd(int fd, struct mac *_mac_p);
int	__mac_set_file(const char *_path_p, struct mac *_mac_p);
int	__mac_set_proc(struct mac *_mac_p);

#else /* _KERNEL */

/*
 * Kernel functions to manage and evaluate labels.
 */
struct bpf_d;
struct componentname;
struct devfs_dirent;
struct ifnet;
struct ifreq;
struct ipq;
struct mbuf;
struct mount;
struct proc;
struct sockaddr;
struct socket;
struct pipe;
struct thread;
struct timespec;
struct ucred;
struct uio;
struct vattr;
struct vnode;

#include <sys/acl.h>			/* XXX acl_type_t */

struct vop_refreshlabel_args;
struct vop_setlabel_args;

/*
 * Label operations.
 */
void	mac_init_bpfdesc(struct bpf_d *);
void	mac_init_cred(struct ucred *);
void	mac_init_devfsdirent(struct devfs_dirent *);
void	mac_init_ifnet(struct ifnet *);
void	mac_init_ipq(struct ipq *);
void	mac_init_socket(struct socket *);
void	mac_init_pipe(struct pipe *);
int	mac_init_mbuf(struct mbuf *m, int how);
void	mac_init_mount(struct mount *);
void	mac_init_vnode(struct vnode *);
void	mac_destroy_bpfdesc(struct bpf_d *);
void	mac_destroy_cred(struct ucred *);
void	mac_destroy_devfsdirent(struct devfs_dirent *);
void	mac_destroy_ifnet(struct ifnet *);
void	mac_destroy_ipq(struct ipq *);
void	mac_destroy_socket(struct socket *);
void	mac_destroy_pipe(struct pipe *);
void	mac_destroy_mbuf(struct mbuf *);
void	mac_destroy_mount(struct mount *);
void	mac_destroy_vnode(struct vnode *);

/*
 * Labeling event operations: file system objects, and things that
 * look a lot like file system objects.
 */
void	mac_create_devfs_device(dev_t dev, struct devfs_dirent *de);
void	mac_create_devfs_directory(char *dirname, int dirnamelen,
	    struct devfs_dirent *de);
void	mac_create_devfs_vnode(struct devfs_dirent *de, struct vnode *vp);
void	mac_create_vnode(struct ucred *cred, struct vnode *parent,
	    struct vnode *child);
void	mac_create_mount(struct ucred *cred, struct mount *mp);
void	mac_create_root_mount(struct ucred *cred, struct mount *mp);
void	mac_relabel_vnode(struct ucred *cred, struct vnode *vp,
	    struct label *newlabel);
void	mac_update_devfsdirent(struct devfs_dirent *de, struct vnode *vp);
void	mac_update_procfsvnode(struct vnode *vp, struct ucred *cred);
void	mac_update_vnode_from_mount(struct vnode *vp, struct mount *mp);

/*
 * Labeling event operations: IPC objects.
 */
void	mac_create_mbuf_from_socket(struct socket *so, struct mbuf *m);
void	mac_create_socket(struct ucred *cred, struct socket *socket);
void	mac_create_socket_from_socket(struct socket *oldsocket,
	    struct socket *newsocket);
void	mac_set_socket_peer_from_mbuf(struct mbuf *mbuf,
	    struct socket *socket);
void	mac_set_socket_peer_from_socket(struct socket *oldsocket,
	    struct socket *newsocket);
void	mac_create_pipe(struct ucred *cred, struct pipe *pipe);

/*
 * Labeling event operations: network objects.
 */
void	mac_create_bpfdesc(struct ucred *cred, struct bpf_d *bpf_d);
void	mac_create_ifnet(struct ifnet *ifp);
void	mac_create_ipq(struct mbuf *fragment, struct ipq *ipq);
void	mac_create_datagram_from_ipq(struct ipq *ipq, struct mbuf *datagram);
void	mac_create_fragment(struct mbuf *datagram, struct mbuf *fragment);
void	mac_create_mbuf_from_mbuf(struct mbuf *oldmbuf, struct mbuf *newmbuf);
void	mac_create_mbuf_linklayer(struct ifnet *ifnet, struct mbuf *m);
void	mac_create_mbuf_from_bpfdesc(struct bpf_d *bpf_d, struct mbuf *m);
void	mac_create_mbuf_from_ifnet(struct ifnet *ifnet, struct mbuf *m);
void	mac_create_mbuf_multicast_encap(struct mbuf *oldmbuf,
	    struct ifnet *ifnet, struct mbuf *newmbuf);
void	mac_create_mbuf_netlayer(struct mbuf *oldmbuf, struct mbuf *newmbuf);
int	mac_fragment_match(struct mbuf *fragment, struct ipq *ipq);
void	mac_update_ipq(struct mbuf *fragment, struct ipq *ipq);

/*
 * Labeling event operations: processes.
 */
void	mac_create_cred(struct ucred *cred_parent, struct ucred *cred_child);
void	mac_execve_transition(struct ucred *old, struct ucred *new,
	    struct vnode *vp);
int	mac_execve_will_transition(struct ucred *old, struct vnode *vp);
void	mac_create_proc0(struct ucred *cred);
void	mac_create_proc1(struct ucred *cred);
void	mac_thread_userret(struct thread *td);

/* Access control checks. */
int	mac_check_bpfdesc_receive(struct bpf_d *bpf_d, struct ifnet *ifnet);
int	mac_check_cred_visible(struct ucred *u1, struct ucred *u2);
int	mac_check_ifnet_transmit(struct ifnet *ifnet, struct mbuf *m);
int	mac_check_mount_stat(struct ucred *cred, struct mount *mp);
int	mac_check_pipe_ioctl(struct ucred *cred, struct pipe *pipe,
	    unsigned long cmd, void *data);
int	mac_check_pipe_poll(struct ucred *cred, struct pipe *pipe);
int	mac_check_pipe_read(struct ucred *cred, struct pipe *pipe);
int	mac_check_pipe_stat(struct ucred *cred, struct pipe *pipe);
int	mac_check_pipe_write(struct ucred *cred, struct pipe *pipe);
int	mac_check_proc_debug(struct ucred *cred, struct proc *proc);
int	mac_check_proc_sched(struct ucred *cred, struct proc *proc);
int	mac_check_proc_signal(struct ucred *cred, struct proc *proc,
	    int signum);
int	mac_check_socket_bind(struct ucred *cred, struct socket *so,
	    struct sockaddr *sockaddr);
int	mac_check_socket_connect(struct ucred *cred, struct socket *so,
	    struct sockaddr *sockaddr);
int	mac_check_socket_deliver(struct socket *so, struct mbuf *m);
int	mac_check_socket_listen(struct ucred *cred, struct socket *so);
int	mac_check_socket_visible(struct ucred *cred, struct socket *so);
int	mac_check_vnode_access(struct ucred *cred, struct vnode *vp,
	    int flags);
int	mac_check_vnode_chdir(struct ucred *cred, struct vnode *dvp);
int	mac_check_vnode_chroot(struct ucred *cred, struct vnode *dvp);
int	mac_check_vnode_create(struct ucred *cred, struct vnode *dvp,
	    struct componentname *cnp, struct vattr *vap);
int	mac_check_vnode_delete(struct ucred *cred, struct vnode *dvp,
	    struct vnode *vp, struct componentname *cnp);
int	mac_check_vnode_deleteacl(struct ucred *cred, struct vnode *vp,
	    acl_type_t type);
int	mac_check_vnode_exec(struct ucred *cred, struct vnode *vp);
int	mac_check_vnode_getacl(struct ucred *cred, struct vnode *vp,
	    acl_type_t type);
int	mac_check_vnode_getextattr(struct ucred *cred, struct vnode *vp,
	    int attrnamespace, const char *name, struct uio *uio);
int	mac_check_vnode_link(struct ucred *cred, struct vnode *dvp,
	    struct vnode *vp, struct componentname *cnp);
int	mac_check_vnode_lookup(struct ucred *cred, struct vnode *dvp,
 	    struct componentname *cnp);
/* XXX This u_char should be vm_prot_t! */
u_char	mac_check_vnode_mmap_prot(struct ucred *cred, struct vnode *vp,
	    int newmapping);
int	mac_check_vnode_open(struct ucred *cred, struct vnode *vp,
	    mode_t acc_mode);
int	mac_check_vnode_poll(struct ucred *active_cred,
	    struct ucred *file_cred, struct vnode *vp);
int	mac_check_vnode_read(struct ucred *active_cred,
	    struct ucred *file_cred, struct vnode *vp);
int	mac_check_vnode_readdir(struct ucred *cred, struct vnode *vp);
int	mac_check_vnode_readlink(struct ucred *cred, struct vnode *vp);
int	mac_check_vnode_rename_from(struct ucred *cred, struct vnode *dvp,
	    struct vnode *vp, struct componentname *cnp);
int	mac_check_vnode_rename_to(struct ucred *cred, struct vnode *dvp,
	    struct vnode *vp, int samedir, struct componentname *cnp);
int	mac_check_vnode_revoke(struct ucred *cred, struct vnode *vp);
int	mac_check_vnode_setacl(struct ucred *cred, struct vnode *vp,
	    acl_type_t type, struct acl *acl);
int	mac_check_vnode_setextattr(struct ucred *cred, struct vnode *vp,
	    int attrnamespace, const char *name, struct uio *uio);
int	mac_check_vnode_setflags(struct ucred *cred, struct vnode *vp,
	    u_long flags);
int	mac_check_vnode_setmode(struct ucred *cred, struct vnode *vp,
	    mode_t mode);
int	mac_check_vnode_setowner(struct ucred *cred, struct vnode *vp,
	    uid_t uid, gid_t gid);
int	mac_check_vnode_setutimes(struct ucred *cred, struct vnode *vp,
	    struct timespec atime, struct timespec mtime);
int	mac_check_vnode_stat(struct ucred *active_cred,
	    struct ucred *file_cred, struct vnode *vp);
int	mac_check_vnode_write(struct ucred *active_cred,
	    struct ucred *file_cred, struct vnode *vp);
int	mac_getsockopt_label_get(struct ucred *cred, struct socket *so,
	    struct mac *extmac);
int	mac_getsockopt_peerlabel_get(struct ucred *cred, struct socket *so,
	    struct mac *extmac);
int	mac_ioctl_ifnet_get(struct ucred *cred, struct ifreq *ifr,
	    struct ifnet *ifnet);
int	mac_ioctl_ifnet_set(struct ucred *cred, struct ifreq *ifr,
	    struct ifnet *ifnet);
int	mac_setsockopt_label_set(struct ucred *cred, struct socket *so,
	    struct mac *extmac);
int	mac_pipe_label_set(struct ucred *cred, struct pipe *pipe,
	    struct label *label);

/*
 * Calls to help various file systems implement labeling functionality
 * using their existing EA implementation.
 */
int	vop_stdcreatevnode_ea(struct vnode *dvp, struct vnode *tvp,
	    struct ucred *cred);
int	vop_stdrefreshlabel_ea(struct vop_refreshlabel_args *ap);
int	vop_stdsetlabel_ea(struct vop_setlabel_args *ap);

#endif /* _KERNEL */

#endif /* !_SYS_MAC_H */
