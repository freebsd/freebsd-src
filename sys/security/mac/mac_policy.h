/*-
 * Copyright (c) 1999-2002 Robert N. M. Watson
 * Copyright (c) 2001-2005 Networks Associates Technology, Inc.
 * Copyright (c) 2005-2006 SPARTA, Inc.
 * All rights reserved.
 *
 * This software was developed by Robert Watson for the TrustedBSD Project.
 *
 * This software was developed for the FreeBSD Project in part by Network
 * Associates Laboratories, the Security Research Division of Network
 * Associates, Inc. under DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"),
 * as part of the DARPA CHATS research program.
 *
 * This software was enhanced by SPARTA ISSO under SPAWAR contract 
 * N66001-04-C-6019 ("SEFOS").
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
 * $FreeBSD: src/sys/security/mac/mac_policy.h,v 1.94.2.1 2007/11/06 14:46:58 rwatson Exp $
 */
/*
 * Kernel interface for MAC policy modules.
 */
#ifndef _SYS_SECURITY_MAC_MAC_POLICY_H_
#define	_SYS_SECURITY_MAC_MAC_POLICY_H_

#ifndef _KERNEL
#error "no user-serviceable parts inside"
#endif

/*-
 * Pluggable access control policy definition structure.
 *
 * List of operations that are performed as part of the implementation of a
 * MAC policy.  Policy implementors declare operations with a mac_policy_ops
 * structure, and using the MAC_POLICY_SET() macro.  If an entry point is not
 * declared, then then the policy will be ignored during evaluation of that
 * event or check.
 *
 * Operations are sorted first by general class of operation, then
 * alphabetically.
 */
#include <sys/acl.h>	/* XXX acl_type_t */

struct acl;
struct auditinfo;
struct auditinfo_addr;
struct bpf_d;
struct cdev;
struct componentname;
struct devfs_dirent;
struct ifnet;
struct image_params;
struct inpcb;
struct ipq;
struct ksem;
struct label;
struct mac_policy_conf;
struct mbuf;
struct mount;
struct msg;
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

/*
 * Policy module operations.
 */
typedef void	(*mpo_destroy_t)(struct mac_policy_conf *mpc);
typedef void	(*mpo_init_t)(struct mac_policy_conf *mpc);

/*
 * General policy-directed security system call so that policies may
 * implement new services without reserving explicit system call numbers.
 */
typedef int	(*mpo_syscall_t)(struct thread *td, int call, void *arg);

/*
 * Place-holder function pointers for ABI-compatibility purposes.
 */
typedef void	(*mpo_placeholder_t)(void);

/*
 * Label operations.  Initialize label storage, destroy label storage,
 * recycle for re-use without init/destroy, copy a label to initialized
 * storage, and externalize/internalize from/to initialized storage.
 */
typedef void	(*mpo_init_bpfdesc_label_t)(struct label *label);
typedef void	(*mpo_init_cred_label_t)(struct label *label);
typedef void	(*mpo_init_devfs_label_t)(struct label *label);
typedef void	(*mpo_init_ifnet_label_t)(struct label *label);
typedef int	(*mpo_init_inpcb_label_t)(struct label *label, int flag);
typedef void	(*mpo_init_sysv_msgmsg_label_t)(struct label *label);
typedef void	(*mpo_init_sysv_msgqueue_label_t)(struct label *label);
typedef void	(*mpo_init_sysv_sem_label_t)(struct label *label);
typedef void	(*mpo_init_sysv_shm_label_t)(struct label *label);
typedef int	(*mpo_init_ipq_label_t)(struct label *label, int flag);
typedef int	(*mpo_init_mbuf_label_t)(struct label *label, int flag);
typedef void	(*mpo_init_mount_label_t)(struct label *label);
typedef int	(*mpo_init_socket_label_t)(struct label *label, int flag);
typedef int	(*mpo_init_socket_peer_label_t)(struct label *label,
		    int flag);
typedef void	(*mpo_init_pipe_label_t)(struct label *label);
typedef void    (*mpo_init_posix_sem_label_t)(struct label *label);
typedef void	(*mpo_init_proc_label_t)(struct label *label);
typedef void	(*mpo_init_vnode_label_t)(struct label *label);
typedef void	(*mpo_destroy_bpfdesc_label_t)(struct label *label);
typedef void	(*mpo_destroy_cred_label_t)(struct label *label);
typedef void	(*mpo_destroy_devfs_label_t)(struct label *label);
typedef void	(*mpo_destroy_ifnet_label_t)(struct label *label);
typedef void	(*mpo_destroy_inpcb_label_t)(struct label *label);
typedef void	(*mpo_destroy_sysv_msgmsg_label_t)(struct label *label);
typedef void	(*mpo_destroy_sysv_msgqueue_label_t)(struct label *label);
typedef void	(*mpo_destroy_sysv_sem_label_t)(struct label *label);
typedef void	(*mpo_destroy_sysv_shm_label_t)(struct label *label);
typedef void	(*mpo_destroy_ipq_label_t)(struct label *label);
typedef void	(*mpo_destroy_mbuf_label_t)(struct label *label);
typedef void	(*mpo_destroy_mount_label_t)(struct label *label);
typedef void	(*mpo_destroy_socket_label_t)(struct label *label);
typedef void	(*mpo_destroy_socket_peer_label_t)(struct label *label);
typedef void	(*mpo_destroy_pipe_label_t)(struct label *label);
typedef void    (*mpo_destroy_posix_sem_label_t)(struct label *label);
typedef void	(*mpo_destroy_proc_label_t)(struct label *label);
typedef void	(*mpo_destroy_vnode_label_t)(struct label *label);
typedef void	(*mpo_cleanup_sysv_msgmsg_t)(struct label *msglabel);
typedef void	(*mpo_cleanup_sysv_msgqueue_t)(struct label *msqlabel);
typedef void	(*mpo_cleanup_sysv_sem_t)(struct label *semalabel);
typedef void	(*mpo_cleanup_sysv_shm_t)(struct label *shmlabel);
typedef void	(*mpo_copy_cred_label_t)(struct label *src,
		    struct label *dest);
typedef void	(*mpo_copy_ifnet_label_t)(struct label *src,
		    struct label *dest);
typedef void	(*mpo_copy_mbuf_label_t)(struct label *src,
		    struct label *dest);
typedef void	(*mpo_copy_pipe_label_t)(struct label *src,
		    struct label *dest);
typedef void	(*mpo_copy_socket_label_t)(struct label *src,
		    struct label *dest);
typedef void	(*mpo_copy_vnode_label_t)(struct label *src,
		    struct label *dest);
typedef int	(*mpo_externalize_cred_label_t)(struct label *label,
		    char *element_name, struct sbuf *sb, int *claimed);
typedef int	(*mpo_externalize_ifnet_label_t)(struct label *label,
		    char *element_name, struct sbuf *sb, int *claimed);
