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
 * $FreeBSD$
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
typedef void	(*mpo_bpfdesc_init_label_t)(struct label *label);
typedef void	(*mpo_cred_init_label_t)(struct label *label);
typedef void	(*mpo_devfs_init_label_t)(struct label *label);
typedef void	(*mpo_ifnet_init_label_t)(struct label *label);
typedef int	(*mpo_inpcb_init_label_t)(struct label *label, int flag);
typedef void	(*mpo_sysvmsg_init_label_t)(struct label *label);
typedef void	(*mpo_sysvmsq_init_label_t)(struct label *label);
typedef void	(*mpo_sysvsem_init_label_t)(struct label *label);
typedef void	(*mpo_sysvshm_init_label_t)(struct label *label);
typedef int	(*mpo_ipq_init_label_t)(struct label *label, int flag);
typedef int	(*mpo_mbuf_init_label_t)(struct label *label, int flag);
typedef void	(*mpo_mount_init_label_t)(struct label *label);
typedef int	(*mpo_socket_init_label_t)(struct label *label, int flag);
typedef int	(*mpo_socketpeer_init_label_t)(struct label *label,
		    int flag);
typedef void	(*mpo_pipe_init_label_t)(struct label *label);
typedef void    (*mpo_posixsem_init_label_t)(struct label *label);
typedef void	(*mpo_proc_init_label_t)(struct label *label);
typedef void	(*mpo_vnode_init_label_t)(struct label *label);
typedef void	(*mpo_bpfdesc_destroy_label_t)(struct label *label);
typedef void	(*mpo_cred_destroy_label_t)(struct label *label);
typedef void	(*mpo_devfs_destroy_label_t)(struct label *label);
typedef void	(*mpo_ifnet_destroy_label_t)(struct label *label);
typedef void	(*mpo_inpcb_destroy_label_t)(struct label *label);
typedef void	(*mpo_sysvmsg_destroy_label_t)(struct label *label);
typedef void	(*mpo_sysvmsq_destroy_label_t)(struct label *label);
typedef void	(*mpo_sysvsem_destroy_label_t)(struct label *label);
typedef void	(*mpo_sysvshm_destroy_label_t)(struct label *label);
typedef void	(*mpo_ipq_destroy_label_t)(struct label *label);
typedef void	(*mpo_mbuf_destroy_label_t)(struct label *label);
typedef void	(*mpo_mount_destroy_label_t)(struct label *label);
typedef void	(*mpo_socket_destroy_label_t)(struct label *label);
typedef void	(*mpo_socketpeer_destroy_label_t)(struct label *label);
typedef void	(*mpo_pipe_destroy_label_t)(struct label *label);
typedef void    (*mpo_posixsem_destroy_label_t)(struct label *label);
typedef void	(*mpo_proc_destroy_label_t)(struct label *label);
typedef void	(*mpo_vnode_destroy_label_t)(struct label *label);
typedef void	(*mpo_sysvmsg_cleanup_t)(struct label *msglabel);
typedef void	(*mpo_sysvmsq_cleanup_t)(struct label *msqlabel);
typedef void	(*mpo_sysvsem_cleanup_t)(struct label *semalabel);
typedef void	(*mpo_sysvshm_cleanup_t)(struct label *shmlabel);
typedef void	(*mpo_cred_copy_label_t)(struct label *src,
		    struct label *dest);
typedef void	(*mpo_ifnet_copy_label_t)(struct label *src,
		    struct label *dest);
typedef void	(*mpo_mbuf_copy_label_t)(struct label *src,
		    struct label *dest);
typedef void	(*mpo_pipe_copy_label_t)(struct label *src,
		    struct label *dest);
typedef void	(*mpo_socket_copy_label_t)(struct label *src,
		    struct label *dest);
typedef void	(*mpo_vnode_copy_label_t)(struct label *src,
		    struct label *dest);
typedef int	(*mpo_cred_externalize_label_t)(struct label *label,
		    char *element_name, struct sbuf *sb, int *claimed);
typedef int	(*mpo_ifnet_externalize_label_t)(struct label *label,
		    char *element_name, struct sbuf *sb, int *claimed);
typedef int	(*mpo_pipe_externalize_label_t)(struct label *label,
		    char *element_name, struct sbuf *sb, int *claimed);
typedef int	(*mpo_socket_externalize_label_t)(struct label *label,
		    char *element_name, struct sbuf *sb, int *claimed);
typedef int	(*mpo_socketpeer_externalize_label_t)(struct label *label,
		    char *element_name, struct sbuf *sb, int *claimed);
typedef int	(*mpo_vnode_externalize_label_t)(struct label *label,
		    char *element_name, struct sbuf *sb, int *claimed);
typedef int	(*mpo_cred_internalize_label_t)(struct label *label,
		    char *element_name, char *element_data, int *claimed);
typedef int	(*mpo_ifnet_internalize_label_t)(struct label *label,
		    char *element_name, char *element_data, int *claimed);
typedef int	(*mpo_pipe_internalize_label_t)(struct label *label,
		    char *element_name, char *element_data, int *claimed);
typedef int	(*mpo_socket_internalize_label_t)(struct label *label,
		    char *element_name, char *element_data, int *claimed);
typedef int	(*mpo_vnode_internalize_label_t)(struct label *label,
		    char *element_name, char *element_data, int *claimed);

/*
 * Labeling event operations: file system objects, and things that look a lot
 * like file system objects.
 */
typedef void	(*mpo_devfs_vnode_associate_t)(struct mount *mp,
		    struct label *mplabel, struct devfs_dirent *de,
		    struct label *delabel, struct vnode *vp,
		    struct label *vplabel);
typedef int	(*mpo_vnode_associate_extattr_t)(struct mount *mp,
		    struct label *mplabel, struct vnode *vp,
		    struct label *vplabel);
typedef void	(*mpo_vnode_associate_singlelabel_t)(struct mount *mp,
		    struct label *mplabel, struct vnode *vp,
		    struct label *vplabel);
