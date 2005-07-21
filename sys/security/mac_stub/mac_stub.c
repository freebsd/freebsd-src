/*-
 * Copyright (c) 1999-2002 Robert N. M. Watson
 * Copyright (c) 2001-2005 McAfee, Inc.
 * Copyright (c) 2005 SPARTA, Inc.
 * All rights reserved.
 *
 * This software was developed by Robert Watson for the TrustedBSD Project.
 *
 * This software was developed for the FreeBSD Project in part by McAfee
 * Research, the Security Research Division of McAfee, Inc. under
 * DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the DARPA
 * CHATS research program.
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
 * Developed by the TrustedBSD Project.
 *
 * Stub module that implements a NOOP for most (if not all) MAC Framework
 * policy entry points.
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
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>

#include <posix4/ksem.h>

#include <fs/devfs/devfs.h>

#include <net/bpfdesc.h>
#include <net/if.h>
#include <net/if_types.h>
#include <net/if_var.h>

#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>

#include <vm/vm.h>

#include <sys/mac_policy.h>

SYSCTL_DECL(_security_mac);

SYSCTL_NODE(_security_mac, OID_AUTO, stub, CTLFLAG_RW, 0,
    "TrustedBSD mac_stub policy controls");

static int	stub_enabled = 1;
SYSCTL_INT(_security_mac_stub, OID_AUTO, enabled, CTLFLAG_RW,
    &stub_enabled, 0, "Enforce mac_stub policy");

/*
 * Policy module operations.
 */
static void
stub_destroy(struct mac_policy_conf *conf)
{

}

static void
stub_init(struct mac_policy_conf *conf)
{

}

static int
stub_syscall(struct thread *td, int call, void *arg)
{

	return (0);
}

/*
 * Label operations.
 */
static void
stub_init_label(struct label *label)
{

}

static int
stub_init_label_waitcheck(struct label *label, int flag)
{

	return (0);
}

static void
stub_destroy_label(struct label *label)
{

}

static void
stub_copy_label(struct label *src, struct label *dest)
{

}

static int
stub_externalize_label(struct label *label, char *element_name,
    struct sbuf *sb, int *claimed)
{

	return (0);
}

static int
stub_internalize_label(struct label *label, char *element_name,
    char *element_data, int *claimed)
{

	return (0);
}

/*
 * Labeling event operations: file system objects, and things that look
 * a lot like file system objects.
 */
static void
stub_associate_vnode_devfs(struct mount *mp, struct label *fslabel,
    struct devfs_dirent *de, struct label *delabel, struct vnode *vp,
    struct label *vlabel)
{

}

static int
stub_associate_vnode_extattr(struct mount *mp, struct label *fslabel,
    struct vnode *vp, struct label *vlabel)
{

	return (0);
}

static void
stub_associate_vnode_singlelabel(struct mount *mp,
    struct label *fslabel, struct vnode *vp, struct label *vlabel)
{

}

static void
stub_create_devfs_device(struct ucred *cred, struct mount *mp,
    struct cdev *dev, struct devfs_dirent *devfs_dirent, struct label *label)
{

}

static void
stub_create_devfs_directory(struct mount *mp, char *dirname,
    int dirnamelen, struct devfs_dirent *devfs_dirent, struct label *label)
{

}

static void
stub_create_devfs_symlink(struct ucred *cred, struct mount *mp,
    struct devfs_dirent *dd, struct label *ddlabel, struct devfs_dirent *de,
    struct label *delabel)
{

}

static int
stub_create_vnode_extattr(struct ucred *cred, struct mount *mp,
    struct label *fslabel, struct vnode *dvp, struct label *dlabel,
    struct vnode *vp, struct label *vlabel, struct componentname *cnp)
{

	return (0);
}

static void
stub_create_mount(struct ucred *cred, struct mount *mp,
    struct label *mntlabel, struct label *fslabel)
{

}

static void
stub_create_root_mount(struct ucred *cred, struct mount *mp,
    struct label *mntlabel, struct label *fslabel)
{

}

static void
stub_relabel_vnode(struct ucred *cred, struct vnode *vp,
    struct label *vnodelabel, struct label *label)
{

}

static int
stub_setlabel_vnode_extattr(struct ucred *cred, struct vnode *vp,
    struct label *vlabel, struct label *intlabel)
{

	return (0);
}

static void
stub_update_devfsdirent(struct mount *mp,
    struct devfs_dirent *devfs_dirent, struct label *direntlabel,
    struct vnode *vp, struct label *vnodelabel)
{

}

/*
 * Labeling event operations: IPC object.
 */
static void
stub_create_mbuf_from_socket(struct socket *so, struct label *socketlabel,
    struct mbuf *m, struct label *mbuflabel)
{

}

static void
stub_create_socket(struct ucred *cred, struct socket *socket,
    struct label *socketlabel)
{

}

static void
stub_create_pipe(struct ucred *cred, struct pipepair *pp,
    struct label *pipelabel)
{

}

static void
stub_create_posix_sem(struct ucred *cred, struct ksem *ksemptr,
    struct label *ks_label)
{

}