typedef int	(*mpo_externalize_pipe_label_t)(struct label *label,
		    char *element_name, struct sbuf *sb, int *claimed);
typedef int	(*mpo_externalize_socket_label_t)(struct label *label,
		    char *element_name, struct sbuf *sb, int *claimed);
typedef int	(*mpo_externalize_socket_peer_label_t)(struct label *label,
		    char *element_name, struct sbuf *sb, int *claimed);
typedef int	(*mpo_externalize_vnode_label_t)(struct label *label,
		    char *element_name, struct sbuf *sb, int *claimed);
typedef int	(*mpo_internalize_cred_label_t)(struct label *label,
		    char *element_name, char *element_data, int *claimed);
typedef int	(*mpo_internalize_ifnet_label_t)(struct label *label,
		    char *element_name, char *element_data, int *claimed);
typedef int	(*mpo_internalize_pipe_label_t)(struct label *label,
		    char *element_name, char *element_data, int *claimed);
typedef int	(*mpo_internalize_socket_label_t)(struct label *label,
		    char *element_name, char *element_data, int *claimed);
typedef int	(*mpo_internalize_vnode_label_t)(struct label *label,
		    char *element_name, char *element_data, int *claimed);

/*
 * Labeling event operations: file system objects, and things that look a lot
 * like file system objects.
 */
typedef void	(*mpo_associate_vnode_devfs_t)(struct mount *mp,
		    struct label *mplabel, struct devfs_dirent *de,
		    struct label *delabel, struct vnode *vp,
		    struct label *vplabel);
typedef int	(*mpo_associate_vnode_extattr_t)(struct mount *mp,
		    struct label *mplabel, struct vnode *vp,
		    struct label *vplabel);
typedef void	(*mpo_associate_vnode_singlelabel_t)(struct mount *mp,
		    struct label *mplabel, struct vnode *vp,
		    struct label *vplabel);
typedef void	(*mpo_create_devfs_device_t)(struct ucred *cred,
		    struct mount *mp, struct cdev *dev,
		    struct devfs_dirent *de, struct label *delabel);
typedef void	(*mpo_create_devfs_directory_t)(struct mount *mp,
		    char *dirname, int dirnamelen, struct devfs_dirent *de,
		    struct label *delabel);
typedef void	(*mpo_create_devfs_symlink_t)(struct ucred *cred,
		    struct mount *mp, struct devfs_dirent *dd,
		    struct label *ddlabel, struct devfs_dirent *de,
		    struct label *delabel);
typedef int	(*mpo_create_vnode_extattr_t)(struct ucred *cred,
		    struct mount *mp, struct label *mplabel,
		    struct vnode *dvp, struct label *dvplabel,
		    struct vnode *vp, struct label *vplabel,
		    struct componentname *cnp);
typedef void	(*mpo_create_mount_t)(struct ucred *cred, struct mount *mp,
		    struct label *mplabel);
typedef void	(*mpo_relabel_vnode_t)(struct ucred *cred, struct vnode *vp,
		    struct label *vplabel, struct label *label);
typedef int	(*mpo_setlabel_vnode_extattr_t)(struct ucred *cred,
		    struct vnode *vp, struct label *vplabel,
		    struct label *intlabel);
typedef void	(*mpo_update_devfs_t)(struct mount *mp,
		    struct devfs_dirent *de, struct label *delabel,
		    struct vnode *vp, struct label *vplabel);

/*
 * Labeling event operations: IPC objects.
 */
typedef void	(*mpo_create_mbuf_from_socket_t)(struct socket *so,
		    struct label *solabel, struct mbuf *m,
		    struct label *mlabel);
typedef void	(*mpo_create_socket_t)(struct ucred *cred, struct socket *so,
		    struct label *solabel);
typedef void	(*mpo_create_socket_from_socket_t)(struct socket *oldso,
		    struct label *oldsolabel, struct socket *newso,
		    struct label *newsolabel);
typedef void	(*mpo_relabel_socket_t)(struct ucred *cred, struct socket *so,
		    struct label *oldlabel, struct label *newlabel);
typedef void	(*mpo_relabel_pipe_t)(struct ucred *cred, struct pipepair *pp,
		    struct label *oldlabel, struct label *newlabel);
typedef void	(*mpo_set_socket_peer_from_mbuf_t)(struct mbuf *m,
		    struct label *mlabel, struct socket *so,
		    struct label *sopeerlabel);
typedef void	(*mpo_set_socket_peer_from_socket_t)(struct socket *oldso,
		    struct label *oldsolabel, struct socket *newso,
		    struct label *newsopeerlabel);
typedef void	(*mpo_create_pipe_t)(struct ucred *cred, struct pipepair *pp,
		    struct label *pplabel);

/*
 * Labeling event operations: System V IPC primitives.
 */
typedef void	(*mpo_create_sysv_msgmsg_t)(struct ucred *cred,
		    struct msqid_kernel *msqkptr, struct label *msqlabel,
		    struct msg *msgptr, struct label *msglabel);
typedef void	(*mpo_create_sysv_msgqueue_t)(struct ucred *cred,
		    struct msqid_kernel *msqkptr, struct label *msqlabel);
typedef void	(*mpo_create_sysv_sem_t)(struct ucred *cred,
		    struct semid_kernel *semakptr, struct label *semalabel);
typedef void	(*mpo_create_sysv_shm_t)(struct ucred *cred,
		    struct shmid_kernel *shmsegptr, struct label *shmlabel);

/*
 * Labeling event operations: POSIX (global/inter-process) semaphores.
 */
typedef void	(*mpo_create_posix_sem_t)(struct ucred *cred,
		    struct ksem *ks, struct label *kslabel);

/*
 * Labeling event operations: network objects.
 */
typedef void	(*mpo_create_bpfdesc_t)(struct ucred *cred,
		    struct bpf_d *d, struct label *dlabel);
typedef void	(*mpo_create_ifnet_t)(struct ifnet *ifp,
		    struct label *ifplabel);
typedef void	(*mpo_create_inpcb_from_socket_t)(struct socket *so,
		    struct label *solabel, struct inpcb *inp,
		    struct label *inplabel);
typedef void	(*mpo_create_ipq_t)(struct mbuf *m, struct label *mlabel,
		    struct ipq *ipq, struct label *ipqlabel);
typedef void	(*mpo_create_datagram_from_ipq)
		    (struct ipq *ipq, struct label *ipqlabel, struct mbuf *m,
		    struct label *mlabel);
typedef void	(*mpo_create_fragment_t)(struct mbuf *m,
		    struct label *mlabel, struct mbuf *frag,
		    struct label *fraglabel);
typedef void	(*mpo_create_mbuf_from_inpcb_t)(struct inpcb *inp,
		    struct label *inplabel, struct mbuf *m,
		    struct label *mlabel);
