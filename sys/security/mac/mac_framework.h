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
#define	MAC_MAX_POLICY_NAME		32
#define	MAC_MAX_LABEL_ELEMENT_NAME	32
#define	MAC_MAX_LABEL_ELEMENT_DATA	4096
#define	MAC_MAX_LABEL_BUF_LEN		8192

struct mac {
	size_t		 m_buflen;
	char		*m_string;
};

typedef struct mac	*mac_t;

#ifndef _KERNEL

/*
 * Location of the userland MAC framework configuration file.  mac.conf
 * binds policy names to shared libraries that understand those policies,
 * as well as setting defaults for MAC-aware applications.
 */
#define	MAC_CONFFILE	"/etc/mac.conf"

/*
 * Extended non-POSIX.1e interfaces that offer additional services
 * available from the userland and kernel MAC frameworks.
 */
int		 mac_execve(char *fname, char **argv, char **envv,
		    mac_t _label);
int		 mac_free(mac_t _label);
int		 mac_from_text(mac_t *_label, const char *_text);
int		 mac_get_fd(int _fd, mac_t _label);
int		 mac_get_file(const char *_path, mac_t _label);
int		 mac_get_link(const char *_path, mac_t _label);
int		 mac_get_pid(pid_t _pid, mac_t _label);
int		 mac_get_proc(mac_t _label);
int		 mac_is_present(const char *_policyname);
int		 mac_prepare(mac_t *_label, char *_elements);
int		 mac_prepare_file_label(mac_t *_label);
int		 mac_prepare_ifnet_label(mac_t *_label);
int		 mac_prepare_process_label(mac_t *_label);
int		 mac_set_fd(int _fildes, const mac_t _label);
int		 mac_set_file(const char *_path, mac_t _label);
int		 mac_set_link(const char *_path, mac_t _label);
int		 mac_set_proc(const mac_t _label);
int		 mac_syscall(const char *_policyname, int _call, void *_arg);
int		 mac_to_text(mac_t mac, char **_text);

#else /* _KERNEL */

/*
 * Kernel functions to manage and evaluate labels.
 */
struct bpf_d;
struct componentname;
struct devfs_dirent;
struct ifnet;
struct ifreq;
struct image_params;
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

struct vop_setlabel_args;

/*
 * Label operations.
 */
void	mac_init_bpfdesc(struct bpf_d *);
void	mac_init_cred(struct ucred *);
void	mac_init_devfsdirent(struct devfs_dirent *);
void	mac_init_ifnet(struct ifnet *);
void	mac_init_ipq(struct ipq *);
int	mac_init_socket(struct socket *, int flag);
void	mac_init_pipe(struct pipe *);
int	mac_init_mbuf(struct mbuf *m, int flag);
void	mac_init_mount(struct mount *);
void	mac_init_vnode(struct vnode *);
void	mac_init_vnode_label(struct label *);
void	mac_copy_vnode_label(struct label *, struct label *label);
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
void	mac_destroy_vnode_label(struct label *);

/*
 * Labeling event operations: file system objects, and things that
 * look a lot like file system objects.
 */
void	mac_associate_vnode_devfs(struct mount *mp, struct devfs_dirent *de,
	    struct vnode *vp);
int	mac_associate_vnode_extattr(struct mount *mp, struct vnode *vp);
void	mac_associate_vnode_singlelabel(struct mount *mp, struct vnode *vp);
void	mac_create_devfs_device(dev_t dev, struct devfs_dirent *de);
void	mac_create_devfs_directory(char *dirname, int dirnamelen,
	    struct devfs_dirent *de);
void	mac_create_devfs_symlink(struct ucred *cred, struct devfs_dirent *dd,
	    struct devfs_dirent *de);
int	mac_create_vnode_extattr(struct ucred *cred, struct mount *mp,
	    struct vnode *dvp, struct vnode *vp, struct componentname *cnp);
