/*-
 * Copyright (c) 1999-2002 Robert N. M. Watson
 * Copyright (c) 2001-2005 Networks Associates Technology, Inc.
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
 * Kernel interface for MAC policy modules.
 */
#ifndef _SYS_MAC_POLICY_H_
#define _SYS_MAC_POLICY_H_

/*-
 * Pluggable access control policy definition structure.
 *
 * List of operations that are performed as part of the implementation
 * of a MAC policy.  Policy implementors declare operations with a
 * mac_policy_ops structure, and using the MAC_POLICY_SET() macro.
 * If an entry point is not declared, then then the policy will be ignored
 * during evaluation of that event or check.
 *
 * Operations are sorted first by general class of operation, then
 * alphabetically.
 */
struct acl;
struct bpf_d;
struct componentname;
struct devfs_dirent;
struct ifnet;
struct image_params;
struct inpcb;
struct ipq;
struct label;
struct mac_policy_conf;
struct mbuf;
struct mount;
struct msqid_kernel;
struct pipepair;
struct proc;
struct sbuf;
struct semid_kernel;
struct shmid_kernel;
struct sockaddr;
struct socket;
struct sysctl_oid;
struct sysctl_req;
struct thread;
struct ucred;
struct uio;
struct vattr;
struct vnode;
struct mac_policy_ops {
	/*
	 * Policy module operations.
	 */
	void	(*mpo_destroy)(struct mac_policy_conf *mpc);
	void	(*mpo_init)(struct mac_policy_conf *mpc);

	/*
	 * General policy-directed security system call so that policies may
	 * implement new services without reserving explicit system call
	 * numbers.
	 */
	int	(*mpo_syscall)(struct thread *td, int call, void *arg);