typedef void	(*mpo_create_mbuf_linklayer_t)(struct ifnet *ifp,
		    struct label *ifplabel, struct mbuf *m,
		    struct label *mlabel);
typedef void	(*mpo_create_mbuf_from_bpfdesc_t)(struct bpf_d *d,
		    struct label *dlabel, struct mbuf *m,
		    struct label *mlabel);
typedef void	(*mpo_create_mbuf_from_ifnet_t)(struct ifnet *ifp,
		    struct label *ifplabel, struct mbuf *m,
		    struct label *mlabel);
typedef void	(*mpo_create_mbuf_multicast_encap_t)(struct mbuf *m,
		    struct label *mlabel, struct ifnet *ifp,
		    struct label *ifplabel, struct mbuf *mnew,
		    struct label *mnewlabel);
typedef void	(*mpo_create_mbuf_netlayer_t)(struct mbuf *m,
		    struct label *mlabel, struct mbuf *mnew,
		    struct label *mnewlabel);
typedef int	(*mpo_fragment_match_t)(struct mbuf *m, struct label *mlabel,
		    struct ipq *ipq, struct label *ipqlabel);
typedef void	(*mpo_reflect_mbuf_icmp_t)(struct mbuf *m,
		    struct label *mlabel);
typedef void	(*mpo_reflect_mbuf_tcp_t)(struct mbuf *m,
		    struct label *mlabel);
typedef void	(*mpo_relabel_ifnet_t)(struct ucred *cred, struct ifnet *ifp,
		    struct label *ifplabel, struct label *newlabel);
typedef void	(*mpo_update_ipq_t)(struct mbuf *m, struct label *mlabel,
		    struct ipq *ipq, struct label *ipqlabel);
typedef void	(*mpo_inpcb_sosetlabel_t)(struct socket *so,
		    struct label *label, struct inpcb *inp,
		    struct label *inplabel);

typedef	void	(*mpo_create_mbuf_from_firewall_t)(struct mbuf *m,
		    struct label *label);
typedef void	(*mpo_destroy_syncache_label_t)(struct label *label);
typedef int	(*mpo_init_syncache_label_t)(struct label *label, int flag);
typedef void	(*mpo_init_syncache_from_inpcb_t)(struct label *label,
		    struct inpcb *inp);
typedef void	(*mpo_create_mbuf_from_syncache_t)(struct label *sc_label,
		    struct mbuf *m, struct label *mlabel);
/*
 * Labeling event operations: processes.
 */
typedef void	(*mpo_execve_transition_t)(struct ucred *old,
		    struct ucred *new, struct vnode *vp,
		    struct label *vplabel, struct label *interpvnodelabel,
		    struct image_params *imgp, struct label *execlabel);
typedef int	(*mpo_execve_will_transition_t)(struct ucred *old,
		    struct vnode *vp, struct label *vplabel,
		    struct label *interpvnodelabel,
		    struct image_params *imgp, struct label *execlabel);
typedef void	(*mpo_create_proc0_t)(struct ucred *cred);
typedef void	(*mpo_create_proc1_t)(struct ucred *cred);
typedef void	(*mpo_relabel_cred_t)(struct ucred *cred,
		    struct label *newlabel);
typedef void	(*mpo_thread_userret_t)(struct thread *thread);

/*
 * Access control checks.
 */
typedef	int	(*mpo_check_bpfdesc_receive_t)(struct bpf_d *d,
		    struct label *dlabel, struct ifnet *ifp,
		    struct label *ifplabel);
typedef int	(*mpo_check_cred_relabel_t)(struct ucred *cred,
		    struct label *newlabel);
typedef int	(*mpo_check_cred_visible_t)(struct ucred *cr1,
		    struct ucred *cr2);
typedef int	(*mpo_check_ifnet_relabel_t)(struct ucred *cred,
		    struct ifnet *ifp, struct label *ifplabel,
		    struct label *newlabel);
typedef int	(*mpo_check_ifnet_transmit_t)(struct ifnet *ifp,
		    struct label *ifplabel, struct mbuf *m,
		    struct label *mlabel);
typedef int	(*mpo_check_inpcb_deliver_t)(struct inpcb *inp,
		    struct label *inplabel, struct mbuf *m,
		    struct label *mlabel);
typedef int	(*mpo_check_sysv_msgmsq_t)(struct ucred *cred,
		    struct msg *msgptr, struct label *msglabel,
		    struct msqid_kernel *msqkptr, struct label *msqklabel);
typedef int	(*mpo_check_sysv_msgrcv_t)(struct ucred *cred,
		    struct msg *msgptr, struct label *msglabel);
typedef int	(*mpo_check_sysv_msgrmid_t)(struct ucred *cred,
		    struct msg *msgptr, struct label *msglabel);
typedef int	(*mpo_check_sysv_msqget_t)(struct ucred *cred,
		    struct msqid_kernel *msqkptr, struct label *msqklabel);
typedef int	(*mpo_check_sysv_msqsnd_t)(struct ucred *cred,
		    struct msqid_kernel *msqkptr, struct label *msqklabel);
typedef int	(*mpo_check_sysv_msqrcv_t)(struct ucred *cred,
		    struct msqid_kernel *msqkptr, struct label *msqklabel);
typedef int	(*mpo_check_sysv_msqctl_t)(struct ucred *cred,
		    struct msqid_kernel *msqkptr, struct label *msqklabel,
		    int cmd);
typedef int	(*mpo_check_sysv_semctl_t)(struct ucred *cred,
		    struct semid_kernel *semakptr, struct label *semaklabel,
		    int cmd);
typedef int	(*mpo_check_sysv_semget_t)(struct ucred *cred,
		    struct semid_kernel *semakptr, struct label *semaklabel);
typedef int	(*mpo_check_sysv_semop_t)(struct ucred *cred,
		    struct semid_kernel *semakptr, struct label *semaklabel,
		    size_t accesstype);
typedef int	(*mpo_check_sysv_shmat_t)(struct ucred *cred,
		    struct shmid_kernel *shmsegptr,
		    struct label *shmseglabel, int shmflg);
typedef int	(*mpo_check_sysv_shmctl_t)(struct ucred *cred,
		    struct shmid_kernel *shmsegptr,
		    struct label *shmseglabel, int cmd);
typedef int	(*mpo_check_sysv_shmdt_t)(struct ucred *cred,
		    struct shmid_kernel *shmsegptr,
		    struct label *shmseglabel);
typedef int	(*mpo_check_sysv_shmget_t)(struct ucred *cred,
		    struct shmid_kernel *shmsegptr,
		    struct label *shmseglabel, int shmflg);
typedef int	(*mpo_check_kenv_dump_t)(struct ucred *cred);
typedef int	(*mpo_check_kenv_get_t)(struct ucred *cred, char *name);
typedef int	(*mpo_check_kenv_set_t)(struct ucred *cred, char *name,
		    char *value);