void	mac_create_mount(struct ucred *cred, struct mount *mp);
void	mac_create_root_mount(struct ucred *cred, struct mount *mp);
void	mac_relabel_vnode(struct ucred *cred, struct vnode *vp,
	    struct label *newlabel);
void	mac_update_devfsdirent(struct devfs_dirent *de, struct vnode *vp);

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
int	mac_execve_enter(struct image_params *imgp, struct mac *mac_p,
	    struct label *execlabel);
void	mac_execve_exit(struct image_params *imgp);
void	mac_execve_transition(struct ucred *old, struct ucred *new,
	    struct vnode *vp, struct label *interpvnodelabel,
	    struct image_params *imgp);
int	mac_execve_will_transition(struct ucred *old, struct vnode *vp,
	    struct label *interpvnodelabel, struct image_params *imgp);
void	mac_create_proc0(struct ucred *cred);
void	mac_create_proc1(struct ucred *cred);
void	mac_thread_userret(struct thread *td);

/* Access control checks. */
int	mac_check_bpfdesc_receive(struct bpf_d *bpf_d, struct ifnet *ifnet);
int	mac_check_cred_visible(struct ucred *u1, struct ucred *u2);
int	mac_check_ifnet_transmit(struct ifnet *ifnet, struct mbuf *m);
int	mac_check_kenv_dump(struct ucred *cred);
int	mac_check_kenv_get(struct ucred *cred, char *name);
int	mac_check_kenv_set(struct ucred *cred, char *name, char *value);
int	mac_check_kenv_unset(struct ucred *cred, char *name);
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
int	mac_check_socket_receive(struct ucred *cred, struct socket *so);
int	mac_check_socket_send(struct ucred *cred, struct socket *so);
int	mac_check_socket_visible(struct ucred *cred, struct socket *so);
int	mac_check_system_acct(struct ucred *cred, struct vnode *vp);
int	mac_check_system_nfsd(struct ucred *cred);
int	mac_check_system_reboot(struct ucred *cred, int howto);
int	mac_check_system_settime(struct ucred *cred);
int	mac_check_system_swapon(struct ucred *cred, struct vnode *vp);
int	mac_check_system_sysctl(struct ucred *cred, int *name,
	    u_int namelen, void *old, size_t *oldlenp, int inkernel,
	    void *new, size_t newlen);
int	mac_check_vnode_access(struct ucred *cred, struct vnode *vp,
	    int acc_mode);
int	mac_check_vnode_chdir(struct ucred *cred, struct vnode *dvp);
int	mac_check_vnode_chroot(struct ucred *cred, struct vnode *dvp);
int	mac_check_vnode_create(struct ucred *cred, struct vnode *dvp,
	    struct componentname *cnp, struct vattr *vap);
int	mac_check_vnode_delete(struct ucred *cred, struct vnode *dvp,
	    struct vnode *vp, struct componentname *cnp);
int	mac_check_vnode_deleteacl(struct ucred *cred, struct vnode *vp,
	    acl_type_t type);
int	mac_check_vnode_exec(struct ucred *cred, struct vnode *vp,
	    struct image_params *imgp);
int	mac_check_vnode_getacl(struct ucred *cred, struct vnode *vp,
	    acl_type_t type);
int	mac_check_vnode_getextattr(struct ucred *cred, struct vnode *vp,
	    int attrnamespace, const char *name, struct uio *uio);
int	mac_check_vnode_link(struct ucred *cred, struct vnode *dvp,
	    struct vnode *vp, struct componentname *cnp);
int	mac_check_vnode_lookup(struct ucred *cred, struct vnode *dvp,
 	    struct componentname *cnp);
int	mac_check_vnode_mmap(struct ucred *cred, struct vnode *vp,
	    int prot);
int	mac_check_vnode_mprotect(struct ucred *cred, struct vnode *vp,
	    int prot);
int	mac_check_vnode_open(struct ucred *cred, struct vnode *vp,
	    int acc_mode);
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
int	vop_stdsetlabel_ea(struct vop_setlabel_args *ap);

#endif /* !_KERNEL */

#endif /* !_SYS_MAC_H */
