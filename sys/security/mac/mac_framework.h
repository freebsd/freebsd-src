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
 * $FreeBSD: src/sys/security/mac/mac_framework.h,v 1.84.2.4.2.1 2008/11/25 02:59:29 kensmith Exp $
 */

/*
 * Kernel interface for Mandatory Access Control -- how kernel services
 * interact with the TrustedBSD MAC Framework.
 */

#ifndef _SECURITY_MAC_MAC_FRAMEWORK_H_
#define	_SECURITY_MAC_MAC_FRAMEWORK_H_

#ifndef _KERNEL
#error "no user-serviceable parts inside"
#endif

struct auditinfo;
struct auditinfo_addr;
struct bpf_d;
struct cdev;
struct componentname;
struct devfs_dirent;
struct ifnet;
struct ifreq;
struct image_params;
struct inpcb;
struct ipq;
struct ksem;
struct label;
struct m_tag;
struct mac;
struct mbuf;
struct mount;
struct msg;
struct msqid_kernel;
struct proc;
struct semid_kernel;
struct shmid_kernel;
struct sockaddr;
struct socket;
struct sysctl_oid;
struct sysctl_req;
struct pipepair;
struct thread;
struct timespec;
struct ucred;
struct uio;
struct vattr;
struct vnode;
struct vop_setlabel_args;

#include <sys/acl.h>			/* XXX acl_type_t */

/*
 * Kernel functions to manage and evaluate labels.
 */
void	mac_init_bpfdesc(struct bpf_d *);
void	mac_init_cred(struct ucred *);
void	mac_init_devfs(struct devfs_dirent *);
void	mac_init_ifnet(struct ifnet *);
int	mac_init_inpcb(struct inpcb *, int);
void	mac_init_sysv_msgmsg(struct msg *);
void	mac_init_sysv_msgqueue(struct msqid_kernel *);
void	mac_init_sysv_sem(struct semid_kernel *);
void	mac_init_sysv_shm(struct shmid_kernel *);
int	mac_init_ipq(struct ipq *, int);
int	mac_init_socket(struct socket *, int);
void	mac_init_pipe(struct pipepair *);
void	mac_init_posix_sem(struct ksem *);
int	mac_init_mbuf(struct mbuf *, int);
int	mac_init_mbuf_tag(struct m_tag *, int);
void	mac_init_mount(struct mount *);
void	mac_init_proc(struct proc *);
void	mac_init_vnode(struct vnode *);
void	mac_copy_mbuf(struct mbuf *, struct mbuf *);
void	mac_copy_mbuf_tag(struct m_tag *, struct m_tag *);
void	mac_copy_vnode_label(struct label *, struct label *);
void	mac_destroy_bpfdesc(struct bpf_d *);
void	mac_destroy_cred(struct ucred *);
void	mac_destroy_devfs(struct devfs_dirent *);
void	mac_destroy_ifnet(struct ifnet *);
void	mac_destroy_inpcb(struct inpcb *);
void	mac_destroy_sysv_msgmsg(struct msg *);
void	mac_destroy_sysv_msgqueue(struct msqid_kernel *);
void	mac_destroy_sysv_sem(struct semid_kernel *);
void	mac_destroy_sysv_shm(struct shmid_kernel *);
void	mac_destroy_ipq(struct ipq *);
void	mac_destroy_socket(struct socket *);
void	mac_destroy_pipe(struct pipepair *);
void	mac_destroy_posix_sem(struct ksem *);
void	mac_destroy_proc(struct proc *);
void	mac_destroy_mbuf_tag(struct m_tag *);
void	mac_destroy_mount(struct mount *);
void	mac_destroy_vnode(struct vnode *);

struct label	*mac_cred_label_alloc(void);
void		 mac_cred_label_free(struct label *);
struct label	*mac_vnode_label_alloc(void);
void		 mac_vnode_label_free(struct label *);

/*
 * Labeling event operations: file system objects, and things that look a lot
 * like file system objects.
 */