	/*
	 * Label operations.  Initialize label storage, destroy label
	 * storage, recycle for re-use without init/destroy, copy a label to
	 * initialized storage, and externalize/internalize from/to
	 * initialized storage.
	 */
	void	(*mpo_init_bpfdesc_label)(struct label *label);
	void	(*mpo_init_cred_label)(struct label *label);
	void	(*mpo_init_devfsdirent_label)(struct label *label);
	void	(*mpo_init_ifnet_label)(struct label *label);
	int	(*mpo_init_inpcb_label)(struct label *label, int flag);
	void	(*mpo_init_sysv_msgmsg_label)(struct label *label);
	void	(*mpo_init_sysv_msgqueue_label)(struct label *label);
	void	(*mpo_init_sysv_sema_label)(struct label *label);
	void	(*mpo_init_sysv_shm_label)(struct label *label);
	int	(*mpo_init_ipq_label)(struct label *label, int flag);
	int	(*mpo_init_mbuf_label)(struct label *label, int flag);
	void	(*mpo_init_mount_label)(struct label *label);
	void	(*mpo_init_mount_fs_label)(struct label *label);
	int	(*mpo_init_socket_label)(struct label *label, int flag);
	int	(*mpo_init_socket_peer_label)(struct label *label, int flag);
	void	(*mpo_init_pipe_label)(struct label *label);
	void	(*mpo_init_proc_label)(struct label *label);
	void	(*mpo_init_vnode_label)(struct label *label);
	void	(*mpo_destroy_bpfdesc_label)(struct label *label);
	void	(*mpo_destroy_cred_label)(struct label *label);
	void	(*mpo_destroy_devfsdirent_label)(struct label *label);
	void	(*mpo_destroy_ifnet_label)(struct label *label);
	void	(*mpo_destroy_inpcb_label)(struct label *label);
	void	(*mpo_destroy_sysv_msgmsg_label)(struct label *label);
	void	(*mpo_destroy_sysv_msgqueue_label)(struct label *label);
	void	(*mpo_destroy_sysv_sema_label)(struct label *label);
	void	(*mpo_destroy_sysv_shm_label)(struct label *label);
	void	(*mpo_destroy_ipq_label)(struct label *label);
	void	(*mpo_destroy_mbuf_label)(struct label *label);
	void	(*mpo_destroy_mount_label)(struct label *label);
	void	(*mpo_destroy_mount_fs_label)(struct label *label);
	void	(*mpo_destroy_socket_label)(struct label *label);
	void	(*mpo_destroy_socket_peer_label)(struct label *label);
	void	(*mpo_destroy_pipe_label)(struct label *label);
	void	(*mpo_destroy_proc_label)(struct label *label);
	void	(*mpo_destroy_vnode_label)(struct label *label);
	void	(*mpo_cleanup_sysv_msgmsg)(struct label *msglabel);
	void	(*mpo_cleanup_sysv_msgqueue)(struct label *msqlabel);
	void	(*mpo_cleanup_sysv_sema)(struct label *semalabel);
	void	(*mpo_cleanup_sysv_shm)(struct label *shmlabel);
	void	(*mpo_copy_cred_label)(struct label *src,
		    struct label *dest);
	void	(*mpo_copy_ifnet_label)(struct label *src,
		    struct label *dest);
	void	(*mpo_copy_mbuf_label)(struct label *src,
		    struct label *dest);
	void	(*mpo_copy_pipe_label)(struct label *src,
		    struct label *dest);
	void	(*mpo_copy_socket_label)(struct label *src,
		    struct label *dest);
	void	(*mpo_copy_vnode_label)(struct label *src,
		    struct label *dest);
	int	(*mpo_externalize_cred_label)(struct label *label,
		    char *element_name, struct sbuf *sb, int *claimed);
	int	(*mpo_externalize_ifnet_label)(struct label *label,
		    char *element_name, struct sbuf *sb, int *claimed);
	int	(*mpo_externalize_pipe_label)(struct label *label,
		    char *element_name, struct sbuf *sb, int *claimed);
	int	(*mpo_externalize_socket_label)(struct label *label,
		    char *element_name, struct sbuf *sb, int *claimed);
	int	(*mpo_externalize_socket_peer_label)(struct label *label,
		    char *element_name, struct sbuf *sb, int *claimed);
	int	(*mpo_externalize_vnode_label)(struct label *label,
		    char *element_name, struct sbuf *sb, int *claimed);
	int	(*mpo_internalize_cred_label)(struct label *label,
		    char *element_name, char *element_data, int *claimed);
	int	(*mpo_internalize_ifnet_label)(struct label *label,
		    char *element_name, char *element_data, int *claimed);
	int	(*mpo_internalize_pipe_label)(struct label *label,
		    char *element_name, char *element_data, int *claimed);
	int	(*mpo_internalize_socket_label)(struct label *label,
		    char *element_name, char *element_data, int *claimed);
	int	(*mpo_internalize_vnode_label)(struct label *label,
		    char *element_name, char *element_data, int *claimed);