typedef void	(*mpo_devfs_create_device_t)(struct ucred *cred,
		    struct mount *mp, struct cdev *dev,
		    struct devfs_dirent *de, struct label *delabel);
typedef void	(*mpo_devfs_create_directory_t)(struct mount *mp,
		    char *dirname, int dirnamelen, struct devfs_dirent *de,
		    struct label *delabel);
typedef void	(*mpo_devfs_create_symlink_t)(struct ucred *cred,
		    struct mount *mp, struct devfs_dirent *dd,
		    struct label *ddlabel, struct devfs_dirent *de,
		    struct label *delabel);
typedef int	(*mpo_vnode_create_extattr_t)(struct ucred *cred,
		    struct mount *mp, struct label *mplabel,
		    struct vnode *dvp, struct label *dvplabel,
		    struct vnode *vp, struct label *vplabel,
		    struct componentname *cnp);
typedef void	(*mpo_mount_create_t)(struct ucred *cred, struct mount *mp,
		    struct label *mplabel);
typedef void	(*mpo_vnode_relabel_t)(struct ucred *cred, struct vnode *vp,
		    struct label *vplabel, struct label *label);
typedef int	(*mpo_vnode_setlabel_extattr_t)(struct ucred *cred,
		    struct vnode *vp, struct label *vplabel,
		    struct label *intlabel);
typedef void	(*mpo_devfs_update_t)(struct mount *mp,
		    struct devfs_dirent *de, struct label *delabel,
		    struct vnode *vp, struct label *vplabel);

/*
 * Labeling event operations: IPC objects.
 */
typedef void	(*mpo_socket_create_mbuf_t)(struct socket *so,
		    struct label *solabel, struct mbuf *m,
		    struct label *mlabel);
typedef void	(*mpo_socket_create_t)(struct ucred *cred, struct socket *so,
		    struct label *solabel);
typedef void	(*mpo_socket_newconn_t)(struct socket *oldso,
		    struct label *oldsolabel, struct socket *newso,
		    struct label *newsolabel);
typedef void	(*mpo_socket_relabel_t)(struct ucred *cred, struct socket *so,
		    struct label *oldlabel, struct label *newlabel);
typedef void	(*mpo_pipe_relabel_t)(struct ucred *cred, struct pipepair *pp,
		    struct label *oldlabel, struct label *newlabel);
typedef void	(*mpo_socketpeer_set_from_mbuf_t)(struct mbuf *m,
		    struct label *mlabel, struct socket *so,
		    struct label *sopeerlabel);
typedef void	(*mpo_socketpeer_set_from_socket_t)(struct socket *oldso,
		    struct label *oldsolabel, struct socket *newso,
		    struct label *newsopeerlabel);
typedef void	(*mpo_pipe_create_t)(struct ucred *cred, struct pipepair *pp,
		    struct label *pplabel);

/*
 * Labeling event operations: System V IPC primitives.
 */
typedef void	(*mpo_sysvmsg_create_t)(struct ucred *cred,
		    struct msqid_kernel *msqkptr, struct label *msqlabel,
		    struct msg *msgptr, struct label *msglabel);
typedef void	(*mpo_sysvmsq_create_t)(struct ucred *cred,
		    struct msqid_kernel *msqkptr, struct label *msqlabel);
typedef void	(*mpo_sysvsem_create_t)(struct ucred *cred,
		    struct semid_kernel *semakptr, struct label *semalabel);
typedef void	(*mpo_sysvshm_create_t)(struct ucred *cred,
		    struct shmid_kernel *shmsegptr, struct label *shmlabel);

/*
 * Labeling event operations: POSIX (global/inter-process) semaphores.
 */
typedef void	(*mpo_posixsem_create_t)(struct ucred *cred,
		    struct ksem *ks, struct label *kslabel);

/*
 * Labeling event operations: network objects.
 */
typedef void	(*mpo_bpfdesc_create_t)(struct ucred *cred,
		    struct bpf_d *d, struct label *dlabel);
typedef void	(*mpo_ifnet_create_t)(struct ifnet *ifp,
		    struct label *ifplabel);
typedef void	(*mpo_inpcb_create_t)(struct socket *so,
		    struct label *solabel, struct inpcb *inp,
		    struct label *inplabel);
typedef void	(*mpo_ipq_create_t)(struct mbuf *m, struct label *mlabel,
		    struct ipq *ipq, struct label *ipqlabel);
typedef void	(*mpo_ipq_reassemble)
		    (struct ipq *ipq, struct label *ipqlabel, struct mbuf *m,
		    struct label *mlabel);
typedef void	(*mpo_netinet_fragment_t)(struct mbuf *m,
		    struct label *mlabel, struct mbuf *frag,
		    struct label *fraglabel);
typedef void	(*mpo_inpcb_create_mbuf_t)(struct inpcb *inp,
		    struct label *inplabel, struct mbuf *m,
		    struct label *mlabel);
typedef void	(*mpo_create_mbuf_linklayer_t)(struct ifnet *ifp,
		    struct label *ifplabel, struct mbuf *m,
		    struct label *mlabel);
typedef void	(*mpo_bpfdesc_create_mbuf_t)(struct bpf_d *d,
		    struct label *dlabel, struct mbuf *m,
		    struct label *mlabel);
typedef void	(*mpo_ifnet_create_mbuf_t)(struct ifnet *ifp,
		    struct label *ifplabel, struct mbuf *m,
		    struct label *mlabel);
typedef void	(*mpo_mbuf_create_multicast_encap_t)(struct mbuf *m,
		    struct label *mlabel, struct ifnet *ifp,
		    struct label *ifplabel, struct mbuf *mnew,
		    struct label *mnewlabel);
typedef void	(*mpo_mbuf_create_netlayer_t)(struct mbuf *m,
		    struct label *mlabel, struct mbuf *mnew,
		    struct label *mnewlabel);
typedef int	(*mpo_ipq_match_t)(struct mbuf *m, struct label *mlabel,
		    struct ipq *ipq, struct label *ipqlabel);
typedef void	(*mpo_netinet_icmp_reply_t)(struct mbuf *m,
		    struct label *mlabel);