static void
stub_create_socket_from_socket(struct socket *oldsocket,
    struct label *oldsocketlabel, struct socket *newsocket,
    struct label *newsocketlabel)
{

}

static void
stub_relabel_socket(struct ucred *cred, struct socket *socket,
    struct label *socketlabel, struct label *newlabel)
{

}

static void
stub_relabel_pipe(struct ucred *cred, struct pipepair *pp,
    struct label *pipelabel, struct label *newlabel)
{

}

static void
stub_set_socket_peer_from_mbuf(struct mbuf *mbuf, struct label *mbuflabel,
    struct socket *socket, struct label *socketpeerlabel)
{

}

static void
stub_set_socket_peer_from_socket(struct socket *oldsocket,
    struct label *oldsocketlabel, struct socket *newsocket,
    struct label *newsocketpeerlabel)
{

}

/*
 * Labeling event operations: network objects.
 */
static void
stub_create_bpfdesc(struct ucred *cred, struct bpf_d *bpf_d,
    struct label *bpflabel)
{

}

static void
stub_create_datagram_from_ipq(struct ipq *ipq, struct label *ipqlabel,
    struct mbuf *datagram, struct label *datagramlabel)
{

}

static void
stub_create_fragment(struct mbuf *datagram, struct label *datagramlabel,
    struct mbuf *fragment, struct label *fragmentlabel)
{

}

static void
stub_create_ifnet(struct ifnet *ifnet, struct label *ifnetlabel)
{

}

static void
stub_create_inpcb_from_socket(struct socket *so, struct label *solabel,
    struct inpcb *inp, struct label *inplabel)
{

}

static void
stub_create_sysv_msgmsg(struct ucred *cred, struct msqid_kernel *msqkptr,
    struct label *msqlabel, struct msg *msgptr, struct label *msglabel)
{

}

static void
stub_create_sysv_msgqueue(struct ucred *cred, struct msqid_kernel *msqkptr,
    struct label *msqlabel)
{

}

static void
stub_create_sysv_sem(struct ucred *cred, struct semid_kernel *semakptr,
    struct label *semalabel)
{

}

static void
stub_create_sysv_shm(struct ucred *cred, struct shmid_kernel *shmsegptr,
    struct label *shmalabel)
{

}

static void
stub_create_ipq(struct mbuf *fragment, struct label *fragmentlabel,
    struct ipq *ipq, struct label *ipqlabel)
{

}

static void
stub_create_mbuf_from_inpcb(struct inpcb *inp, struct label *inplabel,
    struct mbuf *m, struct label *mlabel)
{

}

static void
stub_create_mbuf_linklayer(struct ifnet *ifnet, struct label *ifnetlabel,
    struct mbuf *mbuf, struct label *mbuflabel)
{

}

static void
stub_create_mbuf_from_bpfdesc(struct bpf_d *bpf_d, struct label *bpflabel,
    struct mbuf *mbuf, struct label *mbuflabel)
{

}

static void
stub_create_mbuf_from_ifnet(struct ifnet *ifnet, struct label *ifnetlabel,
    struct mbuf *m, struct label *mbuflabel)
{

}

static void
stub_create_mbuf_multicast_encap(struct mbuf *oldmbuf,
    struct label *oldmbuflabel, struct ifnet *ifnet, struct label *ifnetlabel,
    struct mbuf *newmbuf, struct label *newmbuflabel)
{

}

static void
stub_create_mbuf_netlayer(struct mbuf *oldmbuf,
    struct label *oldmbuflabel, struct mbuf *newmbuf, struct label *newmbuflabel)
{

}

static int
stub_fragment_match(struct mbuf *fragment, struct label *fragmentlabel,
    struct ipq *ipq, struct label *ipqlabel)
{

	return (1);
}

static void
stub_reflect_mbuf_icmp(struct mbuf *m, struct label *mlabel)
{

}

static void
stub_reflect_mbuf_tcp(struct mbuf *m, struct label *mlabel)
{

}

static void
stub_relabel_ifnet(struct ucred *cred, struct ifnet *ifnet,
    struct label *ifnetlabel, struct label *newlabel)
{

}

static void
stub_update_ipq(struct mbuf *fragment, struct label *fragmentlabel,
    struct ipq *ipq, struct label *ipqlabel)
{

}

static void
stub_inpcb_sosetlabel(struct socket *so, struct label *solabel,
    struct inpcb *inp, struct label *inplabel)
{

}

/*
 * Labeling event operations: processes.
 */
static void
stub_execve_transition(struct ucred *old, struct ucred *new,
    struct vnode *vp, struct label *vnodelabel,
    struct label *interpvnodelabel, struct image_params *imgp,
    struct label *execlabel)
{

}

static int
stub_execve_will_transition(struct ucred *old, struct vnode *vp,
    struct label *vnodelabel, struct label *interpvnodelabel,
    struct image_params *imgp, struct label *execlabel)
{

	return (0);
}

static void
stub_create_proc0(struct ucred *cred)
{

}

static void
stub_create_proc1(struct ucred *cred)
{

}

static void
stub_relabel_cred(struct ucred *cred, struct label *newlabel)
{

}

static void
stub_thread_userret(struct thread *td)
{

}

/*
 * Label cleanup/flush operations
 */