typedef int	(*mpo_check_kenv_unset_t)(struct ucred *cred, char *name);
typedef int	(*mpo_check_kld_load_t)(struct ucred *cred, struct vnode *vp,
		    struct label *vplabel);
typedef int	(*mpo_check_kld_stat_t)(struct ucred *cred);
typedef int	(*mpo_mpo_placeholder19_t)(void);
typedef int	(*mpo_mpo_placeholder20_t)(void);
typedef int	(*mpo_check_mount_stat_t)(struct ucred *cred,
		    struct mount *mp, struct label *mplabel);
typedef int	(*mpo_mpo_placeholder21_t)(void);
typedef int	(*mpo_check_pipe_ioctl_t)(struct ucred *cred,
		    struct pipepair *pp, struct label *pplabel,
		    unsigned long cmd, void *data);
typedef int	(*mpo_check_pipe_poll_t)(struct ucred *cred,
		    struct pipepair *pp, struct label *pplabel);
typedef int	(*mpo_check_pipe_read_t)(struct ucred *cred,
		    struct pipepair *pp, struct label *pplabel);
typedef int	(*mpo_check_pipe_relabel_t)(struct ucred *cred,
		    struct pipepair *pp, struct label *pplabel,
		    struct label *newlabel);
typedef int	(*mpo_check_pipe_stat_t)(struct ucred *cred,
		    struct pipepair *pp, struct label *pplabel);
typedef int	(*mpo_check_pipe_write_t)(struct ucred *cred,
		    struct pipepair *pp, struct label *pplabel);
typedef int	(*mpo_check_posix_sem_destroy_t)(struct ucred *cred,
		    struct ksem *ks, struct label *kslabel);
typedef int	(*mpo_check_posix_sem_getvalue_t)(struct ucred *cred,
		    struct ksem *ks, struct label *kslabel);
typedef int	(*mpo_check_posix_sem_open_t)(struct ucred *cred,
		    struct ksem *ks, struct label *kslabel);
typedef int	(*mpo_check_posix_sem_post_t)(struct ucred *cred,
		    struct ksem *ks, struct label *kslabel);
typedef int	(*mpo_check_posix_sem_unlink_t)(struct ucred *cred,
		    struct ksem *ks, struct label *kslabel);
typedef int	(*mpo_check_posix_sem_wait_t)(struct ucred *cred,
		    struct ksem *ks, struct label *kslabel);
typedef int	(*mpo_check_proc_debug_t)(struct ucred *cred,
		    struct proc *p);
typedef int	(*mpo_check_proc_sched_t)(struct ucred *cred,
		    struct proc *p);
typedef int	(*mpo_check_proc_setaudit_t)(struct ucred *cred,
		    struct auditinfo *ai);
typedef int	(*mpo_check_proc_setaudit_addr_t)(struct ucred *cred,
		    struct auditinfo_addr *aia);
typedef int	(*mpo_check_proc_setauid_t)(struct ucred *cred, uid_t auid);
typedef int	(*mpo_check_proc_setuid_t)(struct ucred *cred, uid_t uid);
typedef int	(*mpo_check_proc_seteuid_t)(struct ucred *cred, uid_t euid);
typedef int	(*mpo_check_proc_setgid_t)(struct ucred *cred, gid_t gid);
typedef int	(*mpo_check_proc_setegid_t)(struct ucred *cred, gid_t egid);
typedef int	(*mpo_check_proc_setgroups_t)(struct ucred *cred, int ngroups,
		    gid_t *gidset);
typedef int	(*mpo_check_proc_setreuid_t)(struct ucred *cred, uid_t ruid,
		    uid_t euid);
typedef int	(*mpo_check_proc_setregid_t)(struct ucred *cred, gid_t rgid,
		    gid_t egid);
typedef int	(*mpo_check_proc_setresuid_t)(struct ucred *cred, uid_t ruid,
		    uid_t euid, uid_t suid);
typedef int	(*mpo_check_proc_setresgid_t)(struct ucred *cred, gid_t rgid,
		    gid_t egid, gid_t sgid);
typedef int	(*mpo_check_proc_signal_t)(struct ucred *cred,
		    struct proc *proc, int signum);
typedef int	(*mpo_check_proc_wait_t)(struct ucred *cred,
		    struct proc *proc);
typedef int	(*mpo_check_socket_accept_t)(struct ucred *cred,
		    struct socket *so, struct label *solabel);
typedef int	(*mpo_check_socket_bind_t)(struct ucred *cred,
		    struct socket *so, struct label *solabel,
		    struct sockaddr *sa);
typedef int	(*mpo_check_socket_connect_t)(struct ucred *cred,
		    struct socket *so, struct label *solabel,
		    struct sockaddr *sa);
typedef int	(*mpo_check_socket_create_t)(struct ucred *cred, int domain,
		    int type, int protocol);
typedef int	(*mpo_check_socket_deliver_t)(struct socket *so,
		    struct label *solabel, struct mbuf *m,
		    struct label *mlabel);
typedef int	(*mpo_check_socket_listen_t)(struct ucred *cred,
		    struct socket *so, struct label *solabel);
typedef int	(*mpo_check_socket_poll_t)(struct ucred *cred,
		    struct socket *so, struct label *solabel);
typedef int	(*mpo_check_socket_receive_t)(struct ucred *cred,
		    struct socket *so, struct label *solabel);
typedef int	(*mpo_check_socket_relabel_t)(struct ucred *cred,
		    struct socket *so, struct label *solabel,
		    struct label *newlabel);
typedef int	(*mpo_check_socket_send_t)(struct ucred *cred,
		    struct socket *so, struct label *solabel);
typedef int	(*mpo_check_socket_stat_t)(struct ucred *cred,
		    struct socket *so, struct label *solabel);
typedef int	(*mpo_check_socket_visible_t)(struct ucred *cred,
		    struct socket *so, struct label *solabel);
typedef int	(*mpo_check_system_acct_t)(struct ucred *cred,
		    struct vnode *vp, struct label *vplabel);
typedef int	(*mpo_check_system_audit_t)(struct ucred *cred, void *record,
		    int length);
typedef int	(*mpo_check_system_auditctl_t)(struct ucred *cred,
		    struct vnode *vp, struct label *vplabel);
typedef int	(*mpo_check_system_auditon_t)(struct ucred *cred, int cmd);
typedef int	(*mpo_check_system_reboot_t)(struct ucred *cred, int howto);
typedef int	(*mpo_check_system_swapon_t)(struct ucred *cred,
		    struct vnode *vp, struct label *vplabel);
typedef int	(*mpo_check_system_swapoff_t)(struct ucred *cred,
		    struct vnode *vp, struct label *vplabel);
typedef int	(*mpo_check_system_sysctl_t)(struct ucred *cred,
		    struct sysctl_oid *oidp, void *arg1, int arg2,
		    struct sysctl_req *req);