	/*
	 * Labeling event operations: file system objects, and things that
	 * look a lot like file system objects.
	 */
	void	(*mpo_associate_vnode_devfs)(struct mount *mp,
		    struct label *fslabel, struct devfs_dirent *de,
		    struct label *delabel, struct vnode *vp,
		    struct label *vlabel);
	int	(*mpo_associate_vnode_extattr)(struct mount *mp,
		    struct label *fslabel, struct vnode *vp,
		    struct label *vlabel);
	void	(*mpo_associate_vnode_singlelabel)(struct mount *mp,
		    struct label *fslabel, struct vnode *vp,
		    struct label *vlabel);
	void	(*mpo_create_devfs_device)(struct mount *mp, struct cdev *dev,
		    struct devfs_dirent *de, struct label *label);
	void	(*mpo_create_devfs_directory)(struct mount *mp, char *dirname,
		    int dirnamelen, struct devfs_dirent *de,
		    struct label *label);
	void	(*mpo_create_devfs_symlink)(struct ucred *cred,
		    struct mount *mp, struct devfs_dirent *dd,
		    struct label *ddlabel, struct devfs_dirent *de,
		    struct label *delabel);
	int	(*mpo_create_vnode_extattr)(struct ucred *cred,
		    struct mount *mp, struct label *fslabel,
		    struct vnode *dvp, struct label *dlabel,
		    struct vnode *vp, struct label *vlabel,
		    struct componentname *cnp);
	void	(*mpo_create_mount)(struct ucred *cred, struct mount *mp,
		    struct label *mntlabel, struct label *fslabel);
	void	(*mpo_create_root_mount)(struct ucred *cred, struct mount *mp,
		    struct label *mountlabel, struct label *fslabel);
	void	(*mpo_relabel_vnode)(struct ucred *cred, struct vnode *vp,
		    struct label *vnodelabel, struct label *label);
	int	(*mpo_setlabel_vnode_extattr)(struct ucred *cred,
		    struct vnode *vp, struct label *vlabel,
		    struct label *intlabel);
	void	(*mpo_update_devfsdirent)(struct mount *mp,
		    struct devfs_dirent *devfs_dirent,
		    struct label *direntlabel, struct vnode *vp,
		    struct label *vnodelabel);

	/*
	 * Labeling event operations: IPC objects.
	 */
	void	(*mpo_create_mbuf_from_socket)(struct socket *so,
		    struct label *socketlabel, struct mbuf *m,
		    struct label *mbuflabel);
	void	(*mpo_create_socket)(struct ucred *cred, struct socket *so,
		    struct label *socketlabel);
	void	(*mpo_create_socket_from_socket)(struct socket *oldsocket,
		    struct label *oldsocketlabel, struct socket *newsocket,
		    struct label *newsocketlabel);
	void	(*mpo_relabel_socket)(struct ucred *cred, struct socket *so,
		    struct label *oldlabel, struct label *newlabel);
	void	(*mpo_relabel_pipe)(struct ucred *cred, struct pipepair *pp,
		    struct label *oldlabel, struct label *newlabel);
	void	(*mpo_set_socket_peer_from_mbuf)(struct mbuf *mbuf,
		    struct label *mbuflabel, struct socket *so,
		    struct label *socketpeerlabel);
	void	(*mpo_set_socket_peer_from_socket)(struct socket *oldsocket,
		    struct label *oldsocketlabel, struct socket *newsocket,
		    struct label *newsocketpeerlabel);
	void	(*mpo_create_pipe)(struct ucred *cred, struct pipepair *pp,
		    struct label *pipelabel);

	/*
	 * Labeling event operations: System V IPC primitives.
	 */
	void	(*mpo_create_sysv_msgmsg)(struct ucred *cred,
		    struct msqid_kernel *msqkptr, struct label *msqlabel,
		    struct msg *msgptr, struct label *msglabel);
	void	(*mpo_create_sysv_msgqueue)(struct ucred *cred,
		    struct msqid_kernel *msqkptr, struct label *msqlabel);
	void	(*mpo_create_sysv_sema)(struct ucred *cred,
		    struct semid_kernel *semakptr, struct label *semalabel);
	void	(*mpo_create_sysv_shm)(struct ucred *cred,
		    struct shmid_kernel *shmsegptr, struct label *shmlabel);