static void
stub_cleanup_sysv_msgmsg(struct label *msglabel)
{

}

static void
stub_cleanup_sysv_msgqueue(struct label *msqlabel)
{

}

static void
stub_cleanup_sysv_sem(struct label *semalabel)
{

}

static void
stub_cleanup_sysv_shm(struct label *shmlabel)
{

}

/*
 * Access control checks.
 */
static int
stub_check_bpfdesc_receive(struct bpf_d *bpf_d, struct label *bpflabel,
    struct ifnet *ifnet, struct label *ifnet_label)
{

        return (0);
}

static int
stub_check_cred_relabel(struct ucred *cred, struct label *newlabel)
{

	return (0);
}

static int
stub_check_cred_visible(struct ucred *u1, struct ucred *u2)
{

	return (0);
}

static int
stub_check_ifnet_relabel(struct ucred *cred, struct ifnet *ifnet,
    struct label *ifnetlabel, struct label *newlabel)
{

	return (0);
}

static int
stub_check_ifnet_transmit(struct ifnet *ifnet, struct label *ifnetlabel,
    struct mbuf *m, struct label *mbuflabel)
{

	return (0);
}

static int
stub_check_inpcb_deliver(struct inpcb *inp, struct label *inplabel,
    struct mbuf *m, struct label *mlabel)
{

	return (0);
}

static int
stub_check_sysv_msgmsq(struct ucred *cred, struct msg *msgptr,
    struct label *msglabel, struct msqid_kernel *msqkptr,
    struct label *msqklabel)
{

	return (0);
}

static int
stub_check_sysv_msgrcv(struct ucred *cred, struct msg *msgptr,
    struct label *msglabel)
{

	return (0);
}


static int
stub_check_sysv_msgrmid(struct ucred *cred, struct msg *msgptr,
    struct label *msglabel)
{

	return (0);
}


static int
stub_check_sysv_msqget(struct ucred *cred, struct msqid_kernel *msqkptr,
    struct label *msqklabel)
{

	return (0);
}


static int
stub_check_sysv_msqsnd(struct ucred *cred, struct msqid_kernel *msqkptr,
    struct label *msqklabel)
{

	return (0);
}

static int
stub_check_sysv_msqrcv(struct ucred *cred, struct msqid_kernel *msqkptr,
    struct label *msqklabel)
{

	return (0);
}


static int
stub_check_sysv_msqctl(struct ucred *cred, struct msqid_kernel *msqkptr,
    struct label *msqklabel, int cmd)
{

	return (0);
}


static int
stub_check_sysv_semctl(struct ucred *cred, struct semid_kernel *semakptr,
    struct label *semaklabel, int cmd)
{

	return (0);
}

static int
stub_check_sysv_semget(struct ucred *cred, struct semid_kernel *semakptr,
    struct label *semaklabel)
{

	return (0);
}


static int
stub_check_sysv_semop(struct ucred *cred, struct semid_kernel *semakptr,
    struct label *semaklabel, size_t accesstype)
{

	return (0);
}

static int
stub_check_sysv_shmat(struct ucred *cred, struct shmid_kernel *shmsegptr,
    struct label *shmseglabel, int shmflg)
{

	return (0);
}

static int
stub_check_sysv_shmctl(struct ucred *cred, struct shmid_kernel *shmsegptr,
    struct label *shmseglabel, int cmd)
{

	return (0);
}

static int
stub_check_sysv_shmdt(struct ucred *cred, struct shmid_kernel *shmsegptr,
    struct label *shmseglabel)
{

	return (0);
}


static int
stub_check_sysv_shmget(struct ucred *cred, struct shmid_kernel *shmsegptr,
    struct label *shmseglabel, int shmflg)
{

	return (0);
}

static int
stub_check_kenv_dump(struct ucred *cred)
{

	return (0);
}

static int
stub_check_kenv_get(struct ucred *cred, char *name)
{

	return (0);
}

static int
stub_check_kenv_set(struct ucred *cred, char *name, char *value)
{

	return (0);
}

static int
stub_check_kenv_unset(struct ucred *cred, char *name)
{

	return (0);
}

static int
stub_check_kld_load(struct ucred *cred, struct vnode *vp,
    struct label *vlabel)
{

	return (0);
}

static int
stub_check_kld_stat(struct ucred *cred)
{

	return (0);
}

static int
stub_check_kld_unload(struct ucred *cred)
{

	return (0);
}

static int
stub_check_mount_stat(struct ucred *cred, struct mount *mp,
    struct label *mntlabel)
{

	return (0);
}

static int
stub_check_pipe_ioctl(struct ucred *cred, struct pipepair *pp,
    struct label *pipelabel, unsigned long cmd, void /* caddr_t */ *data)
{

	return (0);
}

static int
stub_check_pipe_poll(struct ucred *cred, struct pipepair *pp,
    struct label *pipelabel)
{

	return (0);
}

static int
stub_check_pipe_read(struct ucred *cred, struct pipepair *pp,
    struct label *pipelabel)
{

	return (0);
}

static int
stub_check_pipe_relabel(struct ucred *cred, struct pipepair *pp,
    struct label *pipelabel, struct label *newlabel)
{