typedef int	(*mpo_check_vnode_access_t)(struct ucred *cred,
		    struct vnode *vp, struct label *vplabel, int acc_mode);
typedef int	(*mpo_check_vnode_chdir_t)(struct ucred *cred,
		    struct vnode *dvp, struct label *dvplabel);
typedef int	(*mpo_check_vnode_chroot_t)(struct ucred *cred,
		    struct vnode *dvp, struct label *dvplabel);
typedef int	(*mpo_check_vnode_create_t)(struct ucred *cred,
		    struct vnode *dvp, struct label *dvplabel,
		    struct componentname *cnp, struct vattr *vap);
typedef int	(*mpo_check_vnode_deleteacl_t)(struct ucred *cred,
		    struct vnode *vp, struct label *vplabel,
		    acl_type_t type);
typedef int	(*mpo_check_vnode_deleteextattr_t)(struct ucred *cred,
		    struct vnode *vp, struct label *vplabel,
		    int attrnamespace, const char *name);
typedef int	(*mpo_check_vnode_exec_t)(struct ucred *cred,
		    struct vnode *vp, struct label *vplabel,
		    struct image_params *imgp, struct label *execlabel);
typedef int	(*mpo_check_vnode_getacl_t)(struct ucred *cred,
		    struct vnode *vp, struct label *vplabel,
		    acl_type_t type);
typedef int	(*mpo_check_vnode_getextattr_t)(struct ucred *cred,
		    struct vnode *vp, struct label *vplabel,
		    int attrnamespace, const char *name, struct uio *uio);
typedef int	(*mpo_check_vnode_link_t)(struct ucred *cred,
		    struct vnode *dvp, struct label *dvplabel,
		    struct vnode *vp, struct label *vplabel,
		    struct componentname *cnp);
typedef int	(*mpo_check_vnode_listextattr_t)(struct ucred *cred,
		    struct vnode *vp, struct label *vplabel,
		    int attrnamespace);
typedef int	(*mpo_check_vnode_lookup_t)(struct ucred *cred,
		    struct vnode *dvp, struct label *dvplabel,
		    struct componentname *cnp);
typedef int	(*mpo_check_vnode_mmap_t)(struct ucred *cred,
		    struct vnode *vp, struct label *label, int prot,
		    int flags);
typedef void	(*mpo_check_vnode_mmap_downgrade_t)(struct ucred *cred,
		    struct vnode *vp, struct label *vplabel, int *prot);
typedef int	(*mpo_check_vnode_mprotect_t)(struct ucred *cred,
		    struct vnode *vp, struct label *vplabel, int prot);
typedef int	(*mpo_check_vnode_open_t)(struct ucred *cred,
		    struct vnode *vp, struct label *vplabel, int acc_mode);
typedef int	(*mpo_check_vnode_poll_t)(struct ucred *active_cred,
		    struct ucred *file_cred, struct vnode *vp,
		    struct label *vplabel);
typedef int	(*mpo_check_vnode_read_t)(struct ucred *active_cred,
		    struct ucred *file_cred, struct vnode *vp,
		    struct label *vplabel);
typedef int	(*mpo_check_vnode_readdir_t)(struct ucred *cred,
		    struct vnode *dvp, struct label *dvplabel);
typedef int	(*mpo_check_vnode_readlink_t)(struct ucred *cred,
		    struct vnode *vp, struct label *vplabel);
typedef int	(*mpo_check_vnode_relabel_t)(struct ucred *cred,
		    struct vnode *vp, struct label *vplabel,
		    struct label *newlabel);
typedef int	(*mpo_check_vnode_rename_from_t)(struct ucred *cred,
		    struct vnode *dvp, struct label *dvplabel,
		    struct vnode *vp, struct label *vplabel,
		    struct componentname *cnp);
typedef int	(*mpo_check_vnode_rename_to_t)(struct ucred *cred,
		    struct vnode *dvp, struct label *dvplabel,
		    struct vnode *vp, struct label *vplabel, int samedir,
		    struct componentname *cnp);
typedef int	(*mpo_check_vnode_revoke_t)(struct ucred *cred,
		    struct vnode *vp, struct label *vplabel);
typedef int	(*mpo_check_vnode_setacl_t)(struct ucred *cred,
		    struct vnode *vp, struct label *vplabel, acl_type_t type,
		    struct acl *acl);
typedef int	(*mpo_check_vnode_setextattr_t)(struct ucred *cred,
		    struct vnode *vp, struct label *vplabel,
		    int attrnamespace, const char *name, struct uio *uio);
typedef int	(*mpo_check_vnode_setflags_t)(struct ucred *cred,
		    struct vnode *vp, struct label *vplabel, u_long flags);
typedef int	(*mpo_check_vnode_setmode_t)(struct ucred *cred,
		    struct vnode *vp, struct label *vplabel, mode_t mode);
typedef int	(*mpo_check_vnode_setowner_t)(struct ucred *cred,
		    struct vnode *vp, struct label *vplabel, uid_t uid,
		    gid_t gid);
typedef int	(*mpo_check_vnode_setutimes_t)(struct ucred *cred,
		    struct vnode *vp, struct label *vplabel,
		    struct timespec atime, struct timespec mtime);
typedef int	(*mpo_check_vnode_stat_t)(struct ucred *active_cred,
		    struct ucred *file_cred, struct vnode *vp,
		    struct label *vplabel);
typedef int	(*mpo_check_vnode_unlink_t)(struct ucred *cred,
		    struct vnode *dvp, struct label *dvplabel,
		    struct vnode *vp, struct label *vplabel,
		    struct componentname *cnp);
typedef int	(*mpo_check_vnode_write_t)(struct ucred *active_cred,
		    struct ucred *file_cred, struct vnode *vp,
		    struct label *vplabel);
typedef void	(*mpo_associate_nfsd_label_t)(struct ucred *cred);
typedef int	(*mpo_priv_check_t)(struct ucred *cred, int priv);
typedef int	(*mpo_priv_grant_t)(struct ucred *cred, int priv);

struct mac_policy_ops {
	/*
	 * Policy module operations.
	 */
	mpo_destroy_t				mpo_destroy;
	mpo_init_t				mpo_init;

	/*
	 * General policy-directed security system call so that policies may
	 * implement new services without reserving explicit system call
	 * numbers.
	 */
	mpo_syscall_t				mpo_syscall;