void	mac_associate_vnode_devfs(struct mount *mp, struct devfs_dirent *de,
	    struct vnode *vp);
int	mac_associate_vnode_extattr(struct mount *mp, struct vnode *vp);
void	mac_associate_vnode_singlelabel(struct mount *mp, struct vnode *vp);
void	mac_create_devfs_device(struct ucred *cred, struct mount *mp,
	    struct cdev *dev, struct devfs_dirent *de);
void	mac_create_devfs_directory(struct mount *mp, char *dirname,
	    int dirnamelen, struct devfs_dirent *de);
void	mac_create_devfs_symlink(struct ucred *cred, struct mount *mp,
	    struct devfs_dirent *dd, struct devfs_dirent *de);
int	mac_create_vnode_extattr(struct ucred *cred, struct mount *mp,
	    struct vnode *dvp, struct vnode *vp, struct componentname *cnp);
void	mac_create_mount(struct ucred *cred, struct mount *mp);
void	mac_relabel_vnode(struct ucred *cred, struct vnode *vp,
	    struct label *newlabel);
void	mac_update_devfs(struct mount *mp, struct devfs_dirent *de,
	    struct vnode *vp);

/*
 * Labeling event operations: IPC objects.
 */
void	mac_create_mbuf_from_socket(struct socket *so, struct mbuf *m);
void	mac_create_socket(struct ucred *cred, struct socket *so);
void	mac_create_socket_from_socket(struct socket *oldso,
	    struct socket *newso);
void	mac_set_socket_peer_from_mbuf(struct mbuf *m, struct socket *so);
void	mac_set_socket_peer_from_socket(struct socket *oldso,
	    struct socket *newso);
void	mac_create_pipe(struct ucred *cred, struct pipepair *pp);

/*
 * Labeling event operations: System V IPC primitives
 */
void	mac_create_sysv_msgmsg(struct ucred *cred,
	    struct msqid_kernel *msqkptr, struct msg *msgptr);
void	mac_create_sysv_msgqueue(struct ucred *cred,
	    struct msqid_kernel *msqkptr);
void	mac_create_sysv_sem(struct ucred *cred,
	    struct semid_kernel *semakptr);
void	mac_create_sysv_shm(struct ucred *cred,
	    struct shmid_kernel *shmsegptr);

/*
 * Labeling event operations: POSIX (global/inter-process) semaphores.
 */
void 	mac_create_posix_sem(struct ucred *cred, struct ksem *ks);

/*
 * Labeling event operations: network objects.
 */
void	mac_create_bpfdesc(struct ucred *cred, struct bpf_d *d);
void	mac_create_ifnet(struct ifnet *ifp);
void	mac_create_inpcb_from_socket(struct socket *so, struct inpcb *inp);
void	mac_create_ipq(struct mbuf *m, struct ipq *q);
void	mac_create_datagram_from_ipq(struct ipq *q, struct mbuf *m);
void	mac_create_fragment(struct mbuf *m, struct mbuf *frag);
void	mac_create_mbuf_from_inpcb(struct inpcb *inp, struct mbuf *m);
void	mac_create_mbuf_linklayer(struct ifnet *ifp, struct mbuf *m);
void	mac_create_mbuf_from_bpfdesc(struct bpf_d *d, struct mbuf *m);
void	mac_create_mbuf_from_ifnet(struct ifnet *ifp, struct mbuf *m);
void	mac_create_mbuf_multicast_encap(struct mbuf *m, struct ifnet *ifp,
	    struct mbuf *mnew);
void	mac_create_mbuf_netlayer(struct mbuf *m, struct mbuf *mnew);
int	mac_fragment_match(struct mbuf *m, struct ipq *q);
void	mac_reflect_mbuf_icmp(struct mbuf *m);
void	mac_reflect_mbuf_tcp(struct mbuf *m);
void	mac_update_ipq(struct mbuf *m, struct ipq *q);
void	mac_inpcb_sosetlabel(struct socket *so, struct inpcb *inp);
void	mac_create_mbuf_from_firewall(struct mbuf *m);
void	mac_destroy_syncache(struct label **l);
int	mac_init_syncache(struct label **l);
void	mac_init_syncache_from_inpcb(struct label *l, struct inpcb *inp);
void	mac_create_mbuf_from_syncache(struct label *l, struct mbuf *m);