	return (0);
}

static int
stub_check_pipe_stat(struct ucred *cred, struct pipepair *pp,
    struct label *pipelabel)
{

	return (0);
}

static int
stub_check_pipe_write(struct ucred *cred, struct pipepair *pp,
    struct label *pipelabel)
{

	return (0);
}

static int
stub_check_posix_sem_destroy(struct ucred *cred, struct ksem *ksemptr,
    struct label *ks_label)
{

	return (0);
}

static int
stub_check_posix_sem_getvalue(struct ucred *cred, struct ksem *ksemptr,
    struct label *ks_label)
{

	return (0);
}

static int
stub_check_posix_sem_open(struct ucred *cred, struct ksem *ksemptr,
    struct label *ks_label)
{

	return (0);
}

static int
stub_check_posix_sem_post(struct ucred *cred, struct ksem *ksemptr,
    struct label *ks_label)
{

	return (0);
}

static int
stub_check_posix_sem_unlink(struct ucred *cred, struct ksem *ksemptr,
    struct label *ks_label)
{

	return (0);
}

static int
stub_check_posix_sem_wait(struct ucred *cred, struct ksem *ksemptr,
    struct label *ks_label)
{

	return (0);
}

static int
stub_check_proc_debug(struct ucred *cred, struct proc *proc)
{

	return (0);
}

static int
stub_check_proc_sched(struct ucred *cred, struct proc *proc)
{

	return (0);
}

static int
stub_check_proc_signal(struct ucred *cred, struct proc *proc, int signum)
{

	return (0);
}

static int
stub_check_proc_wait(struct ucred *cred, struct proc *proc)
{

	return (0);
}

static int
stub_check_proc_setuid(struct ucred *cred, uid_t uid)
{

	return (0);
}

static int
stub_check_proc_seteuid(struct ucred *cred, uid_t euid)
{

	return (0);
}

static int
stub_check_proc_setgid(struct ucred *cred, gid_t gid)
{

	return (0);
}

static int
stub_check_proc_setegid(struct ucred *cred, gid_t egid)
{

	return (0);
}

static int
stub_check_proc_setgroups(struct ucred *cred, int ngroups,
	gid_t *gidset)
{

	return (0);
}

static int
stub_check_proc_setreuid(struct ucred *cred, uid_t ruid, uid_t euid)
{

	return (0);
}

static int
stub_check_proc_setregid(struct ucred *cred, gid_t rgid, gid_t egid)
{

	return (0);
}

static int
stub_check_proc_setresuid(struct ucred *cred, uid_t ruid, uid_t euid,
	uid_t suid)
{

	return (0);
}

static int
stub_check_proc_setresgid(struct ucred *cred, gid_t rgid, gid_t egid,
	gid_t sgid)
{

	return (0);
}

static int
stub_check_socket_accept(struct ucred *cred, struct socket *socket,
    struct label *socketlabel)
{

	return (0);
}

static int
stub_check_socket_bind(struct ucred *cred, struct socket *socket,
    struct label *socketlabel, struct sockaddr *sockaddr)
{

	return (0);
}

static int
stub_check_socket_connect(struct ucred *cred, struct socket *socket,
    struct label *socketlabel, struct sockaddr *sockaddr)
{

	return (0);
}

static int
stub_check_socket_create(struct ucred *cred, int domain, int type,
    int protocol)
{

	return (0);
}

static int
stub_check_socket_deliver(struct socket *so, struct label *socketlabel,
    struct mbuf *m, struct label *mbuflabel)
{

	return (0);
}

static int
stub_check_socket_listen(struct ucred *cred, struct socket *so,
    struct label *socketlabel)
{

	return (0);
}

static int
stub_check_socket_poll(struct ucred *cred, struct socket *so,
    struct label *socketlabel)
{

	return (0);
}

static int
stub_check_socket_receive(struct ucred *cred, struct socket *so,
    struct label *socketlabel)
{

	return (0);
}

static int
stub_check_socket_relabel(struct ucred *cred, struct socket *socket,
    struct label *socketlabel, struct label *newlabel)
{

	return (0);
}
static int
stub_check_socket_send(struct ucred *cred, struct socket *so,
    struct label *socketlabel)
{

	return (0);
}

static int
stub_check_socket_stat(struct ucred *cred, struct socket *so,
    struct label *socketlabel)
{

	return (0);
}

static int
stub_check_socket_visible(struct ucred *cred, struct socket *socket,
   struct label *socketlabel)
{

	return (0);
}

static int
stub_check_sysarch_ioperm(struct ucred *cred)
{

	return (0);
}

static int
stub_check_system_acct(struct ucred *cred, struct vnode *vp,
    struct label *vlabel)
{

	return (0);
}

static int
stub_check_system_reboot(struct ucred *cred, int how)
{

	return (0);
}

static int
stub_check_system_settime(struct ucred *cred)
{

	return (0);
}

static int
stub_check_system_swapon(struct ucred *cred, struct vnode *vp,
    struct label *label)
{

	return (0);
}

static int
stub_check_system_swapoff(struct ucred *cred, struct vnode *vp,
    struct label *label)
{

	return (0);
}