	/*
	 * Label operations.  Initialize label storage, destroy label
	 * storage, recycle for re-use without init/destroy, copy a label to
	 * initialized storage, and externalize/internalize from/to
	 * initialized storage.
	 */
	mpo_init_bpfdesc_label_t		mpo_init_bpfdesc_label;
	mpo_init_cred_label_t			mpo_init_cred_label;
	mpo_init_devfs_label_t			mpo_init_devfs_label;
	mpo_placeholder_t			_mpo_placeholder0;
	mpo_init_ifnet_label_t			mpo_init_ifnet_label;
	mpo_init_inpcb_label_t			mpo_init_inpcb_label;
	mpo_init_sysv_msgmsg_label_t		mpo_init_sysv_msgmsg_label;
	mpo_init_sysv_msgqueue_label_t		mpo_init_sysv_msgqueue_label;
	mpo_init_sysv_sem_label_t		mpo_init_sysv_sem_label;
	mpo_init_sysv_shm_label_t		mpo_init_sysv_shm_label;
	mpo_init_ipq_label_t			mpo_init_ipq_label;
	mpo_init_mbuf_label_t			mpo_init_mbuf_label;
	mpo_init_mount_label_t			mpo_init_mount_label;
	mpo_init_socket_label_t			mpo_init_socket_label;
	mpo_init_socket_peer_label_t		mpo_init_socket_peer_label;
	mpo_init_pipe_label_t			mpo_init_pipe_label;
	mpo_init_posix_sem_label_t		mpo_init_posix_sem_label;
	mpo_init_proc_label_t			mpo_init_proc_label;
	mpo_init_vnode_label_t			mpo_init_vnode_label;
	mpo_destroy_bpfdesc_label_t		mpo_destroy_bpfdesc_label;
	mpo_destroy_cred_label_t		mpo_destroy_cred_label;
	mpo_destroy_devfs_label_t		mpo_destroy_devfs_label;
	mpo_placeholder_t			_mpo_placeholder1;
	mpo_destroy_ifnet_label_t		mpo_destroy_ifnet_label;
	mpo_destroy_inpcb_label_t		mpo_destroy_inpcb_label;
	mpo_destroy_sysv_msgmsg_label_t		mpo_destroy_sysv_msgmsg_label;
	mpo_destroy_sysv_msgqueue_label_t	mpo_destroy_sysv_msgqueue_label;
	mpo_destroy_sysv_sem_label_t		mpo_destroy_sysv_sem_label;
	mpo_destroy_sysv_shm_label_t		mpo_destroy_sysv_shm_label;
	mpo_destroy_ipq_label_t			mpo_destroy_ipq_label;
	mpo_destroy_mbuf_label_t		mpo_destroy_mbuf_label;
	mpo_destroy_mount_label_t		mpo_destroy_mount_label;
	mpo_destroy_socket_label_t		mpo_destroy_socket_label;
	mpo_destroy_socket_peer_label_t		mpo_destroy_socket_peer_label;
	mpo_destroy_pipe_label_t		mpo_destroy_pipe_label;
	mpo_destroy_posix_sem_label_t		mpo_destroy_posix_sem_label;
	mpo_destroy_proc_label_t		mpo_destroy_proc_label;
	mpo_destroy_vnode_label_t		mpo_destroy_vnode_label;
	mpo_cleanup_sysv_msgmsg_t		mpo_cleanup_sysv_msgmsg;
	mpo_cleanup_sysv_msgqueue_t		mpo_cleanup_sysv_msgqueue;
	mpo_cleanup_sysv_sem_t			mpo_cleanup_sysv_sem;
	mpo_cleanup_sysv_shm_t			mpo_cleanup_sysv_shm;
	mpo_copy_cred_label_t			mpo_copy_cred_label;
	mpo_copy_ifnet_label_t			mpo_copy_ifnet_label;
	mpo_copy_mbuf_label_t			mpo_copy_mbuf_label;
	mpo_placeholder_t			_mpo_placeholder2;
	mpo_copy_pipe_label_t			mpo_copy_pipe_label;
	mpo_copy_socket_label_t			mpo_copy_socket_label;
	mpo_copy_vnode_label_t			mpo_copy_vnode_label;
	mpo_externalize_cred_label_t		mpo_externalize_cred_label;
	mpo_externalize_ifnet_label_t		mpo_externalize_ifnet_label;
	mpo_placeholder_t			_mpo_placeholder3;
	mpo_externalize_pipe_label_t		mpo_externalize_pipe_label;
	mpo_externalize_socket_label_t		mpo_externalize_socket_label;
	mpo_externalize_socket_peer_label_t	mpo_externalize_socket_peer_label;
	mpo_externalize_vnode_label_t		mpo_externalize_vnode_label;
	mpo_internalize_cred_label_t		mpo_internalize_cred_label;
	mpo_internalize_ifnet_label_t		mpo_internalize_ifnet_label;
	mpo_placeholder_t			_mpo_placeholder4;
	mpo_internalize_pipe_label_t		mpo_internalize_pipe_label;
	mpo_internalize_socket_label_t		mpo_internalize_socket_label;
	mpo_internalize_vnode_label_t		mpo_internalize_vnode_label;

	/*
	 * Labeling event operations: file system objects, and things that
	 * look a lot like file system objects.
	 */
	mpo_associate_vnode_devfs_t		mpo_associate_vnode_devfs;
	mpo_associate_vnode_extattr_t		mpo_associate_vnode_extattr;
	mpo_associate_vnode_singlelabel_t	mpo_associate_vnode_singlelabel;
	mpo_create_devfs_device_t		mpo_create_devfs_device;
	mpo_create_devfs_directory_t		mpo_create_devfs_directory;
	mpo_create_devfs_symlink_t		mpo_create_devfs_symlink;
	mpo_placeholder_t			_mpo_placeholder5;
	mpo_create_vnode_extattr_t		mpo_create_vnode_extattr;
	mpo_create_mount_t			mpo_create_mount;
	mpo_relabel_vnode_t			mpo_relabel_vnode;
	mpo_setlabel_vnode_extattr_t		mpo_setlabel_vnode_extattr;
	mpo_update_devfs_t			mpo_update_devfs;

	/*
	 * Labeling event operations: IPC objects.
	 */
	mpo_create_mbuf_from_socket_t		mpo_create_mbuf_from_socket;
	mpo_create_socket_t			mpo_create_socket;
	mpo_create_socket_from_socket_t		mpo_create_socket_from_socket;
	mpo_relabel_socket_t			mpo_relabel_socket;
	mpo_relabel_pipe_t			mpo_relabel_pipe;
	mpo_set_socket_peer_from_mbuf_t		mpo_set_socket_peer_from_mbuf;
	mpo_set_socket_peer_from_socket_t	mpo_set_socket_peer_from_socket;
	mpo_create_pipe_t			mpo_create_pipe;

	/*
	 * Labeling event operations: System V IPC primitives.
	 */
	mpo_create_sysv_msgmsg_t		mpo_create_sysv_msgmsg;
	mpo_create_sysv_msgqueue_t		mpo_create_sysv_msgqueue;
	mpo_create_sysv_sem_t			mpo_create_sysv_sem;
	mpo_create_sysv_shm_t			mpo_create_sysv_shm;