/*
 * Labeling event operations: processes.
 */
void	mac_copy_cred(struct ucred *cr1, struct ucred *cr2);
int	mac_execve_enter(struct image_params *imgp, struct mac *mac_p);
void	mac_execve_exit(struct image_params *imgp);
void	mac_execve_transition(struct ucred *oldcred, struct ucred *newcred,
	    struct vnode *vp, struct label *interpvnodelabel,
	    struct image_params *imgp);
int	mac_execve_will_transition(struct ucred *cred, struct vnode *vp,
	    struct label *interpvnodelabel, struct image_params *imgp);
void	mac_create_proc0(struct ucred *cred);
void	mac_create_proc1(struct ucred *cred);
void	mac_thread_userret(struct thread *td);

/*
 * Label cleanup operation: This is the inverse complement for the mac_create
 * and associate type of hooks. This hook lets the policy module(s) perform a
 * cleanup/flushing operation on the label associated with the objects,
 * without freeing up the space allocated.  This hook is useful in cases
 * where it is desirable to remove any labeling reference when recycling any
 * object to a pool. This hook does not replace the mac_destroy hooks.
 *
 * XXXRW: These object methods are inconsistent with the life cycles of other
 * objects, and likely should be revised to be more consistent.
 */
void	mac_cleanup_sysv_msgmsg(struct msg *msgptr);
void	mac_cleanup_sysv_msgqueue(struct msqid_kernel *msqkptr);
void	mac_cleanup_sysv_sem(struct semid_kernel *semakptr);
void	mac_cleanup_sysv_shm(struct shmid_kernel *shmsegptr);

/*
 * Access control checks.
 */
int	mac_check_bpfdesc_receive(struct bpf_d *d, struct ifnet *ifp);
int	mac_check_cred_visible(struct ucred *cr1, struct ucred *cr2);
int	mac_check_ifnet_transmit(struct ifnet *ifp, struct mbuf *m);
int	mac_check_inpcb_deliver(struct inpcb *inp, struct mbuf *m);
int	mac_check_inpcb_visible(struct ucred *cred, struct inpcb *inp);
int	mac_check_sysv_msgmsq(struct ucred *cred, struct msg *msgptr,
	    struct msqid_kernel *msqkptr);
int	mac_check_sysv_msgrcv(struct ucred *cred, struct msg *msgptr);
int	mac_check_sysv_msgrmid(struct ucred *cred, struct msg *msgptr);
int	mac_check_sysv_msqget(struct ucred *cred,
	    struct msqid_kernel *msqkptr);
int	mac_check_sysv_msqsnd(struct ucred *cred,
	    struct msqid_kernel *msqkptr);
int	mac_check_sysv_msqrcv(struct ucred *cred,
	    struct msqid_kernel *msqkptr);
int	mac_check_sysv_msqctl(struct ucred *cred,
	    struct msqid_kernel *msqkptr, int cmd);
int	mac_check_sysv_semctl(struct ucred *cred,
	    struct semid_kernel *semakptr, int cmd);
int	mac_check_sysv_semget(struct ucred *cred,
	   struct semid_kernel *semakptr);
int	mac_check_sysv_semop(struct ucred *cred,struct semid_kernel *semakptr,
	    size_t accesstype);
int	mac_check_sysv_shmat(struct ucred *cred,
	    struct shmid_kernel *shmsegptr, int shmflg);
int	mac_check_sysv_shmctl(struct ucred *cred,
	    struct shmid_kernel *shmsegptr, int cmd);
int	mac_check_sysv_shmdt(struct ucred *cred,
	    struct shmid_kernel *shmsegptr);