static int
stub_check_system_sysctl(struct ucred *cred, struct sysctl_oid *oidp,
    void *arg1, int arg2, struct sysctl_req *req)
{

	return (0);
}

static int
stub_check_vnode_access(struct ucred *cred, struct vnode *vp,
    struct label *label, int acc_mode)
{

	return (0);
}

static int
stub_check_vnode_chdir(struct ucred *cred, struct vnode *dvp,
    struct label *dlabel)
{

	return (0);
}

static int
stub_check_vnode_chroot(struct ucred *cred, struct vnode *dvp,
    struct label *dlabel)
{

	return (0);
}

static int
stub_check_vnode_create(struct ucred *cred, struct vnode *dvp,
    struct label *dlabel, struct componentname *cnp, struct vattr *vap)
{

	return (0);
}

static int
stub_check_vnode_delete(struct ucred *cred, struct vnode *dvp,
    struct label *dlabel, struct vnode *vp, struct label *label,
    struct componentname *cnp)
{

	return (0);
}

static int
stub_check_vnode_deleteacl(struct ucred *cred, struct vnode *vp,
    struct label *label, acl_type_t type)
{

	return (0);
}

static int
stub_check_vnode_deleteextattr(struct ucred *cred, struct vnode *vp,
    struct label *label, int attrnamespace, const char *name)
{

	return (0);
}

static int
stub_check_vnode_exec(struct ucred *cred, struct vnode *vp,
    struct label *label, struct image_params *imgp,
    struct label *execlabel)
{

	return (0);
}

static int
stub_check_vnode_getacl(struct ucred *cred, struct vnode *vp,
    struct label *label, acl_type_t type)
{

	return (0);
}

static int
stub_check_vnode_getextattr(struct ucred *cred, struct vnode *vp,
    struct label *label, int attrnamespace, const char *name, struct uio *uio)
{

	return (0);
}

static int
stub_check_vnode_link(struct ucred *cred, struct vnode *dvp,
    struct label *dlabel, struct vnode *vp, struct label *label,
    struct componentname *cnp)
{

	return (0);
}

static int
stub_check_vnode_listextattr(struct ucred *cred, struct vnode *vp,
    struct label *label, int attrnamespace)
{

	return (0);
}

static int
stub_check_vnode_lookup(struct ucred *cred, struct vnode *dvp,
    struct label *dlabel, struct componentname *cnp)
{

	return (0);
}

static int
stub_check_vnode_mmap(struct ucred *cred, struct vnode *vp,
    struct label *label, int prot, int flags)
{

	return (0);
}

static int
stub_check_vnode_open(struct ucred *cred, struct vnode *vp,
    struct label *filelabel, int acc_mode)
{

	return (0);
}

static int
stub_check_vnode_poll(struct ucred *active_cred, struct ucred *file_cred,
    struct vnode *vp, struct label *label)
{

	return (0);
}

static int
stub_check_vnode_read(struct ucred *active_cred, struct ucred *file_cred,
    struct vnode *vp, struct label *label)
{

	return (0);
}

static int
stub_check_vnode_readdir(struct ucred *cred, struct vnode *vp,
    struct label *dlabel)
{

	return (0);
}

static int
stub_check_vnode_readlink(struct ucred *cred, struct vnode *vp,
    struct label *vnodelabel)
{

	return (0);
}

static int
stub_check_vnode_relabel(struct ucred *cred, struct vnode *vp,
    struct label *vnodelabel, struct label *newlabel)
{

	return (0);
}

static int
stub_check_vnode_rename_from(struct ucred *cred, struct vnode *dvp,
    struct label *dlabel, struct vnode *vp, struct label *label,
    struct componentname *cnp)
{

	return (0);
}

static int
stub_check_vnode_rename_to(struct ucred *cred, struct vnode *dvp,
    struct label *dlabel, struct vnode *vp, struct label *label, int samedir,
    struct componentname *cnp)
{

	return (0);
}

static int
stub_check_vnode_revoke(struct ucred *cred, struct vnode *vp,
    struct label *label)
{

	return (0);
}

static int
stub_check_vnode_setacl(struct ucred *cred, struct vnode *vp,
    struct label *label, acl_type_t type, struct acl *acl)
{

	return (0);
}

static int
stub_check_vnode_setextattr(struct ucred *cred, struct vnode *vp,
    struct label *label, int attrnamespace, const char *name, struct uio *uio)
{

	return (0);
}

static int
stub_check_vnode_setflags(struct ucred *cred, struct vnode *vp,
    struct label *label, u_long flags)
{

	return (0);
}

static int
stub_check_vnode_setmode(struct ucred *cred, struct vnode *vp,
    struct label *label, mode_t mode)
{

	return (0);
}

static int
stub_check_vnode_setowner(struct ucred *cred, struct vnode *vp,
    struct label *label, uid_t uid, gid_t gid)
{

	return (0);
}

static int
stub_check_vnode_setutimes(struct ucred *cred, struct vnode *vp,
    struct label *label, struct timespec atime, struct timespec mtime)
{

	return (0);
}

static int
stub_check_vnode_stat(struct ucred *active_cred, struct ucred *file_cred,
    struct vnode *vp, struct label *label)
{