	/*
	 * Labeling event operations: POSIX (global/inter-process) semaphores.
	 */
	mpo_create_posix_sem_t			mpo_create_posix_sem;

	/*
	 * Labeling event operations: network objects.
	 */
	mpo_create_bpfdesc_t			mpo_create_bpfdesc;
	mpo_create_ifnet_t			mpo_create_ifnet;
	mpo_create_inpcb_from_socket_t		mpo_create_inpcb_from_socket;
	mpo_create_ipq_t			mpo_create_ipq;
	mpo_create_datagram_from_ipq		mpo_create_datagram_from_ipq;
	mpo_create_fragment_t			mpo_create_fragment;
	mpo_create_mbuf_from_inpcb_t		mpo_create_mbuf_from_inpcb;
	mpo_create_mbuf_linklayer_t		mpo_create_mbuf_linklayer;
	mpo_create_mbuf_from_bpfdesc_t		mpo_create_mbuf_from_bpfdesc;
	mpo_create_mbuf_from_ifnet_t		mpo_create_mbuf_from_ifnet;
	mpo_create_mbuf_multicast_encap_t	mpo_create_mbuf_multicast_encap;
	mpo_create_mbuf_netlayer_t		mpo_create_mbuf_netlayer;
	mpo_fragment_match_t			mpo_fragment_match;
	mpo_reflect_mbuf_icmp_t			mpo_reflect_mbuf_icmp;
	mpo_reflect_mbuf_tcp_t			mpo_reflect_mbuf_tcp;
	mpo_relabel_ifnet_t			mpo_relabel_ifnet;
	mpo_update_ipq_t			mpo_update_ipq;
	mpo_inpcb_sosetlabel_t			mpo_inpcb_sosetlabel;

	/*
	 * Labeling event operations: processes.
	 */
	mpo_execve_transition_t			mpo_execve_transition;
	mpo_execve_will_transition_t		mpo_execve_will_transition;
	mpo_create_proc0_t			mpo_create_proc0;
	mpo_create_proc1_t			mpo_create_proc1;
	mpo_relabel_cred_t			mpo_relabel_cred;
	mpo_placeholder_t			_mpo_placeholder6;
	mpo_thread_userret_t			mpo_thread_userret;