int	mac_check_sysv_shmget(struct ucred *cred,
	    struct shmid_kernel *shmsegptr, int shmflg);
int	mac_check_kenv_dump(struct ucred *cred);
int	mac_check_kenv_get(struct ucred *cred, char *name);
int	mac_check_kenv_set(struct ucred *cred, char *name, char *value);
int	mac_check_kenv_unset(struct ucred *cred, char *name);
int	mac_check_kld_load(struct ucred *cred, struct vnode *vp);
int	mac_check_kld_stat(struct ucred *cred);
int	mac_check_mount_stat(struct ucred *cred, struct mount *mp);
int	mac_check_pipe_ioctl(struct ucred *cred, struct pipepair *pp,
	    unsigned long cmd, void *data);
int	mac_check_pipe_poll(struct ucred *cred, struct pipepair *pp);
int	mac_check_pipe_read(struct ucred *cred, struct pipepair *pp);
int	mac_check_pipe_stat(struct ucred *cred, struct pipepair *pp);
int	mac_check_pipe_write(struct ucred *cred, struct pipepair *pp);
int	mac_check_posix_sem_destroy(struct ucred *cred, struct ksem *ks);
int	mac_check_posix_sem_getvalue(struct ucred *cred,struct ksem *ks);
int	mac_check_posix_sem_open(struct ucred *cred, struct ksem *ks);
int	mac_check_posix_sem_post(struct ucred *cred, struct ksem *ks);
int	mac_check_posix_sem_unlink(struct ucred *cred, struct ksem *ks);
int	mac_check_posix_sem_wait(struct ucred *cred, struct ksem *ks);
int	mac_check_proc_debug(struct ucred *cred, struct proc *p);
int	mac_check_proc_sched(struct ucred *cred, struct proc *p);
int	mac_check_proc_setaudit(struct ucred *cred, struct auditinfo *ai);
int	mac_check_proc_setaudit_addr(struct ucred *cred,
	    struct auditinfo_addr *aia);
int	mac_check_proc_setauid(struct ucred *cred, uid_t auid);
int	mac_check_proc_setuid(struct proc *p,  struct ucred *cred,
	    uid_t uid);
int	mac_check_proc_seteuid(struct proc *p, struct ucred *cred,
	    uid_t euid);
int	mac_check_proc_setgid(struct proc *p, struct ucred *cred,
	    gid_t gid);
int	mac_check_proc_setegid(struct proc *p, struct ucred *cred,
	    gid_t egid);
int	mac_check_proc_setgroups(struct proc *p, struct ucred *cred,
	    int ngroups, gid_t *gidset);
int	mac_check_proc_setreuid(struct proc *p, struct ucred *cred,
	    uid_t ruid, uid_t euid);
int	mac_check_proc_setregid(struct proc *p, struct ucred *cred,
	    gid_t rgid, gid_t egid);
int	mac_check_proc_setresuid(struct proc *p, struct ucred *cred,
	    uid_t ruid, uid_t euid, uid_t suid);
int	mac_check_proc_setresgid(struct proc *p, struct ucred *cred,
	    gid_t rgid, gid_t egid, gid_t sgid);
int	mac_check_proc_signal(struct ucred *cred, struct proc *p,
	    int signum);
int	mac_check_proc_wait(struct ucred *cred, struct proc *p);
int	mac_check_socket_accept(struct ucred *cred, struct socket *so);
int	mac_check_socket_bind(struct ucred *cred, struct socket *so,
	    struct sockaddr *sa);
int	mac_check_socket_connect(struct ucred *cred, struct socket *so,
	    struct sockaddr *sa);
int	mac_check_socket_create(struct ucred *cred, int domain, int type,
	    int proto);