	/*
	 * Labeling event operations: network objects.
	 */
	void	(*mpo_create_bpfdesc)(struct ucred *cred, struct bpf_d *bpf_d,
		    struct label *bpflabel);
	void	(*mpo_create_ifnet)(struct ifnet *ifnet,
		    struct label *ifnetlabel);
	void	(*mpo_create_inpcb_from_socket)(struct socket *so,
		    struct label *solabel, struct inpcb *inp,
		    struct label *inplabel);
	void	(*mpo_create_ipq)(struct mbuf *fragment,
		    struct label *fragmentlabel, struct ipq *ipq,
		    struct label *ipqlabel);
	void	(*mpo_create_datagram_from_ipq)
		    (struct ipq *ipq, struct label *ipqlabel,
		    struct mbuf *datagram, struct label *datagramlabel);
	void	(*mpo_create_fragment)(struct mbuf *datagram,
		    struct label *datagramlabel, struct mbuf *fragment,
		    struct label *fragmentlabel);
	void	(*mpo_create_mbuf_from_inpcb)(struct inpcb *inp,
		    struct label *inplabel, struct mbuf *m,
		    struct label *mlabel);
	void	(*mpo_create_mbuf_from_mbuf)(struct mbuf *oldmbuf,
		    struct label *oldlabel, struct mbuf *newmbuf,
		    struct label *newlabel);
	void	(*mpo_create_mbuf_linklayer)(struct ifnet *ifnet,
		    struct label *ifnetlabel, struct mbuf *mbuf,
		    struct label *mbuflabel);
	void	(*mpo_create_mbuf_from_bpfdesc)(struct bpf_d *bpf_d,
		    struct label *bpflabel, struct mbuf *mbuf,
		    struct label *mbuflabel);
	void	(*mpo_create_mbuf_from_ifnet)(struct ifnet *ifnet,
		    struct label *ifnetlabel, struct mbuf *mbuf,
		    struct label *mbuflabel);
	void	(*mpo_create_mbuf_multicast_encap)(struct mbuf *oldmbuf,
		    struct label *oldmbuflabel, struct ifnet *ifnet,
		    struct label *ifnetlabel, struct mbuf *newmbuf,
		    struct label *newmbuflabel);
	void	(*mpo_create_mbuf_netlayer)(struct mbuf *oldmbuf,
		    struct label *oldmbuflabel, struct mbuf *newmbuf,
		    struct label *newmbuflabel);
	int	(*mpo_fragment_match)(struct mbuf *fragment,
		    struct label *fragmentlabel, struct ipq *ipq,
		    struct label *ipqlabel);
	void	(*mpo_reflect_mbuf_icmp)(struct mbuf *m,
		    struct label *mlabel);
	void	(*mpo_reflect_mbuf_tcp)(struct mbuf *m, struct label *mlabel);
	void	(*mpo_relabel_ifnet)(struct ucred *cred, struct ifnet *ifnet,
		    struct label *ifnetlabel, struct label *newlabel);
	void	(*mpo_update_ipq)(struct mbuf *fragment,
		    struct label *fragmentlabel, struct ipq *ipq,
		    struct label *ipqlabel);
	void	(*mpo_inpcb_sosetlabel)(struct socket *so,
		    struct label *label, struct inpcb *inp,
		    struct label *inplabel);

	/*
	 * Labeling event operations: processes.
	 */
	void	(*mpo_execve_transition)(struct ucred *old, struct ucred *new,
		    struct vnode *vp, struct label *vnodelabel,
		    struct label *interpvnodelabel,
		    struct image_params *imgp, struct label *execlabel);
	int	(*mpo_execve_will_transition)(struct ucred *old,
		    struct vnode *vp, struct label *vnodelabel,
		    struct label *interpvnodelabel,
		    struct image_params *imgp, struct label *execlabel);
	void	(*mpo_create_proc0)(struct ucred *cred);
	void	(*mpo_create_proc1)(struct ucred *cred);
	void	(*mpo_relabel_cred)(struct ucred *cred,
		    struct label *newlabel);
	void	(*mpo_thread_userret)(struct thread *thread);