	/*
	 * Access control checks.
	 */
	mpo_check_bpfdesc_receive_t		mpo_check_bpfdesc_receive;
	mpo_placeholder_t			_mpo_placeholder7;
	mpo_check_cred_relabel_t		mpo_check_cred_relabel;
	mpo_check_cred_visible_t		mpo_check_cred_visible;
	mpo_placeholder_t			_mpo_placeholder8;
	mpo_placeholder_t			_mpo_placeholder9;
	mpo_placeholder_t			_mpo_placeholder10;
	mpo_placeholder_t			_mpo_placeholder11;
	mpo_placeholder_t			_mpo_placeholder12;
	mpo_placeholder_t			_mpo_placeholder13;
	mpo_placeholder_t			_mpo_placeholder14;
	mpo_placeholder_t			_mpo_placeholder15;
	mpo_placeholder_t			_mpo_placeholder16;
	mpo_placeholder_t			_mpo_placeholder17;
	mpo_placeholder_t			_mpo_placeholder18;
	mpo_check_ifnet_relabel_t		mpo_check_ifnet_relabel;
	mpo_check_ifnet_transmit_t		mpo_check_ifnet_transmit;
	mpo_check_inpcb_deliver_t		mpo_check_inpcb_deliver;
	mpo_check_sysv_msgmsq_t			mpo_check_sysv_msgmsq;
	mpo_check_sysv_msgrcv_t			mpo_check_sysv_msgrcv;
	mpo_check_sysv_msgrmid_t		mpo_check_sysv_msgrmid;
	mpo_check_sysv_msqget_t			mpo_check_sysv_msqget;
	mpo_check_sysv_msqsnd_t			mpo_check_sysv_msqsnd;
	mpo_check_sysv_msqrcv_t			mpo_check_sysv_msqrcv;
	mpo_check_sysv_msqctl_t			mpo_check_sysv_msqctl;
	mpo_check_sysv_semctl_t			mpo_check_sysv_semctl;
	mpo_check_sysv_semget_t			mpo_check_sysv_semget;
	mpo_check_sysv_semop_t			mpo_check_sysv_semop;
	mpo_check_sysv_shmat_t			mpo_check_sysv_shmat;
	mpo_check_sysv_shmctl_t			mpo_check_sysv_shmctl;
	mpo_check_sysv_shmdt_t			mpo_check_sysv_shmdt;
	mpo_check_sysv_shmget_t			mpo_check_sysv_shmget;
	mpo_check_kenv_dump_t			mpo_check_kenv_dump;
	mpo_check_kenv_get_t			mpo_check_kenv_get;
	mpo_check_kenv_set_t			mpo_check_kenv_set;
	mpo_check_kenv_unset_t			mpo_check_kenv_unset;
	mpo_check_kld_load_t			mpo_check_kld_load;
	mpo_check_kld_stat_t			mpo_check_kld_stat;
	mpo_placeholder_t			_mpo_placeholder19;
	mpo_placeholder_t			_mpo_placeholder20;
	mpo_check_mount_stat_t			mpo_check_mount_stat;
	mpo_placeholder_t			_mpo_placeholder_21;
	mpo_check_pipe_ioctl_t			mpo_check_pipe_ioctl;
	mpo_check_pipe_poll_t			mpo_check_pipe_poll;
	mpo_check_pipe_read_t			mpo_check_pipe_read;
	mpo_check_pipe_relabel_t		mpo_check_pipe_relabel;
	mpo_check_pipe_stat_t			mpo_check_pipe_stat;
	mpo_check_pipe_write_t			mpo_check_pipe_write;
	mpo_check_posix_sem_destroy_t		mpo_check_posix_sem_destroy;
	mpo_check_posix_sem_getvalue_t		mpo_check_posix_sem_getvalue;
	mpo_check_posix_sem_open_t		mpo_check_posix_sem_open;
	mpo_check_posix_sem_post_t		mpo_check_posix_sem_post;
	mpo_check_posix_sem_unlink_t		mpo_check_posix_sem_unlink;
	mpo_check_posix_sem_wait_t		mpo_check_posix_sem_wait;
	mpo_check_proc_debug_t			mpo_check_proc_debug;
	mpo_check_proc_sched_t			mpo_check_proc_sched;
	mpo_check_proc_setaudit_t		mpo_check_proc_setaudit;
	mpo_check_proc_setaudit_addr_t		mpo_check_proc_setaudit_addr;
	mpo_check_proc_setauid_t		mpo_check_proc_setauid;
	mpo_check_proc_setuid_t			mpo_check_proc_setuid;
	mpo_check_proc_seteuid_t		mpo_check_proc_seteuid;
	mpo_check_proc_setgid_t			mpo_check_proc_setgid;
	mpo_check_proc_setegid_t		mpo_check_proc_setegid;
	mpo_check_proc_setgroups_t		mpo_check_proc_setgroups;
	mpo_check_proc_setreuid_t		mpo_check_proc_setreuid;
	mpo_check_proc_setregid_t		mpo_check_proc_setregid;
	mpo_check_proc_setresuid_t		mpo_check_proc_setresuid;
	mpo_check_proc_setresgid_t		mpo_check_proc_setresgid;
	mpo_check_proc_signal_t			mpo_check_proc_signal;
	mpo_check_proc_wait_t			mpo_check_proc_wait;
	mpo_check_socket_accept_t		mpo_check_socket_accept;
	mpo_check_socket_bind_t			mpo_check_socket_bind;
	mpo_check_socket_connect_t		mpo_check_socket_connect;
	mpo_check_socket_create_t		mpo_check_socket_create;
	mpo_check_socket_deliver_t		mpo_check_socket_deliver;
	mpo_placeholder_t			_mpo_placeholder22;
	mpo_check_socket_listen_t		mpo_check_socket_listen;
	mpo_check_socket_poll_t			mpo_check_socket_poll;
	mpo_check_socket_receive_t		mpo_check_socket_receive;
	mpo_check_socket_relabel_t		mpo_check_socket_relabel;
	mpo_check_socket_send_t			mpo_check_socket_send;
	mpo_check_socket_stat_t			mpo_check_socket_stat;
	mpo_check_socket_visible_t		mpo_check_socket_visible;
	mpo_check_system_acct_t			mpo_check_system_acct;
	mpo_check_system_audit_t		mpo_check_system_audit;
	mpo_check_system_auditctl_t		mpo_check_system_auditctl;
	mpo_check_system_auditon_t		mpo_check_system_auditon;
	mpo_check_system_reboot_t		mpo_check_system_reboot;
	mpo_check_system_swapon_t		mpo_check_system_swapon;
	mpo_check_system_swapoff_t		mpo_check_system_swapoff;
	mpo_check_system_sysctl_t		mpo_check_system_sysctl;
	mpo_placeholder_t			_mpo_placeholder23;
	mpo_check_vnode_access_t		mpo_check_vnode_access;
	mpo_check_vnode_chdir_t			mpo_check_vnode_chdir;
	mpo_check_vnode_chroot_t		mpo_check_vnode_chroot;
	mpo_check_vnode_create_t		mpo_check_vnode_create;
	mpo_check_vnode_deleteacl_t		mpo_check_vnode_deleteacl;
	mpo_check_vnode_deleteextattr_t		mpo_check_vnode_deleteextattr;
	mpo_check_vnode_exec_t			mpo_check_vnode_exec;
	mpo_check_vnode_getacl_t		mpo_check_vnode_getacl;
	mpo_check_vnode_getextattr_t		mpo_check_vnode_getextattr;
	mpo_placeholder_t			_mpo_placeholder24;
	mpo_check_vnode_link_t			mpo_check_vnode_link;
	mpo_check_vnode_listextattr_t		mpo_check_vnode_listextattr;
	mpo_check_vnode_lookup_t		mpo_check_vnode_lookup;
	mpo_check_vnode_mmap_t			mpo_check_vnode_mmap;
	mpo_check_vnode_mmap_downgrade_t	mpo_check_vnode_mmap_downgrade;
	mpo_check_vnode_mprotect_t		mpo_check_vnode_mprotect;
	mpo_check_vnode_open_t			mpo_check_vnode_open;
	mpo_check_vnode_poll_t			mpo_check_vnode_poll;
	mpo_check_vnode_read_t			mpo_check_vnode_read;
	mpo_check_vnode_readdir_t		mpo_check_vnode_readdir;
	mpo_check_vnode_readlink_t		mpo_check_vnode_readlink;
	mpo_check_vnode_relabel_t		mpo_check_vnode_relabel;
	mpo_check_vnode_rename_from_t		mpo_check_vnode_rename_from;
	mpo_check_vnode_rename_to_t		mpo_check_vnode_rename_to;
	mpo_check_vnode_revoke_t		mpo_check_vnode_revoke;
	mpo_check_vnode_setacl_t		mpo_check_vnode_setacl;
	mpo_check_vnode_setextattr_t		mpo_check_vnode_setextattr;
	mpo_check_vnode_setflags_t		mpo_check_vnode_setflags;
	mpo_check_vnode_setmode_t		mpo_check_vnode_setmode;
	mpo_check_vnode_setowner_t		mpo_check_vnode_setowner;
	mpo_check_vnode_setutimes_t		mpo_check_vnode_setutimes;
	mpo_check_vnode_stat_t			mpo_check_vnode_stat;
	mpo_check_vnode_unlink_t		mpo_check_vnode_unlink;
	mpo_check_vnode_write_t			mpo_check_vnode_write;
	mpo_associate_nfsd_label_t		mpo_associate_nfsd_label;
	mpo_create_mbuf_from_firewall_t		mpo_create_mbuf_from_firewall;
	mpo_init_syncache_label_t		mpo_init_syncache_label;
	mpo_destroy_syncache_label_t		mpo_destroy_syncache_label;
	mpo_init_syncache_from_inpcb_t		mpo_init_syncache_from_inpcb;
	mpo_create_mbuf_from_syncache_t		mpo_create_mbuf_from_syncache;
	mpo_priv_check_t			mpo_priv_check;
	mpo_priv_grant_t			mpo_priv_grant;
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

/*-
 * The TrustedBSD MAC Framework has a major version number, MAC_VERSION,
 * which defines the ABI of the Framework present in the kernel (and depended
 * on by policy modules compiled against that kernel).  Currently,
 * MAC_POLICY_SET() requires that the kernel and module ABI version numbers
 * exactly match.  The following major versions have been defined to date:
 *
 *   MAC version             FreeBSD versions
 *   1                       5.x
 *   2                       6.x
 *   3                       7.x
 */
#define	MAC_VERSION	3

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
	MODULE_DEPEND(mpname, kernel_mac_support, MAC_VERSION,		\
	    MAC_VERSION, MAC_VERSION);					\
	DECLARE_MODULE(mpname, mpname##_mod, SI_SUB_MAC_POLICY,		\
	    SI_ORDER_MIDDLE)

int	mac_policy_modevent(module_t mod, int type, void *data);

/*
 * Policy interface to map a struct label pointer to per-policy data.
 * Typically, policies wrap this in their own accessor macro that casts a
 * uintptr_t to a policy-specific data type.
 */
intptr_t	mac_label_get(struct label *l, int slot);
void		mac_label_set(struct label *l, int slot, intptr_t v);

#endif /* !_SYS_SECURITY_MAC_MAC_POLICY_H_ */