	return (0);
}

static int
stub_check_vnode_write(struct ucred *active_cred,
    struct ucred *file_cred, struct vnode *vp, struct label *label)
{

	return (0);
}

static struct mac_policy_ops mac_stub_ops =
{
	.mpo_destroy = stub_destroy,
	.mpo_init = stub_init,
	.mpo_syscall = stub_syscall,
	.mpo_init_bpfdesc_label = stub_init_label,
	.mpo_init_cred_label = stub_init_label,
	.mpo_init_devfsdirent_label = stub_init_label,
	.mpo_init_ifnet_label = stub_init_label,
	.mpo_init_inpcb_label = stub_init_label_waitcheck,
	.mpo_init_sysv_msgmsg_label = stub_init_label,
	.mpo_init_sysv_msgqueue_label = stub_init_label,
	.mpo_init_sysv_sem_label = stub_init_label,
	.mpo_init_sysv_shm_label = stub_init_label,
	.mpo_init_ipq_label = stub_init_label_waitcheck,
	.mpo_init_mbuf_label = stub_init_label_waitcheck,
	.mpo_init_mount_label = stub_init_label,
	.mpo_init_mount_fs_label = stub_init_label,
	.mpo_init_pipe_label = stub_init_label,
	.mpo_init_posix_sem_label = stub_init_label,
	.mpo_init_socket_label = stub_init_label_waitcheck,
	.mpo_init_socket_peer_label = stub_init_label_waitcheck,
	.mpo_init_vnode_label = stub_init_label,
	.mpo_destroy_bpfdesc_label = stub_destroy_label,
	.mpo_destroy_cred_label = stub_destroy_label,
	.mpo_destroy_devfsdirent_label = stub_destroy_label,
	.mpo_destroy_ifnet_label = stub_destroy_label,
	.mpo_destroy_inpcb_label = stub_destroy_label,
	.mpo_destroy_sysv_msgmsg_label = stub_destroy_label,
	.mpo_destroy_sysv_msgqueue_label = stub_destroy_label,
	.mpo_destroy_sysv_sem_label = stub_destroy_label,
	.mpo_destroy_sysv_shm_label = stub_destroy_label,
	.mpo_destroy_ipq_label = stub_destroy_label,
	.mpo_destroy_mbuf_label = stub_destroy_label,
	.mpo_destroy_mount_label = stub_destroy_label,
	.mpo_destroy_mount_fs_label = stub_destroy_label,
	.mpo_destroy_pipe_label = stub_destroy_label,
	.mpo_destroy_posix_sem_label = stub_destroy_label,
	.mpo_destroy_socket_label = stub_destroy_label,
	.mpo_destroy_socket_peer_label = stub_destroy_label,
	.mpo_destroy_vnode_label = stub_destroy_label,
	.mpo_copy_cred_label = stub_copy_label,
	.mpo_copy_ifnet_label = stub_copy_label,
	.mpo_copy_mbuf_label = stub_copy_label,
	.mpo_copy_pipe_label = stub_copy_label,
	.mpo_copy_socket_label = stub_copy_label,
	.mpo_copy_vnode_label = stub_copy_label,
	.mpo_externalize_cred_label = stub_externalize_label,
	.mpo_externalize_ifnet_label = stub_externalize_label,
	.mpo_externalize_pipe_label = stub_externalize_label,
	.mpo_externalize_socket_label = stub_externalize_label,
	.mpo_externalize_socket_peer_label = stub_externalize_label,
	.mpo_externalize_vnode_label = stub_externalize_label,
	.mpo_internalize_cred_label = stub_internalize_label,
	.mpo_internalize_ifnet_label = stub_internalize_label,
	.mpo_internalize_pipe_label = stub_internalize_label,
	.mpo_internalize_socket_label = stub_internalize_label,
	.mpo_internalize_vnode_label = stub_internalize_label,
	.mpo_associate_vnode_devfs = stub_associate_vnode_devfs,
	.mpo_associate_vnode_extattr = stub_associate_vnode_extattr,
	.mpo_associate_vnode_singlelabel = stub_associate_vnode_singlelabel,
	.mpo_create_devfs_device = stub_create_devfs_device,
	.mpo_create_devfs_directory = stub_create_devfs_directory,
	.mpo_create_devfs_symlink = stub_create_devfs_symlink,
	.mpo_create_sysv_msgmsg = stub_create_sysv_msgmsg,
	.mpo_create_sysv_msgqueue = stub_create_sysv_msgqueue,
	.mpo_create_sysv_sem = stub_create_sysv_sem,
	.mpo_create_sysv_shm = stub_create_sysv_shm,
	.mpo_create_vnode_extattr = stub_create_vnode_extattr,
	.mpo_create_mount = stub_create_mount,
	.mpo_create_root_mount = stub_create_root_mount,
	.mpo_relabel_vnode = stub_relabel_vnode,
	.mpo_setlabel_vnode_extattr = stub_setlabel_vnode_extattr,
	.mpo_update_devfsdirent = stub_update_devfsdirent,
	.mpo_create_mbuf_from_socket = stub_create_mbuf_from_socket,
	.mpo_create_pipe = stub_create_pipe,
	.mpo_create_posix_sem = stub_create_posix_sem,
	.mpo_create_socket = stub_create_socket,
	.mpo_create_socket_from_socket = stub_create_socket_from_socket,
	.mpo_relabel_pipe = stub_relabel_pipe,
	.mpo_relabel_socket = stub_relabel_socket,
	.mpo_set_socket_peer_from_mbuf = stub_set_socket_peer_from_mbuf,
	.mpo_set_socket_peer_from_socket = stub_set_socket_peer_from_socket,
	.mpo_create_bpfdesc = stub_create_bpfdesc,
	.mpo_create_ifnet = stub_create_ifnet,
	.mpo_create_inpcb_from_socket = stub_create_inpcb_from_socket,
	.mpo_create_ipq = stub_create_ipq,
	.mpo_create_datagram_from_ipq = stub_create_datagram_from_ipq,
	.mpo_create_fragment = stub_create_fragment,
	.mpo_create_mbuf_from_inpcb = stub_create_mbuf_from_inpcb,
	.mpo_create_mbuf_linklayer = stub_create_mbuf_linklayer,
	.mpo_create_mbuf_from_bpfdesc = stub_create_mbuf_from_bpfdesc,
	.mpo_create_mbuf_from_ifnet = stub_create_mbuf_from_ifnet,
	.mpo_create_mbuf_multicast_encap = stub_create_mbuf_multicast_encap,
	.mpo_create_mbuf_netlayer = stub_create_mbuf_netlayer,
	.mpo_fragment_match = stub_fragment_match,
	.mpo_reflect_mbuf_icmp = stub_reflect_mbuf_icmp,
	.mpo_reflect_mbuf_tcp = stub_reflect_mbuf_tcp,
	.mpo_relabel_ifnet = stub_relabel_ifnet,
	.mpo_update_ipq = stub_update_ipq,
	.mpo_inpcb_sosetlabel = stub_inpcb_sosetlabel,
	.mpo_execve_transition = stub_execve_transition,
	.mpo_execve_will_transition = stub_execve_will_transition,
	.mpo_create_proc0 = stub_create_proc0,
	.mpo_create_proc1 = stub_create_proc1,
	.mpo_relabel_cred = stub_relabel_cred,
	.mpo_thread_userret = stub_thread_userret,
	.mpo_cleanup_sysv_msgmsg = stub_cleanup_sysv_msgmsg,
	.mpo_cleanup_sysv_msgqueue = stub_cleanup_sysv_msgqueue,
	.mpo_cleanup_sysv_sem = stub_cleanup_sysv_sem,
	.mpo_cleanup_sysv_shm = stub_cleanup_sysv_shm,
	.mpo_check_bpfdesc_receive = stub_check_bpfdesc_receive,
	.mpo_check_cred_relabel = stub_check_cred_relabel,
	.mpo_check_cred_visible = stub_check_cred_visible,
	.mpo_check_ifnet_relabel = stub_check_ifnet_relabel,
	.mpo_check_ifnet_transmit = stub_check_ifnet_transmit,
	.mpo_check_inpcb_deliver = stub_check_inpcb_deliver,
	.mpo_check_sysv_msgmsq = stub_check_sysv_msgmsq,
	.mpo_check_sysv_msgrcv = stub_check_sysv_msgrcv,
	.mpo_check_sysv_msgrmid = stub_check_sysv_msgrmid,
	.mpo_check_sysv_msqget = stub_check_sysv_msqget,
	.mpo_check_sysv_msqsnd = stub_check_sysv_msqsnd,
	.mpo_check_sysv_msqrcv = stub_check_sysv_msqrcv,
	.mpo_check_sysv_msqctl = stub_check_sysv_msqctl,
	.mpo_check_sysv_semctl = stub_check_sysv_semctl,
	.mpo_check_sysv_semget = stub_check_sysv_semget,
	.mpo_check_sysv_semop = stub_check_sysv_semop,
	.mpo_check_sysv_shmat = stub_check_sysv_shmat,
	.mpo_check_sysv_shmctl = stub_check_sysv_shmctl,
	.mpo_check_sysv_shmdt = stub_check_sysv_shmdt,
	.mpo_check_sysv_shmget = stub_check_sysv_shmget,
	.mpo_check_kenv_dump = stub_check_kenv_dump,
	.mpo_check_kenv_get = stub_check_kenv_get,
	.mpo_check_kenv_set = stub_check_kenv_set,
	.mpo_check_kenv_unset = stub_check_kenv_unset,
	.mpo_check_kld_load = stub_check_kld_load,
	.mpo_check_kld_stat = stub_check_kld_stat,
	.mpo_check_kld_unload = stub_check_kld_unload,
	.mpo_check_mount_stat = stub_check_mount_stat,
	.mpo_check_pipe_ioctl = stub_check_pipe_ioctl,
	.mpo_check_pipe_poll = stub_check_pipe_poll,
	.mpo_check_pipe_read = stub_check_pipe_read,
	.mpo_check_pipe_relabel = stub_check_pipe_relabel,
	.mpo_check_pipe_stat = stub_check_pipe_stat,
	.mpo_check_pipe_write = stub_check_pipe_write,
	.mpo_check_posix_sem_destroy = stub_check_posix_sem_destroy,
	.mpo_check_posix_sem_getvalue = stub_check_posix_sem_getvalue,
	.mpo_check_posix_sem_open = stub_check_posix_sem_open,
	.mpo_check_posix_sem_post = stub_check_posix_sem_post,
	.mpo_check_posix_sem_unlink = stub_check_posix_sem_unlink,
	.mpo_check_posix_sem_wait = stub_check_posix_sem_wait,
	.mpo_check_proc_debug = stub_check_proc_debug,
	.mpo_check_proc_sched = stub_check_proc_sched,
	.mpo_check_proc_setuid = stub_check_proc_setuid,
	.mpo_check_proc_seteuid = stub_check_proc_seteuid,
	.mpo_check_proc_setgid = stub_check_proc_setgid,
	.mpo_check_proc_setegid = stub_check_proc_setegid,
	.mpo_check_proc_setgroups = stub_check_proc_setgroups,
	.mpo_check_proc_setreuid = stub_check_proc_setreuid,
	.mpo_check_proc_setregid = stub_check_proc_setregid,
	.mpo_check_proc_setresuid = stub_check_proc_setresuid,
	.mpo_check_proc_setresgid = stub_check_proc_setresgid,
	.mpo_check_proc_signal = stub_check_proc_signal,
	.mpo_check_proc_wait = stub_check_proc_wait,
	.mpo_check_socket_accept = stub_check_socket_accept,
	.mpo_check_socket_bind = stub_check_socket_bind,
	.mpo_check_socket_connect = stub_check_socket_connect,
	.mpo_check_socket_create = stub_check_socket_create,
	.mpo_check_socket_deliver = stub_check_socket_deliver,
	.mpo_check_socket_listen = stub_check_socket_listen,
	.mpo_check_socket_poll = stub_check_socket_poll,
	.mpo_check_socket_receive = stub_check_socket_receive,
	.mpo_check_socket_relabel = stub_check_socket_relabel,
	.mpo_check_socket_send = stub_check_socket_send,
	.mpo_check_socket_stat = stub_check_socket_stat,
	.mpo_check_socket_visible = stub_check_socket_visible,
	.mpo_check_sysarch_ioperm = stub_check_sysarch_ioperm,
	.mpo_check_system_acct = stub_check_system_acct,
	.mpo_check_system_reboot = stub_check_system_reboot,
	.mpo_check_system_settime = stub_check_system_settime,
	.mpo_check_system_swapon = stub_check_system_swapon,
	.mpo_check_system_swapoff = stub_check_system_swapoff,
	.mpo_check_system_sysctl = stub_check_system_sysctl,
	.mpo_check_vnode_access = stub_check_vnode_access,
	.mpo_check_vnode_chdir = stub_check_vnode_chdir,
	.mpo_check_vnode_chroot = stub_check_vnode_chroot,
	.mpo_check_vnode_create = stub_check_vnode_create,
	.mpo_check_vnode_delete = stub_check_vnode_delete,
	.mpo_check_vnode_deleteacl = stub_check_vnode_deleteacl,
	.mpo_check_vnode_deleteextattr = stub_check_vnode_deleteextattr,
	.mpo_check_vnode_exec = stub_check_vnode_exec,
	.mpo_check_vnode_getacl = stub_check_vnode_getacl,
	.mpo_check_vnode_getextattr = stub_check_vnode_getextattr,
	.mpo_check_vnode_link = stub_check_vnode_link,
	.mpo_check_vnode_listextattr = stub_check_vnode_listextattr,
	.mpo_check_vnode_lookup = stub_check_vnode_lookup,
	.mpo_check_vnode_mmap = stub_check_vnode_mmap,
	.mpo_check_vnode_open = stub_check_vnode_open,
	.mpo_check_vnode_poll = stub_check_vnode_poll,
	.mpo_check_vnode_read = stub_check_vnode_read,
	.mpo_check_vnode_readdir = stub_check_vnode_readdir,
	.mpo_check_vnode_readlink = stub_check_vnode_readlink,
	.mpo_check_vnode_relabel = stub_check_vnode_relabel,
	.mpo_check_vnode_rename_from = stub_check_vnode_rename_from,
	.mpo_check_vnode_rename_to = stub_check_vnode_rename_to,
	.mpo_check_vnode_revoke = stub_check_vnode_revoke,
	.mpo_check_vnode_setacl = stub_check_vnode_setacl,
	.mpo_check_vnode_setextattr = stub_check_vnode_setextattr,
	.mpo_check_vnode_setflags = stub_check_vnode_setflags,
	.mpo_check_vnode_setmode = stub_check_vnode_setmode,
	.mpo_check_vnode_setowner = stub_check_vnode_setowner,
	.mpo_check_vnode_setutimes = stub_check_vnode_setutimes,
	.mpo_check_vnode_stat = stub_check_vnode_stat,
	.mpo_check_vnode_write = stub_check_vnode_write,
};

MAC_POLICY_SET(&mac_stub_ops, mac_stub, "TrustedBSD MAC/Stub",
    MPC_LOADTIME_FLAG_UNLOADOK, NULL);