	/*
	 * Access control checks.
	 */
	int	(*mpo_check_bpfdesc_receive)(struct bpf_d *bpf_d,
		    struct label *bpflabel, struct ifnet *ifnet,
		    struct label *ifnetlabel);
	int	(*mpo_check_cred_relabel)(struct ucred *cred,
		    struct label *newlabel);
	int	(*mpo_check_cred_visible)(struct ucred *u1, struct ucred *u2);
	int	(*mpo_check_ifnet_relabel)(struct ucred *cred,
		    struct ifnet *ifnet, struct label *ifnetlabel,
		    struct label *newlabel);
	int	(*mpo_check_ifnet_transmit)(struct ifnet *ifnet,
		    struct label *ifnetlabel, struct mbuf *m,
		    struct label *mbuflabel);
	int	(*mpo_check_inpcb_deliver)(struct inpcb *inp,
		    struct label *inplabel, struct mbuf *m,
		    struct label *mlabel);
	int	(*mpo_check_sysv_msgmsq)(struct ucred *cred,
		    struct msg *msgptr, struct label *msglabel,
		    struct msqid_kernel *msqkptr, struct label *msqklabel);
	int	(*mpo_check_sysv_msgrcv)(struct ucred *cred,
		    struct msg *msgptr, struct label *msglabel);
	int	(*mpo_check_sysv_msgrmid)(struct ucred *cred,
		    struct msg *msgptr, struct label *msglabel);
	int	(*mpo_check_sysv_msqget)(struct ucred *cred,
		    struct msqid_kernel *msqkptr, struct label *msqklabel);
	int	(*mpo_check_sysv_msqsnd)(struct ucred *cred,
		    struct msqid_kernel *msqkptr, struct label *msqklabel);
	int	(*mpo_check_sysv_msqrcv)(struct ucred *cred,
		    struct msqid_kernel *msqkptr, struct label *msqklabel);
	int	(*mpo_check_sysv_msqctl)(struct ucred *cred,
		    struct msqid_kernel *msqkptr, struct label *msqklabel,
		    int cmd);
	int	(*mpo_check_sysv_semctl)(struct ucred *cred,
		    struct semid_kernel *semakptr, struct label *semaklabel,
		    int cmd);
	int	(*mpo_check_sysv_semget)(struct ucred *cred,
		    struct semid_kernel *semakptr, struct label *semaklabel);
	int	(*mpo_check_sysv_semop)(struct ucred *cred,
		    struct semid_kernel *semakptr, struct label *semaklabel,
		    size_t accesstype);
	int	(*mpo_check_sysv_shmat)(struct ucred *cred,
		    struct shmid_kernel *shmsegptr,
		    struct label *shmseglabel, int shmflg);
	int	(*mpo_check_sysv_shmctl)(struct ucred *cred,
		    struct shmid_kernel *shmsegptr,
		    struct label *shmseglabel, int cmd);
	int	(*mpo_check_sysv_shmdt)(struct ucred *cred,
		    struct shmid_kernel *shmsegptr,
		    struct label *shmseglabel);
	int	(*mpo_check_sysv_shmget)(struct ucred *cred,
		    struct shmid_kernel *shmsegptr,
		    struct label *shmseglabel, int shmflg);
	int	(*mpo_check_kenv_dump)(struct ucred *cred);
	int	(*mpo_check_kenv_get)(struct ucred *cred, char *name);
	int	(*mpo_check_kenv_set)(struct ucred *cred, char *name,
		    char *value);
	int	(*mpo_check_kenv_unset)(struct ucred *cred, char *name);
	int	(*mpo_check_kld_load)(struct ucred *cred, struct vnode *vp,
		    struct label *vlabel);
	int	(*mpo_check_kld_stat)(struct ucred *cred);
	int	(*mpo_check_kld_unload)(struct ucred *cred);
	int	(*mpo_check_mount_stat)(struct ucred *cred, struct mount *mp,
		    struct label *mntlabel);
	int	(*mpo_check_pipe_ioctl)(struct ucred *cred,
		    struct pipepair *pp, struct label *pipelabel,
		    unsigned long cmd, void *data);
	int	(*mpo_check_pipe_poll)(struct ucred *cred,
		    struct pipepair *pp, struct label *pipelabel);
	int	(*mpo_check_pipe_read)(struct ucred *cred,
		    struct pipepair *pp, struct label *pipelabel);
	int	(*mpo_check_pipe_relabel)(struct ucred *cred,
		    struct pipepair *pp, struct label *pipelabel,
		    struct label *newlabel);
	int	(*mpo_check_pipe_stat)(struct ucred *cred,
		    struct pipepair *pp, struct label *pipelabel);
	int	(*mpo_check_pipe_write)(struct ucred *cred,
		    struct pipepair *pp, struct label *pipelabel);
	int	(*mpo_check_proc_debug)(struct ucred *cred,
		    struct proc *proc);
	int	(*mpo_check_proc_sched)(struct ucred *cred,
		    struct proc *proc);
	int	(*mpo_check_proc_setuid)(struct ucred *cred, uid_t uid);
	int	(*mpo_check_proc_seteuid)(struct ucred *cred, uid_t euid);
	int	(*mpo_check_proc_setgid)(struct ucred *cred, gid_t gid);
	int	(*mpo_check_proc_setegid)(struct ucred *cred, gid_t egid);
	int	(*mpo_check_proc_setgroups)(struct ucred *cred, int ngroups,
		    gid_t *gidset);
	int	(*mpo_check_proc_setreuid)(struct ucred *cred, uid_t ruid,
		    uid_t euid);
	int	(*mpo_check_proc_setregid)(struct ucred *cred, gid_t rgid,
		    gid_t egid);
	int	(*mpo_check_proc_setresuid)(struct ucred *cred, uid_t ruid,
		    uid_t euid, uid_t suid);
	int	(*mpo_check_proc_setresgid)(struct ucred *cred, gid_t rgid,
		    gid_t egid, gid_t sgid);
	int	(*mpo_check_proc_signal)(struct ucred *cred,
		    struct proc *proc, int signum);
	int	(*mpo_check_proc_wait)(struct ucred *cred,
		    struct proc *proc);
	int	(*mpo_check_socket_accept)(struct ucred *cred,
		    struct socket *so, struct label *socketlabel);
	int	(*mpo_check_socket_bind)(struct ucred *cred,
		    struct socket *so, struct label *socketlabel,
		    struct sockaddr *sockaddr);
	int	(*mpo_check_socket_connect)(struct ucred *cred,
		    struct socket *so, struct label *socketlabel,
		    struct sockaddr *sockaddr);
	int	(*mpo_check_socket_deliver)(struct socket *so,
		    struct label *socketlabel, struct mbuf *m,
		    struct label *mbuflabel);
	int	(*mpo_check_socket_listen)(struct ucred *cred,
		    struct socket *so, struct label *socketlabel);
	int	(*mpo_check_socket_poll)(struct ucred *cred,
		    struct socket *so, struct label *socketlabel);
	int	(*mpo_check_socket_receive)(struct ucred *cred,
		    struct socket *so, struct label *socketlabel);
	int	(*mpo_check_socket_relabel)(struct ucred *cred,
		    struct socket *so, struct label *socketlabel,
		    struct label *newlabel);
	int	(*mpo_check_socket_send)(struct ucred *cred,
		    struct socket *so, struct label *socketlabel);
	int	(*mpo_check_socket_stat)(struct ucred *cred,
		    struct socket *so, struct label *socketlabel);
	int	(*mpo_check_socket_visible)(struct ucred *cred,
		    struct socket *so, struct label *socketlabel);
	int	(*mpo_check_sysarch_ioperm)(struct ucred *cred);
	int	(*mpo_check_system_acct)(struct ucred *cred,
		    struct vnode *vp, struct label *vlabel);
	int	(*mpo_check_system_nfsd)(struct ucred *cred);
	int	(*mpo_check_system_reboot)(struct ucred *cred, int howto);
	int	(*mpo_check_system_settime)(struct ucred *cred);
	int	(*mpo_check_system_swapon)(struct ucred *cred,
		    struct vnode *vp, struct label *label);
	int	(*mpo_check_system_swapoff)(struct ucred *cred,
		    struct vnode *vp, struct label *label);
	int	(*mpo_check_system_sysctl)(struct ucred *cred,
		    struct sysctl_oid *oidp, void *arg1, int arg2,
		    struct sysctl_req *req);
	int	(*mpo_check_vnode_access)(struct ucred *cred,
		    struct vnode *vp, struct label *label, int acc_mode);
	int	(*mpo_check_vnode_chdir)(struct ucred *cred,
		    struct vnode *dvp, struct label *dlabel);
	int	(*mpo_check_vnode_chroot)(struct ucred *cred,
		    struct vnode *dvp, struct label *dlabel);
	int	(*mpo_check_vnode_create)(struct ucred *cred,
		    struct vnode *dvp, struct label *dlabel,
		    struct componentname *cnp, struct vattr *vap);
	int	(*mpo_check_vnode_delete)(struct ucred *cred,
		    struct vnode *dvp, struct label *dlabel,
		    struct vnode *vp, struct label *label,
		    struct componentname *cnp);
	int	(*mpo_check_vnode_deleteacl)(struct ucred *cred,
		    struct vnode *vp, struct label *label, acl_type_t type);
	int	(*mpo_check_vnode_deleteextattr)(struct ucred *cred,
		    struct vnode *vp, struct label *label, int attrnamespace,
		    const char *name);
	int	(*mpo_check_vnode_exec)(struct ucred *cred, struct vnode *vp,
		    struct label *label, struct image_params *imgp,
		    struct label *execlabel);
	int	(*mpo_check_vnode_getacl)(struct ucred *cred,
		    struct vnode *vp, struct label *label, acl_type_t type);
	int	(*mpo_check_vnode_getextattr)(struct ucred *cred,
		    struct vnode *vp, struct label *label, int attrnamespace,
		    const char *name, struct uio *uio);
	int	(*mpo_check_vnode_link)(struct ucred *cred, struct vnode *dvp,
		    struct label *dlabel, struct vnode *vp,
		    struct label *label, struct componentname *cnp);
	int	(*mpo_check_vnode_listextattr)(struct ucred *cred,
		    struct vnode *vp, struct label *label, int attrnamespace);
	int	(*mpo_check_vnode_lookup)(struct ucred *cred,
		    struct vnode *dvp, struct label *dlabel,
		    struct componentname *cnp);
	int	(*mpo_check_vnode_mmap)(struct ucred *cred, struct vnode *vp,
		    struct label *label, int prot, int flags);
	void	(*mpo_check_vnode_mmap_downgrade)(struct ucred *cred,
		    struct vnode *vp, struct label *label, int *prot);
	int	(*mpo_check_vnode_mprotect)(struct ucred *cred,
		    struct vnode *vp, struct label *label, int prot);
	int	(*mpo_check_vnode_open)(struct ucred *cred, struct vnode *vp,
		    struct label *label, int acc_mode);
	int	(*mpo_check_vnode_poll)(struct ucred *active_cred,
		    struct ucred *file_cred, struct vnode *vp,
		    struct label *label);
	int	(*mpo_check_vnode_read)(struct ucred *active_cred,
		    struct ucred *file_cred, struct vnode *vp,
		    struct label *label);
	int	(*mpo_check_vnode_readdir)(struct ucred *cred,
		    struct vnode *dvp, struct label *dlabel);
	int	(*mpo_check_vnode_readlink)(struct ucred *cred,
		    struct vnode *vp, struct label *label);
	int	(*mpo_check_vnode_relabel)(struct ucred *cred,
		    struct vnode *vp, struct label *vnodelabel,
		    struct label *newlabel);
	int	(*mpo_check_vnode_rename_from)(struct ucred *cred,
		    struct vnode *dvp, struct label *dlabel, struct vnode *vp,
		    struct label *label, struct componentname *cnp);
	int	(*mpo_check_vnode_rename_to)(struct ucred *cred,
		    struct vnode *dvp, struct label *dlabel, struct vnode *vp,
		    struct label *label, int samedir,
		    struct componentname *cnp);
	int	(*mpo_check_vnode_revoke)(struct ucred *cred,
		    struct vnode *vp, struct label *label);
	int	(*mpo_check_vnode_setacl)(struct ucred *cred,
		    struct vnode *vp, struct label *label, acl_type_t type,
		    struct acl *acl);
	int	(*mpo_check_vnode_setextattr)(struct ucred *cred,
		    struct vnode *vp, struct label *label, int attrnamespace,
		    const char *name, struct uio *uio);
	int	(*mpo_check_vnode_setflags)(struct ucred *cred,
		    struct vnode *vp, struct label *label, u_long flags);
	int	(*mpo_check_vnode_setmode)(struct ucred *cred,
		    struct vnode *vp, struct label *label, mode_t mode);
	int	(*mpo_check_vnode_setowner)(struct ucred *cred,
		    struct vnode *vp, struct label *label, uid_t uid,
		    gid_t gid);
	int	(*mpo_check_vnode_setutimes)(struct ucred *cred,
		    struct vnode *vp, struct label *label,
		    struct timespec atime, struct timespec mtime);
	int	(*mpo_check_vnode_stat)(struct ucred *active_cred,
		    struct ucred *file_cred, struct vnode *vp,
		    struct label *label);
	int	(*mpo_check_vnode_write)(struct ucred *active_cred,
		    struct ucred *file_cred, struct vnode *vp,
		    struct label *label);
};