typedef void	(*mpo_netinet_tcp_reply_t)(struct mbuf *m,
		    struct label *mlabel);
typedef void	(*mpo_ifnet_relabel_t)(struct ucred *cred, struct ifnet *ifp,
		    struct label *ifplabel, struct label *newlabel);
typedef void	(*mpo_ipq_update_t)(struct mbuf *m, struct label *mlabel,
		    struct ipq *ipq, struct label *ipqlabel);
typedef void	(*mpo_inpcb_sosetlabel_t)(struct socket *so,
		    struct label *label, struct inpcb *inp,
		    struct label *inplabel);

typedef	void	(*mpo_mbuf_create_from_firewall_t)(struct mbuf *m,
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
typedef void	(*mpo_vnode_execve_transition_t)(struct ucred *old,
		    struct ucred *new, struct vnode *vp,
		    struct label *vplabel, struct label *interpvnodelabel,
		    struct image_params *imgp, struct label *execlabel);
typedef int	(*mpo_vnode_execve_will_transition_t)(struct ucred *old,
		    struct vnode *vp, struct label *vplabel,
		    struct label *interpvnodelabel,
		    struct image_params *imgp, struct label *execlabel);
typedef void	(*mpo_proc_create_swapper_t)(struct ucred *cred);
typedef void	(*mpo_proc_create_init_t)(struct ucred *cred);
typedef void	(*mpo_cred_relabel_t)(struct ucred *cred,
		    struct label *newlabel);
typedef void	(*mpo_thread_userret_t)(struct thread *thread);

/*
 * Access control checks.
 */
typedef	int	(*mpo_bpfdesc_check_receive_t)(struct bpf_d *d,
		    struct label *dlabel, struct ifnet *ifp,
		    struct label *ifplabel);
typedef int	(*mpo_cred_check_relabel_t)(struct ucred *cred,
		    struct label *newlabel);
typedef int	(*mpo_cred_check_visible_t)(struct ucred *cr1,
		    struct ucred *cr2);
typedef int	(*mpo_ifnet_check_relabel_t)(struct ucred *cred,
		    struct ifnet *ifp, struct label *ifplabel,
		    struct label *newlabel);
typedef int	(*mpo_ifnet_check_transmit_t)(struct ifnet *ifp,
		    struct label *ifplabel, struct mbuf *m,
		    struct label *mlabel);
typedef int	(*mpo_inpcb_check_deliver_t)(struct inpcb *inp,
		    struct label *inplabel, struct mbuf *m,
		    struct label *mlabel);
typedef int	(*mpo_sysvmsq_check_msgmsq_t)(struct ucred *cred,
		    struct msg *msgptr, struct label *msglabel,
		    struct msqid_kernel *msqkptr, struct label *msqklabel);
typedef int	(*mpo_sysvmsq_check_msgrcv_t)(struct ucred *cred,
		    struct msg *msgptr, struct label *msglabel);
typedef int	(*mpo_sysvmsq_check_msgrmid_t)(struct ucred *cred,
		    struct msg *msgptr, struct label *msglabel);
typedef int	(*mpo_sysvmsq_check_msqget_t)(struct ucred *cred,
		    struct msqid_kernel *msqkptr, struct label *msqklabel);
typedef int	(*mpo_sysvmsq_check_msqsnd_t)(struct ucred *cred,
		    struct msqid_kernel *msqkptr, struct label *msqklabel);
typedef int	(*mpo_sysvmsq_check_msqrcv_t)(struct ucred *cred,
		    struct msqid_kernel *msqkptr, struct label *msqklabel);
typedef int	(*mpo_sysvmsq_check_msqctl_t)(struct ucred *cred,
		    struct msqid_kernel *msqkptr, struct label *msqklabel,
		    int cmd);
typedef int	(*mpo_sysvsem_check_semctl_t)(struct ucred *cred,
		    struct semid_kernel *semakptr, struct label *semaklabel,
		    int cmd);
typedef int	(*mpo_sysvsem_check_semget_t)(struct ucred *cred,
		    struct semid_kernel *semakptr, struct label *semaklabel);
typedef int	(*mpo_sysvsem_check_semop_t)(struct ucred *cred,
		    struct semid_kernel *semakptr, struct label *semaklabel,
		    size_t accesstype);
typedef int	(*mpo_sysvshm_check_shmat_t)(struct ucred *cred,
		    struct shmid_kernel *shmsegptr,
		    struct label *shmseglabel, int shmflg);
typedef int	(*mpo_sysvshm_check_shmctl_t)(struct ucred *cred,
		    struct shmid_kernel *shmsegptr,
		    struct label *shmseglabel, int cmd);
typedef int	(*mpo_sysvshm_check_shmdt_t)(struct ucred *cred,
		    struct shmid_kernel *shmsegptr,
		    struct label *shmseglabel);
typedef int	(*mpo_sysvshm_check_shmget_t)(struct ucred *cred,
		    struct shmid_kernel *shmsegptr,
		    struct label *shmseglabel, int shmflg);
typedef int	(*mpo_kenv_check_dump_t)(struct ucred *cred);
typedef int	(*mpo_kenv_check_get_t)(struct ucred *cred, char *name);
typedef int	(*mpo_kenv_check_set_t)(struct ucred *cred, char *name,
		    char *value);
typedef int	(*mpo_kenv_check_unset_t)(struct ucred *cred, char *name);
typedef int	(*mpo_kld_check_load_t)(struct ucred *cred, struct vnode *vp,
		    struct label *vplabel);
typedef int	(*mpo_kld_check_stat_t)(struct ucred *cred);
typedef int	(*mpo_mpo_placeholder19_t)(void);
typedef int	(*mpo_mpo_placeholder20_t)(void);
typedef int	(*mpo_mount_check_stat_t)(struct ucred *cred,
		    struct mount *mp, struct label *mplabel);
typedef int	(*mpo_mpo_placeholder21_t)(void);
typedef int	(*mpo_pipe_check_ioctl_t)(struct ucred *cred,
		    struct pipepair *pp, struct label *pplabel,
		    unsigned long cmd, void *data);
typedef int	(*mpo_pipe_check_poll_t)(struct ucred *cred,
		    struct pipepair *pp, struct label *pplabel);
typedef int	(*mpo_pipe_check_read_t)(struct ucred *cred,
		    struct pipepair *pp, struct label *pplabel);
typedef int	(*mpo_pipe_check_relabel_t)(struct ucred *cred,
		    struct pipepair *pp, struct label *pplabel,
		    struct label *newlabel);
typedef int	(*mpo_pipe_check_stat_t)(struct ucred *cred,
		    struct pipepair *pp, struct label *pplabel);
typedef int	(*mpo_pipe_check_write_t)(struct ucred *cred,
		    struct pipepair *pp, struct label *pplabel);
typedef int	(*mpo_posixsem_check_destroy_t)(struct ucred *cred,
		    struct ksem *ks, struct label *kslabel);
typedef int	(*mpo_posixsem_check_getvalue_t)(struct ucred *cred,
		    struct ksem *ks, struct label *kslabel);
typedef int	(*mpo_posixsem_check_open_t)(struct ucred *cred,
		    struct ksem *ks, struct label *kslabel);
typedef int	(*mpo_posixsem_check_post_t)(struct ucred *cred,
		    struct ksem *ks, struct label *kslabel);
typedef int	(*mpo_posixsem_check_unlink_t)(struct ucred *cred,
		    struct ksem *ks, struct label *kslabel);
typedef int	(*mpo_posixsem_check_wait_t)(struct ucred *cred,
		    struct ksem *ks, struct label *kslabel);
typedef int	(*mpo_proc_check_debug_t)(struct ucred *cred,
		    struct proc *p);
typedef int	(*mpo_proc_check_sched_t)(struct ucred *cred,
		    struct proc *p);
typedef int	(*mpo_proc_check_setaudit_t)(struct ucred *cred,
		    struct auditinfo *ai);
typedef int	(*mpo_proc_check_setaudit_addr_t)(struct ucred *cred,
		    struct auditinfo_addr *aia);
typedef int	(*mpo_proc_check_setauid_t)(struct ucred *cred, uid_t auid);
typedef int	(*mpo_proc_check_setuid_t)(struct ucred *cred, uid_t uid);
typedef int	(*mpo_proc_check_seteuid_t)(struct ucred *cred, uid_t euid);
typedef int	(*mpo_proc_check_setgid_t)(struct ucred *cred, gid_t gid);
typedef int	(*mpo_proc_check_setegid_t)(struct ucred *cred, gid_t egid);
typedef int	(*mpo_proc_check_setgroups_t)(struct ucred *cred, int ngroups,
		    gid_t *gidset);
typedef int	(*mpo_proc_check_setreuid_t)(struct ucred *cred, uid_t ruid,
		    uid_t euid);
typedef int	(*mpo_proc_check_setregid_t)(struct ucred *cred, gid_t rgid,
		    gid_t egid);
typedef int	(*mpo_proc_check_setresuid_t)(struct ucred *cred, uid_t ruid,
		    uid_t euid, uid_t suid);
typedef int	(*mpo_proc_check_setresgid_t)(struct ucred *cred, gid_t rgid,
		    gid_t egid, gid_t sgid);
typedef int	(*mpo_proc_check_signal_t)(struct ucred *cred,
		    struct proc *proc, int signum);
typedef int	(*mpo_proc_check_wait_t)(struct ucred *cred,
		    struct proc *proc);
typedef int	(*mpo_socket_check_accept_t)(struct ucred *cred,
		    struct socket *so, struct label *solabel);
typedef int	(*mpo_socket_check_bind_t)(struct ucred *cred,
		    struct socket *so, struct label *solabel,
		    struct sockaddr *sa);
typedef int	(*mpo_socket_check_connect_t)(struct ucred *cred,
		    struct socket *so, struct label *solabel,
		    struct sockaddr *sa);
typedef int	(*mpo_socket_check_create_t)(struct ucred *cred, int domain,
		    int type, int protocol);
typedef int	(*mpo_socket_check_deliver_t)(struct socket *so,
		    struct label *solabel, struct mbuf *m,
		    struct label *mlabel);
typedef int	(*mpo_socket_check_listen_t)(struct ucred *cred,
		    struct socket *so, struct label *solabel);
typedef int	(*mpo_socket_check_poll_t)(struct ucred *cred,
		    struct socket *so, struct label *solabel);
typedef int	(*mpo_socket_check_receive_t)(struct ucred *cred,
		    struct socket *so, struct label *solabel);
typedef int	(*mpo_socket_check_relabel_t)(struct ucred *cred,
		    struct socket *so, struct label *solabel,
		    struct label *newlabel);
typedef int	(*mpo_socket_check_send_t)(struct ucred *cred,
		    struct socket *so, struct label *solabel);
typedef int	(*mpo_socket_check_stat_t)(struct ucred *cred,
		    struct socket *so, struct label *solabel);
typedef int	(*mpo_socket_check_visible_t)(struct ucred *cred,
		    struct socket *so, struct label *solabel);
typedef int	(*mpo_system_check_acct_t)(struct ucred *cred,
		    struct vnode *vp, struct label *vplabel);
typedef int	(*mpo_system_check_audit_t)(struct ucred *cred, void *record,
		    int length);
typedef int	(*mpo_system_check_auditctl_t)(struct ucred *cred,
		    struct vnode *vp, struct label *vplabel);
typedef int	(*mpo_system_check_auditon_t)(struct ucred *cred, int cmd);
typedef int	(*mpo_system_check_reboot_t)(struct ucred *cred, int howto);
typedef int	(*mpo_system_check_swapon_t)(struct ucred *cred,
		    struct vnode *vp, struct label *vplabel);
typedef int	(*mpo_system_check_swapoff_t)(struct ucred *cred,
		    struct vnode *vp, struct label *vplabel);
typedef int	(*mpo_system_check_sysctl_t)(struct ucred *cred,
		    struct sysctl_oid *oidp, void *arg1, int arg2,
		    struct sysctl_req *req);
typedef int	(*mpo_vnode_check_access_t)(struct ucred *cred,
		    struct vnode *vp, struct label *vplabel, int acc_mode);
typedef int	(*mpo_vnode_check_chdir_t)(struct ucred *cred,
		    struct vnode *dvp, struct label *dvplabel);
typedef int	(*mpo_vnode_check_chroot_t)(struct ucred *cred,
		    struct vnode *dvp, struct label *dvplabel);
typedef int	(*mpo_vnode_check_create_t)(struct ucred *cred,
		    struct vnode *dvp, struct label *dvplabel,
		    struct componentname *cnp, struct vattr *vap);
typedef int	(*mpo_vnode_check_deleteacl_t)(struct ucred *cred,
		    struct vnode *vp, struct label *vplabel,
		    acl_type_t type);
typedef int	(*mpo_vnode_check_deleteextattr_t)(struct ucred *cred,
		    struct vnode *vp, struct label *vplabel,
		    int attrnamespace, const char *name);
typedef int	(*mpo_vnode_check_exec_t)(struct ucred *cred,
		    struct vnode *vp, struct label *vplabel,
		    struct image_params *imgp, struct label *execlabel);
typedef int	(*mpo_vnode_check_getacl_t)(struct ucred *cred,
		    struct vnode *vp, struct label *vplabel,
		    acl_type_t type);
typedef int	(*mpo_vnode_check_getextattr_t)(struct ucred *cred,
		    struct vnode *vp, struct label *vplabel,
		    int attrnamespace, const char *name, struct uio *uio);
typedef int	(*mpo_vnode_check_link_t)(struct ucred *cred,
		    struct vnode *dvp, struct label *dvplabel,
		    struct vnode *vp, struct label *vplabel,
		    struct componentname *cnp);
typedef int	(*mpo_vnode_check_listextattr_t)(struct ucred *cred,
		    struct vnode *vp, struct label *vplabel,
		    int attrnamespace);
typedef int	(*mpo_vnode_check_lookup_t)(struct ucred *cred,
		    struct vnode *dvp, struct label *dvplabel,
		    struct componentname *cnp);
typedef int	(*mpo_vnode_check_mmap_t)(struct ucred *cred,
		    struct vnode *vp, struct label *label, int prot,
		    int flags);
typedef void	(*mpo_vnode_check_mmap_downgrade_t)(struct ucred *cred,
		    struct vnode *vp, struct label *vplabel, int *prot);
typedef int	(*mpo_vnode_check_mprotect_t)(struct ucred *cred,
		    struct vnode *vp, struct label *vplabel, int prot);
typedef int	(*mpo_vnode_check_open_t)(struct ucred *cred,
		    struct vnode *vp, struct label *vplabel, int acc_mode);
typedef int	(*mpo_vnode_check_poll_t)(struct ucred *active_cred,
		    struct ucred *file_cred, struct vnode *vp,
		    struct label *vplabel);
typedef int	(*mpo_vnode_check_read_t)(struct ucred *active_cred,
		    struct ucred *file_cred, struct vnode *vp,
		    struct label *vplabel);
typedef int	(*mpo_vnode_check_readdir_t)(struct ucred *cred,
		    struct vnode *dvp, struct label *dvplabel);
typedef int	(*mpo_vnode_check_readlink_t)(struct ucred *cred,
		    struct vnode *vp, struct label *vplabel);
typedef int	(*mpo_vnode_check_relabel_t)(struct ucred *cred,
		    struct vnode *vp, struct label *vplabel,
		    struct label *newlabel);
typedef int	(*mpo_vnode_check_rename_from_t)(struct ucred *cred,
		    struct vnode *dvp, struct label *dvplabel,
		    struct vnode *vp, struct label *vplabel,
		    struct componentname *cnp);
typedef int	(*mpo_vnode_check_rename_to_t)(struct ucred *cred,
		    struct vnode *dvp, struct label *dvplabel,
		    struct vnode *vp, struct label *vplabel, int samedir,
		    struct componentname *cnp);
typedef int	(*mpo_vnode_check_revoke_t)(struct ucred *cred,
		    struct vnode *vp, struct label *vplabel);
typedef int	(*mpo_vnode_check_setacl_t)(struct ucred *cred,
		    struct vnode *vp, struct label *vplabel, acl_type_t type,
		    struct acl *acl);
typedef int	(*mpo_vnode_check_setextattr_t)(struct ucred *cred,
		    struct vnode *vp, struct label *vplabel,
		    int attrnamespace, const char *name, struct uio *uio);
typedef int	(*mpo_vnode_check_setflags_t)(struct ucred *cred,
		    struct vnode *vp, struct label *vplabel, u_long flags);
typedef int	(*mpo_vnode_check_setmode_t)(struct ucred *cred,
		    struct vnode *vp, struct label *vplabel, mode_t mode);
typedef int	(*mpo_vnode_check_setowner_t)(struct ucred *cred,
		    struct vnode *vp, struct label *vplabel, uid_t uid,
		    gid_t gid);
typedef int	(*mpo_vnode_check_setutimes_t)(struct ucred *cred,
		    struct vnode *vp, struct label *vplabel,
		    struct timespec atime, struct timespec mtime);
typedef int	(*mpo_vnode_check_stat_t)(struct ucred *active_cred,
		    struct ucred *file_cred, struct vnode *vp,
		    struct label *vplabel);
typedef int	(*mpo_vnode_check_unlink_t)(struct ucred *cred,
		    struct vnode *dvp, struct label *dvplabel,
		    struct vnode *vp, struct label *vplabel,
		    struct componentname *cnp);
typedef int	(*mpo_vnode_check_write_t)(struct ucred *active_cred,
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
	mpo_bpfdesc_init_label_t		mpo_bpfdesc_init_label;
	mpo_cred_init_label_t			mpo_cred_init_label;
	mpo_devfs_init_label_t			mpo_devfs_init_label;
	mpo_placeholder_t			_mpo_placeholder0;
	mpo_ifnet_init_label_t			mpo_ifnet_init_label;
	mpo_inpcb_init_label_t			mpo_inpcb_init_label;
	mpo_sysvmsg_init_label_t		mpo_sysvmsg_init_label;
	mpo_sysvmsq_init_label_t		mpo_sysvmsq_init_label;
	mpo_sysvsem_init_label_t		mpo_sysvsem_init_label;
	mpo_sysvshm_init_label_t		mpo_sysvshm_init_label;
	mpo_ipq_init_label_t			mpo_ipq_init_label;
	mpo_mbuf_init_label_t			mpo_mbuf_init_label;
	mpo_mount_init_label_t			mpo_mount_init_label;
	mpo_socket_init_label_t			mpo_socket_init_label;
	mpo_socketpeer_init_label_t		mpo_socketpeer_init_label;
	mpo_pipe_init_label_t			mpo_pipe_init_label;
	mpo_posixsem_init_label_t		mpo_posixsem_init_label;
	mpo_proc_init_label_t			mpo_proc_init_label;
	mpo_vnode_init_label_t			mpo_vnode_init_label;
	mpo_bpfdesc_destroy_label_t		mpo_bpfdesc_destroy_label;
	mpo_cred_destroy_label_t		mpo_cred_destroy_label;
	mpo_devfs_destroy_label_t		mpo_devfs_destroy_label;
	mpo_placeholder_t			_mpo_placeholder1;
	mpo_ifnet_destroy_label_t		mpo_ifnet_destroy_label;
	mpo_inpcb_destroy_label_t		mpo_inpcb_destroy_label;
	mpo_sysvmsg_destroy_label_t		mpo_sysvmsg_destroy_label;
	mpo_sysvmsq_destroy_label_t		mpo_sysvmsq_destroy_label;
	mpo_sysvsem_destroy_label_t		mpo_sysvsem_destroy_label;
	mpo_sysvshm_destroy_label_t		mpo_sysvshm_destroy_label;
	mpo_ipq_destroy_label_t			mpo_ipq_destroy_label;
	mpo_mbuf_destroy_label_t		mpo_mbuf_destroy_label;
	mpo_mount_destroy_label_t		mpo_mount_destroy_label;
	mpo_socket_destroy_label_t		mpo_socket_destroy_label;
	mpo_socketpeer_destroy_label_t		mpo_socketpeer_destroy_label;
	mpo_pipe_destroy_label_t		mpo_pipe_destroy_label;
	mpo_posixsem_destroy_label_t		mpo_posixsem_destroy_label;
	mpo_proc_destroy_label_t		mpo_proc_destroy_label;
	mpo_vnode_destroy_label_t		mpo_vnode_destroy_label;
	mpo_sysvmsg_cleanup_t			mpo_sysvmsg_cleanup;
	mpo_sysvmsq_cleanup_t			mpo_sysvmsq_cleanup;
	mpo_sysvsem_cleanup_t			mpo_sysvsem_cleanup;
	mpo_sysvshm_cleanup_t			mpo_sysvshm_cleanup;
	mpo_cred_copy_label_t			mpo_cred_copy_label;
	mpo_ifnet_copy_label_t			mpo_ifnet_copy_label;
	mpo_mbuf_copy_label_t			mpo_mbuf_copy_label;
	mpo_placeholder_t			_mpo_placeholder2;
	mpo_pipe_copy_label_t			mpo_pipe_copy_label;
	mpo_socket_copy_label_t			mpo_socket_copy_label;
	mpo_vnode_copy_label_t			mpo_vnode_copy_label;
	mpo_cred_externalize_label_t		mpo_cred_externalize_label;
	mpo_ifnet_externalize_label_t		mpo_ifnet_externalize_label;
	mpo_placeholder_t			_mpo_placeholder3;
	mpo_pipe_externalize_label_t		mpo_pipe_externalize_label;
	mpo_socket_externalize_label_t		mpo_socket_externalize_label;
	mpo_socketpeer_externalize_label_t	mpo_socketpeer_externalize_label;
	mpo_vnode_externalize_label_t		mpo_vnode_externalize_label;
	mpo_cred_internalize_label_t		mpo_cred_internalize_label;
	mpo_ifnet_internalize_label_t		mpo_ifnet_internalize_label;
	mpo_placeholder_t			_mpo_placeholder4;
	mpo_pipe_internalize_label_t		mpo_pipe_internalize_label;
	mpo_socket_internalize_label_t		mpo_socket_internalize_label;
	mpo_vnode_internalize_label_t		mpo_vnode_internalize_label;

	/*
	 * Labeling event operations: file system objects, and things that
	 * look a lot like file system objects.
	 */
	mpo_devfs_vnode_associate_t		mpo_devfs_vnode_associate;
	mpo_vnode_associate_extattr_t		mpo_vnode_associate_extattr;
	mpo_vnode_associate_singlelabel_t	mpo_vnode_associate_singlelabel;
	mpo_devfs_create_device_t		mpo_devfs_create_device;
	mpo_devfs_create_directory_t		mpo_devfs_create_directory;
	mpo_devfs_create_symlink_t		mpo_devfs_create_symlink;
	mpo_placeholder_t			_mpo_placeholder5;
	mpo_vnode_create_extattr_t		mpo_vnode_create_extattr;
	mpo_mount_create_t			mpo_mount_create;
	mpo_vnode_relabel_t			mpo_vnode_relabel;
	mpo_vnode_setlabel_extattr_t		mpo_vnode_setlabel_extattr;
	mpo_devfs_update_t			mpo_devfs_update;

	/*
	 * Labeling event operations: IPC objects.
	 */
	mpo_socket_create_mbuf_t		mpo_socket_create_mbuf;
	mpo_socket_create_t			mpo_socket_create;
	mpo_socket_newconn_t			mpo_socket_newconn;
	mpo_socket_relabel_t			mpo_socket_relabel;
	mpo_pipe_relabel_t			mpo_pipe_relabel;
	mpo_socketpeer_set_from_mbuf_t		mpo_socketpeer_set_from_mbuf;
	mpo_socketpeer_set_from_socket_t	mpo_socketpeer_set_from_socket;
	mpo_pipe_create_t			mpo_pipe_create;

	/*
	 * Labeling event operations: System V IPC primitives.
	 */
	mpo_sysvmsg_create_t			mpo_sysvmsg_create;
	mpo_sysvmsq_create_t			mpo_sysvmsq_create;
	mpo_sysvsem_create_t			mpo_sysvsem_create;
	mpo_sysvshm_create_t			mpo_sysvshm_create;

	/*
	 * Labeling event operations: POSIX (global/inter-process) semaphores.
	 */
	mpo_posixsem_create_t			mpo_posixsem_create;

	/*
	 * Labeling event operations: network objects.
	 */
	mpo_bpfdesc_create_t			mpo_bpfdesc_create;
	mpo_ifnet_create_t			mpo_ifnet_create;
	mpo_inpcb_create_t			mpo_inpcb_create;
	mpo_ipq_create_t			mpo_ipq_create;
	mpo_ipq_reassemble			mpo_ipq_reassemble;
	mpo_netinet_fragment_t			mpo_netinet_fragment;
	mpo_inpcb_create_mbuf_t			mpo_inpcb_create_mbuf;
	mpo_create_mbuf_linklayer_t		mpo_create_mbuf_linklayer;
	mpo_bpfdesc_create_mbuf_t		mpo_bpfdesc_create_mbuf;
	mpo_ifnet_create_mbuf_t			mpo_ifnet_create_mbuf;
	mpo_mbuf_create_multicast_encap_t	mpo_mbuf_create_multicast_encap;
	mpo_mbuf_create_netlayer_t		mpo_mbuf_create_netlayer;
	mpo_ipq_match_t				mpo_ipq_match;
	mpo_netinet_icmp_reply_t		mpo_netinet_icmp_reply;
	mpo_netinet_tcp_reply_t			mpo_netinet_tcp_reply;
	mpo_ifnet_relabel_t			mpo_ifnet_relabel;
	mpo_ipq_update_t			mpo_ipq_update;
	mpo_inpcb_sosetlabel_t			mpo_inpcb_sosetlabel;

	/*
	 * Labeling event operations: processes.
	 */
	mpo_vnode_execve_transition_t		mpo_vnode_execve_transition;
	mpo_vnode_execve_will_transition_t	mpo_vnode_execve_will_transition;
	mpo_proc_create_swapper_t		mpo_proc_create_swapper;
	mpo_proc_create_init_t			mpo_proc_create_init;
	mpo_cred_relabel_t			mpo_cred_relabel;
	mpo_placeholder_t			_mpo_placeholder6;
	mpo_thread_userret_t			mpo_thread_userret;

	/*
	 * Access control checks.
	 */
	mpo_bpfdesc_check_receive_t		mpo_bpfdesc_check_receive;
	mpo_placeholder_t			_mpo_placeholder7;
	mpo_cred_check_relabel_t		mpo_cred_check_relabel;
	mpo_cred_check_visible_t		mpo_cred_check_visible;
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
	mpo_ifnet_check_relabel_t		mpo_ifnet_check_relabel;
	mpo_ifnet_check_transmit_t		mpo_ifnet_check_transmit;
	mpo_inpcb_check_deliver_t		mpo_inpcb_check_deliver;
	mpo_sysvmsq_check_msgmsq_t		mpo_sysvmsq_check_msgmsq;
	mpo_sysvmsq_check_msgrcv_t		mpo_sysvmsq_check_msgrcv;
	mpo_sysvmsq_check_msgrmid_t		mpo_sysvmsq_check_msgrmid;
	mpo_sysvmsq_check_msqget_t		mpo_sysvmsq_check_msqget;
	mpo_sysvmsq_check_msqsnd_t		mpo_sysvmsq_check_msqsnd;
	mpo_sysvmsq_check_msqrcv_t		mpo_sysvmsq_check_msqrcv;
	mpo_sysvmsq_check_msqctl_t		mpo_sysvmsq_check_msqctl;
	mpo_sysvsem_check_semctl_t		mpo_sysvsem_check_semctl;
	mpo_sysvsem_check_semget_t		mpo_sysvsem_check_semget;
	mpo_sysvsem_check_semop_t		mpo_sysvsem_check_semop;
	mpo_sysvshm_check_shmat_t		mpo_sysvshm_check_shmat;
	mpo_sysvshm_check_shmctl_t		mpo_sysvshm_check_shmctl;
	mpo_sysvshm_check_shmdt_t		mpo_sysvshm_check_shmdt;
	mpo_sysvshm_check_shmget_t		mpo_sysvshm_check_shmget;
	mpo_kenv_check_dump_t			mpo_kenv_check_dump;
	mpo_kenv_check_get_t			mpo_kenv_check_get;
	mpo_kenv_check_set_t			mpo_kenv_check_set;
	mpo_kenv_check_unset_t			mpo_kenv_check_unset;
	mpo_kld_check_load_t			mpo_kld_check_load;
	mpo_kld_check_stat_t			mpo_kld_check_stat;
	mpo_placeholder_t			_mpo_placeholder19;
	mpo_placeholder_t			_mpo_placeholder20;
	mpo_mount_check_stat_t			mpo_mount_check_stat;
	mpo_placeholder_t			_mpo_placeholder_21;
	mpo_pipe_check_ioctl_t			mpo_pipe_check_ioctl;
	mpo_pipe_check_poll_t			mpo_pipe_check_poll;
	mpo_pipe_check_read_t			mpo_pipe_check_read;
	mpo_pipe_check_relabel_t		mpo_pipe_check_relabel;
	mpo_pipe_check_stat_t			mpo_pipe_check_stat;
	mpo_pipe_check_write_t			mpo_pipe_check_write;
	mpo_posixsem_check_destroy_t		mpo_posixsem_check_destroy;
	mpo_posixsem_check_getvalue_t		mpo_posixsem_check_getvalue;
	mpo_posixsem_check_open_t		mpo_posixsem_check_open;
	mpo_posixsem_check_post_t		mpo_posixsem_check_post;
	mpo_posixsem_check_unlink_t		mpo_posixsem_check_unlink;
	mpo_posixsem_check_wait_t		mpo_posixsem_check_wait;
	mpo_proc_check_debug_t			mpo_proc_check_debug;
	mpo_proc_check_sched_t			mpo_proc_check_sched;
	mpo_proc_check_setaudit_t		mpo_proc_check_setaudit;
	mpo_proc_check_setaudit_addr_t		mpo_proc_check_setaudit_addr;
	mpo_proc_check_setauid_t		mpo_proc_check_setauid;
	mpo_proc_check_setuid_t			mpo_proc_check_setuid;
	mpo_proc_check_seteuid_t		mpo_proc_check_seteuid;
	mpo_proc_check_setgid_t			mpo_proc_check_setgid;
	mpo_proc_check_setegid_t		mpo_proc_check_setegid;
	mpo_proc_check_setgroups_t		mpo_proc_check_setgroups;
	mpo_proc_check_setreuid_t		mpo_proc_check_setreuid;
	mpo_proc_check_setregid_t		mpo_proc_check_setregid;
	mpo_proc_check_setresuid_t		mpo_proc_check_setresuid;
	mpo_proc_check_setresgid_t		mpo_proc_check_setresgid;
	mpo_proc_check_signal_t			mpo_proc_check_signal;
	mpo_proc_check_wait_t			mpo_proc_check_wait;
	mpo_socket_check_accept_t		mpo_socket_check_accept;
	mpo_socket_check_bind_t			mpo_socket_check_bind;
	mpo_socket_check_connect_t		mpo_socket_check_connect;
	mpo_socket_check_create_t		mpo_socket_check_create;
	mpo_socket_check_deliver_t		mpo_socket_check_deliver;
	mpo_placeholder_t			_mpo_placeholder22;
	mpo_socket_check_listen_t		mpo_socket_check_listen;
	mpo_socket_check_poll_t			mpo_socket_check_poll;
	mpo_socket_check_receive_t		mpo_socket_check_receive;
	mpo_socket_check_relabel_t		mpo_socket_check_relabel;
	mpo_socket_check_send_t			mpo_socket_check_send;
	mpo_socket_check_stat_t			mpo_socket_check_stat;
	mpo_socket_check_visible_t		mpo_socket_check_visible;
	mpo_system_check_acct_t			mpo_system_check_acct;
	mpo_system_check_audit_t		mpo_system_check_audit;
	mpo_system_check_auditctl_t		mpo_system_check_auditctl;
	mpo_system_check_auditon_t		mpo_system_check_auditon;
	mpo_system_check_reboot_t		mpo_system_check_reboot;
	mpo_system_check_swapon_t		mpo_system_check_swapon;
	mpo_system_check_swapoff_t		mpo_system_check_swapoff;
	mpo_system_check_sysctl_t		mpo_system_check_sysctl;
	mpo_placeholder_t			_mpo_placeholder23;
	mpo_vnode_check_access_t		mpo_vnode_check_access;
	mpo_vnode_check_chdir_t			mpo_vnode_check_chdir;
	mpo_vnode_check_chroot_t		mpo_vnode_check_chroot;
	mpo_vnode_check_create_t		mpo_vnode_check_create;
	mpo_vnode_check_deleteacl_t		mpo_vnode_check_deleteacl;
	mpo_vnode_check_deleteextattr_t		mpo_vnode_check_deleteextattr;
	mpo_vnode_check_exec_t			mpo_vnode_check_exec;
	mpo_vnode_check_getacl_t		mpo_vnode_check_getacl;
	mpo_vnode_check_getextattr_t		mpo_vnode_check_getextattr;
	mpo_placeholder_t			_mpo_placeholder24;
	mpo_vnode_check_link_t			mpo_vnode_check_link;
	mpo_vnode_check_listextattr_t		mpo_vnode_check_listextattr;
	mpo_vnode_check_lookup_t		mpo_vnode_check_lookup;
	mpo_vnode_check_mmap_t			mpo_vnode_check_mmap;
	mpo_vnode_check_mmap_downgrade_t	mpo_vnode_check_mmap_downgrade;
	mpo_vnode_check_mprotect_t		mpo_vnode_check_mprotect;
	mpo_vnode_check_open_t			mpo_vnode_check_open;
	mpo_vnode_check_poll_t			mpo_vnode_check_poll;
	mpo_vnode_check_read_t			mpo_vnode_check_read;
	mpo_vnode_check_readdir_t		mpo_vnode_check_readdir;
	mpo_vnode_check_readlink_t		mpo_vnode_check_readlink;
	mpo_vnode_check_relabel_t		mpo_vnode_check_relabel;
	mpo_vnode_check_rename_from_t		mpo_vnode_check_rename_from;
	mpo_vnode_check_rename_to_t		mpo_vnode_check_rename_to;
	mpo_vnode_check_revoke_t		mpo_vnode_check_revoke;
	mpo_vnode_check_setacl_t		mpo_vnode_check_setacl;
	mpo_vnode_check_setextattr_t		mpo_vnode_check_setextattr;
	mpo_vnode_check_setflags_t		mpo_vnode_check_setflags;
	mpo_vnode_check_setmode_t		mpo_vnode_check_setmode;
	mpo_vnode_check_setowner_t		mpo_vnode_check_setowner;
	mpo_vnode_check_setutimes_t		mpo_vnode_check_setutimes;
	mpo_vnode_check_stat_t			mpo_vnode_check_stat;
	mpo_vnode_check_unlink_t		mpo_vnode_check_unlink;
	mpo_vnode_check_write_t			mpo_vnode_check_write;
	mpo_associate_nfsd_label_t		mpo_associate_nfsd_label;
	mpo_mbuf_create_from_firewall_t		mpo_mbuf_create_from_firewall;
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
 *   4                       8.x
 */
#define	MAC_VERSION	4

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