int	mac_check_socket_deliver(struct socket *so, struct mbuf *m);
int	mac_check_socket_listen(struct ucred *cred, struct socket *so);
int	mac_check_socket_poll(struct ucred *cred, struct socket *so);
int	mac_check_socket_receive(struct ucred *cred, struct socket *so);
int	mac_check_socket_send(struct ucred *cred, struct socket *so);
int	mac_check_socket_stat(struct ucred *cred, struct socket *so);
int	mac_check_socket_visible(struct ucred *cred, struct socket *so);
int	mac_check_system_acct(struct ucred *cred, struct vnode *vp);
int	mac_check_system_audit(struct ucred *cred, void *record, int length);
int	mac_check_system_auditctl(struct ucred *cred, struct vnode *vp);
int	mac_check_system_auditon(struct ucred *cred, int cmd);
int	mac_check_system_reboot(struct ucred *cred, int howto);
int	mac_check_system_swapon(struct ucred *cred, struct vnode *vp);
int	mac_check_system_swapoff(struct ucred *cred, struct vnode *vp);
int	mac_check_system_sysctl(struct ucred *cred, struct sysctl_oid *oidp,
	    void *arg1, int arg2, struct sysctl_req *req);
int	mac_check_vnode_access(struct ucred *cred, struct vnode *vp,
	    int acc_mode);
int	mac_check_vnode_chdir(struct ucred *cred, struct vnode *dvp);
int	mac_check_vnode_chroot(struct ucred *cred, struct vnode *dvp);
int	mac_check_vnode_create(struct ucred *cred, struct vnode *dvp,
	    struct componentname *cnp, struct vattr *vap);
int	mac_check_vnode_deleteacl(struct ucred *cred, struct vnode *vp,
	    acl_type_t type);
int	mac_check_vnode_deleteextattr(struct ucred *cred, struct vnode *vp,
	    int attrnamespace, const char *name);
int	mac_check_vnode_exec(struct ucred *cred, struct vnode *vp,
	    struct image_params *imgp);
int	mac_check_vnode_getacl(struct ucred *cred, struct vnode *vp,
	    acl_type_t type);
int	mac_check_vnode_getextattr(struct ucred *cred, struct vnode *vp,
	    int attrnamespace, const char *name, struct uio *uio);
int	mac_check_vnode_link(struct ucred *cred, struct vnode *dvp,
	    struct vnode *vp, struct componentname *cnp);
int	mac_check_vnode_listextattr(struct ucred *cred, struct vnode *vp,
	    int attrnamespace);
int	mac_check_vnode_lookup(struct ucred *cred, struct vnode *dvp,
 	    struct componentname *cnp);
int	mac_check_vnode_mmap(struct ucred *cred, struct vnode *vp, int prot,
	    int flags);
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
int	mac_check_vnode_unlink(struct ucred *cred, struct vnode *dvp,
	    struct vnode *vp, struct componentname *cnp);
int	mac_check_vnode_write(struct ucred *active_cred,
	    struct ucred *file_cred, struct vnode *vp);
int	mac_getsockopt_label(struct ucred *cred, struct socket *so,
	    struct mac *extmac);
int	mac_getsockopt_peerlabel(struct ucred *cred, struct socket *so,
	    struct mac *extmac);
int	mac_ioctl_ifnet_get(struct ucred *cred, struct ifreq *ifr,
	    struct ifnet *ifp);
int	mac_ioctl_ifnet_set(struct ucred *cred, struct ifreq *ifr,
	    struct ifnet *ifp);
int	mac_setsockopt_label(struct ucred *cred, struct socket *so,
	    struct mac *extmac);
int	mac_pipe_label_set(struct ucred *cred, struct pipepair *pp,
	    struct label *label);
void	mac_cred_mmapped_drop_perms(struct thread *td, struct ucred *cred);
void	mac_associate_nfsd_label(struct ucred *cred);
int	mac_priv_check(struct ucred *cred, int priv);
int	mac_priv_grant(struct ucred *cred, int priv);

/*
 * Calls to help various file systems implement labeling functionality using
 * their existing EA implementation.
 */
int	vop_stdsetlabel_ea(struct vop_setlabel_args *ap);

#endif /* !_SECURITY_MAC_MAC_FRAMEWORK_H_ */