/*
 * struct mac_policy_conf is the registration structure for policies, and is
 * provided to the MAC Framework using MAC_POLICY_SET() to invoke a SYSINIT
 * to register the policy.  In general, the fields are immutable, with the
 * exception of the "security field", run-time flags, and policy list entry,
 * which are managed by the MAC Framework.  Be careful when modifying this
 * structure, as its layout is statically compiled into all policies.
 */
struct mac_policy_conf {
	char				*mpc_name;	/* policy name */
	char				*mpc_fullname;	/* policy full name */
	struct mac_policy_ops		*mpc_ops;	/* policy operations */
	int				 mpc_loadtime_flags;	/* flags */
	int				*mpc_field_off; /* security field */
	int				 mpc_runtime_flags; /* flags */
	LIST_ENTRY(mac_policy_conf)	 mpc_list;	/* global list */
};

/* Flags for the mpc_loadtime_flags field. */
#define	MPC_LOADTIME_FLAG_NOTLATE	0x00000001
#define	MPC_LOADTIME_FLAG_UNLOADOK	0x00000002
#define	MPC_LOADTIME_FLAG_LABELMBUFS	0x00000004

/* Flags for the mpc_runtime_flags field. */
#define	MPC_RUNTIME_FLAG_REGISTERED	0x00000001

#define	MAC_POLICY_SET(mpops, mpname, mpfullname, mpflags, privdata_wanted) \
	static struct mac_policy_conf mpname##_mac_policy_conf = {	\
		#mpname,						\
		mpfullname,						\
		mpops,							\
		mpflags,						\
		privdata_wanted,					\
		0,							\
	};								\
	static moduledata_t mpname##_mod = {				\
		#mpname,						\
		mac_policy_modevent,					\
		&mpname##_mac_policy_conf				\
	};								\
	MODULE_DEPEND(mpname, kernel_mac_support, 2, 2, 2);		\
	DECLARE_MODULE(mpname, mpname##_mod, SI_SUB_MAC_POLICY,		\
	    SI_ORDER_MIDDLE)

int	mac_policy_modevent(module_t mod, int type, void *data);

#define	LABEL_TO_SLOT(l, s)	(l)->l_perpolicy[s]

#endif /* !_SYS_MAC_POLICY_H_ */
