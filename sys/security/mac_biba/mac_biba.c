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
 * Developed by the TrustedBSD Project.
 * Biba fixed label mandatory integrity policy.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/acl.h>
#include <sys/conf.h>
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

#include <security/mac_biba/mac_biba.h>

SYSCTL_DECL(_security_mac);

SYSCTL_NODE(_security_mac, OID_AUTO, biba, CTLFLAG_RW, 0,
    "TrustedBSD mac_biba policy controls");

static int	mac_biba_enabled = 0;
SYSCTL_INT(_security_mac_biba, OID_AUTO, enabled, CTLFLAG_RW,
    &mac_biba_enabled, 0, "Enforce MAC/Biba policy");

static int	destroyed_not_inited;
SYSCTL_INT(_security_mac_biba, OID_AUTO, destroyed_not_inited, CTLFLAG_RD,
    &destroyed_not_inited, 0, "Count of labels destroyed but not inited");

static int	trust_all_interfaces = 0;
SYSCTL_INT(_security_mac_biba, OID_AUTO, trust_all_interfaces, CTLFLAG_RD,
    &trust_all_interfaces, 0, "Consider all interfaces 'trusted' by MAC/Biba");
TUNABLE_INT("security.mac.biba.trust_all_interfaces", &trust_all_interfaces);

static char	trusted_interfaces[128];
SYSCTL_STRING(_security_mac_biba, OID_AUTO, trusted_interfaces, CTLFLAG_RD,
    trusted_interfaces, 0, "Interfaces considered 'trusted' by MAC/Biba");
TUNABLE_STR("security.mac.biba.trusted_interfaces", trusted_interfaces,
    sizeof(trusted_interfaces));

static int	mac_biba_revocation_enabled = 0;
SYSCTL_INT(_security_mac_biba, OID_AUTO, revocation_enabled, CTLFLAG_RW,
    &mac_biba_revocation_enabled, 0, "Revoke access to objects on relabel");
TUNABLE_INT("security.mac.biba.revocation_enabled",
    &mac_biba_revocation_enabled);

static int	mac_biba_slot;
#define	SLOT(l)	((struct mac_biba *)LABEL_TO_SLOT((l), mac_biba_slot).l_ptr)

MALLOC_DEFINE(M_MACBIBA, "biba label", "MAC/Biba labels");

static int	mac_biba_check_vnode_open(struct ucred *cred, struct vnode *vp,
		    struct label *vnodelabel, mode_t acc_mode);

static struct mac_biba *
biba_alloc(int how)
{
	struct mac_biba *mac_biba;

	mac_biba = malloc(sizeof(struct mac_biba), M_MACBIBA, M_ZERO | how);

	return (mac_biba);
}

static void
biba_free(struct mac_biba *mac_biba)
{

	if (mac_biba != NULL)
		free(mac_biba, M_MACBIBA);
	else
		atomic_add_int(&destroyed_not_inited, 1);
}

static int
mac_biba_dominate_element(struct mac_biba_element *a,
    struct mac_biba_element *b)
{

	switch(a->mbe_type) {
	case MAC_BIBA_TYPE_EQUAL:
	case MAC_BIBA_TYPE_HIGH:
		return (1);

	case MAC_BIBA_TYPE_LOW:
		switch (b->mbe_type) {
		case MAC_BIBA_TYPE_GRADE:
		case MAC_BIBA_TYPE_HIGH:
			return (0);

		case MAC_BIBA_TYPE_EQUAL:
		case MAC_BIBA_TYPE_LOW:
			return (1);

		default:
			panic("mac_biba_dominate_element: b->mbe_type invalid");
		}

	case MAC_BIBA_TYPE_GRADE:
		switch (b->mbe_type) {
		case MAC_BIBA_TYPE_EQUAL:
		case MAC_BIBA_TYPE_LOW:
			return (1);

		case MAC_BIBA_TYPE_HIGH:
			return (0);

		case MAC_BIBA_TYPE_GRADE:
			return (a->mbe_grade >= b->mbe_grade);

		default:
			panic("mac_biba_dominate_element: b->mbe_type invalid");
		}

	default:
		panic("mac_biba_dominate_element: a->mbe_type invalid");
	}

	return (0);
}

static int
mac_biba_range_in_range(struct mac_biba *rangea, struct mac_biba *rangeb)
{

	return (mac_biba_dominate_element(&rangeb->mb_rangehigh,
	    &rangea->mb_rangehigh) &&
	    mac_biba_dominate_element(&rangea->mb_rangelow,
	    &rangeb->mb_rangelow));
}

static int
mac_biba_single_in_range(struct mac_biba *single, struct mac_biba *range)
{

	KASSERT((single->mb_flag & MAC_BIBA_FLAG_SINGLE) != 0,
	    ("mac_biba_single_in_range: a not single"));
	KASSERT((range->mb_flag & MAC_BIBA_FLAG_RANGE) != 0,
	    ("mac_biba_single_in_range: b not range"));

	return (mac_biba_dominate_element(&range->mb_rangehigh,
	    &single->mb_single) &&
	    mac_biba_dominate_element(&single->mb_single,
	    &range->mb_rangelow));

	return (1);
}

static int
mac_biba_dominate_single(struct mac_biba *a, struct mac_biba *b)
{
	KASSERT((a->mb_flags & MAC_BIBA_FLAG_SINGLE) != 0,
	    ("mac_biba_dominate_single: a not single"));
	KASSERT((b->mb_flags & MAC_BIBA_FLAG_SINGLE) != 0,
	    ("mac_biba_dominate_single: b not single"));

	return (mac_biba_dominate_element(&a->mb_single, &b->mb_single));
}

static int
mac_biba_equal_element(struct mac_biba_element *a, struct mac_biba_element *b)
{

	if (a->mbe_type == MAC_BIBA_TYPE_EQUAL ||
	    b->mbe_type == MAC_BIBA_TYPE_EQUAL)
		return (1);

	return (a->mbe_type == b->mbe_type && a->mbe_grade == b->mbe_grade);
}

static int
mac_biba_equal_range(struct mac_biba *a, struct mac_biba *b)
{

	KASSERT((a->mb_flags & MAC_BIBA_FLAG_RANGE) != 0,
	    ("mac_biba_equal_range: a not range"));
	KASSERT((b->mb_flags & MAC_BIBA_FLAG_RANGE) != 0,
	    ("mac_biba_equal_range: b not range"));

	return (mac_biba_equal_element(&a->mb_rangelow, &b->mb_rangelow) &&
	    mac_biba_equal_element(&a->mb_rangehigh, &b->mb_rangehigh));
}

static int
mac_biba_equal_single(struct mac_biba *a, struct mac_biba *b)
{

	KASSERT((a->mb_flags & MAC_BIBA_FLAG_SINGLE) != 0,
	    ("mac_biba_equal_single: a not single"));
	KASSERT((b->mb_flags & MAC_BIBA_FLAG_SINGLE) != 0,
	    ("mac_biba_equal_single: b not single"));

	return (mac_biba_equal_element(&a->mb_single, &b->mb_single));
}

static int
mac_biba_high_single(struct mac_biba *mac_biba)
{

	return (mac_biba->mb_single.mbe_type == MAC_BIBA_TYPE_HIGH);
}

static int
mac_biba_valid(struct mac_biba *mac_biba)
{

	if (mac_biba->mb_flags & MAC_BIBA_FLAG_SINGLE) {
		switch (mac_biba->mb_single.mbe_type) {
		case MAC_BIBA_TYPE_GRADE:
			break;

		case MAC_BIBA_TYPE_EQUAL:
		case MAC_BIBA_TYPE_HIGH:
		case MAC_BIBA_TYPE_LOW:
			if (mac_biba->mb_single.mbe_grade != 0)
				return (EINVAL);
			break;

		default:
			return (EINVAL);
		}
	} else {
		if (mac_biba->mb_single.mbe_type != MAC_BIBA_TYPE_UNDEF)
			return (EINVAL);
	}

	if (mac_biba->mb_flags & MAC_BIBA_FLAG_RANGE) {
		switch (mac_biba->mb_rangelow.mbe_type) {
		case MAC_BIBA_TYPE_GRADE:
			break;

		case MAC_BIBA_TYPE_EQUAL:
		case MAC_BIBA_TYPE_HIGH:
		case MAC_BIBA_TYPE_LOW:
			if (mac_biba->mb_rangelow.mbe_grade != 0)
				return (EINVAL);
			break;

		default:
			return (EINVAL);
		}

		switch (mac_biba->mb_rangehigh.mbe_type) {
		case MAC_BIBA_TYPE_GRADE:
			break;

		case MAC_BIBA_TYPE_EQUAL:
		case MAC_BIBA_TYPE_HIGH:
		case MAC_BIBA_TYPE_LOW:
			if (mac_biba->mb_rangehigh.mbe_grade != 0)
				return (EINVAL);
			break;

		default:
			return (EINVAL);
		}
		if (!mac_biba_dominate_element(&mac_biba->mb_rangehigh,
		    &mac_biba->mb_rangelow))
			return (EINVAL);
	} else {
		if (mac_biba->mb_rangelow.mbe_type != MAC_BIBA_TYPE_UNDEF ||
		    mac_biba->mb_rangehigh.mbe_type != MAC_BIBA_TYPE_UNDEF)
			return (EINVAL);
	}

	return (0);
}

static void
mac_biba_set_range(struct mac_biba *mac_biba, u_short typelow,
    u_short gradelow, u_short typehigh, u_short gradehigh)
{

	mac_biba->mb_rangelow.mbe_type = typelow;
	mac_biba->mb_rangelow.mbe_grade = gradelow;
	mac_biba->mb_rangehigh.mbe_type = typehigh;
	mac_biba->mb_rangehigh.mbe_grade = gradehigh;
	mac_biba->mb_flags |= MAC_BIBA_FLAG_RANGE;
}

static void
mac_biba_set_single(struct mac_biba *mac_biba, u_short type, u_short grade)
{

	mac_biba->mb_single.mbe_type = type;
	mac_biba->mb_single.mbe_grade = grade;
	mac_biba->mb_flags |= MAC_BIBA_FLAG_SINGLE;
}

static void
mac_biba_copy_range(struct mac_biba *labelfrom, struct mac_biba *labelto)
{
	KASSERT((labelfrom->mb_flags & MAC_BIBA_FLAG_RANGE) != 0,
	    ("mac_biba_copy_range: labelfrom not range"));

	labelto->mb_rangelow = labelfrom->mb_rangelow;
	labelto->mb_rangehigh = labelfrom->mb_rangehigh;
	labelto->mb_flags |= MAC_BIBA_FLAG_RANGE;
}

static void
mac_biba_copy_single(struct mac_biba *labelfrom, struct mac_biba *labelto)
{

	KASSERT((labelfrom->mb_flags & MAC_BIBA_FLAG_SINGLE) != 0,
	    ("mac_biba_copy_single: labelfrom not single"));

	labelto->mb_single = labelfrom->mb_single;
	labelto->mb_flags |= MAC_BIBA_FLAG_SINGLE;
}

static void
mac_biba_copy_single_to_range(struct mac_biba *labelfrom,
    struct mac_biba *labelto)
{

	KASSERT((labelfrom->mb_flags & MAC_BIBA_FLAG_SINGLE) != 0,
	    ("mac_biba_copy_single_to_range: labelfrom not single"));

	labelto->mb_rangelow = labelfrom->mb_single;
	labelto->mb_rangehigh = labelfrom->mb_single;
	labelto->mb_flags |= MAC_BIBA_FLAG_RANGE;
}

/*
 * Policy module operations.
 */
static void
mac_biba_destroy(struct mac_policy_conf *conf)
{

}

static void
mac_biba_init(struct mac_policy_conf *conf)
{

}

/*
 * Label operations.
 */
static void
mac_biba_init_bpfdesc(struct bpf_d *bpf_d, struct label *label)
{

	SLOT(label) = biba_alloc(M_WAITOK);
}

static void
mac_biba_init_cred(struct ucred *ucred, struct label *label)
{

	SLOT(label) = biba_alloc(M_WAITOK);
}

static void
mac_biba_init_devfsdirent(struct devfs_dirent *devfs_dirent,
    struct label *label)
{

	SLOT(label) = biba_alloc(M_WAITOK);
}

static void
mac_biba_init_ifnet(struct ifnet *ifnet, struct label *label)
{

	SLOT(label) = biba_alloc(M_WAITOK);
}

static void
mac_biba_init_ipq(struct ipq *ipq, struct label *label)
{

	SLOT(label) = biba_alloc(M_WAITOK);
}

static int
mac_biba_init_mbuf(struct mbuf *mbuf, int how, struct label *label)
{

	SLOT(label) = biba_alloc(how);
	if (SLOT(label) == NULL)
		return (ENOMEM);

	return (0);
}

static void
mac_biba_init_mount(struct mount *mount, struct label *mntlabel,
    struct label *fslabel)
{

	SLOT(mntlabel) = biba_alloc(M_WAITOK);
	SLOT(fslabel) = biba_alloc(M_WAITOK);
}

static void
mac_biba_init_socket(struct socket *socket, struct label *label,
    struct label *peerlabel)
{

	SLOT(label) = biba_alloc(M_WAITOK);
	SLOT(peerlabel) = biba_alloc(M_WAITOK);
}

static void
mac_biba_init_pipe(struct pipe *pipe, struct label *label)
{

	SLOT(label) = biba_alloc(M_WAITOK);
}

static void
mac_biba_init_temp(struct label *label)
{

	SLOT(label) = biba_alloc(M_WAITOK);
}

static void
mac_biba_init_vnode(struct vnode *vp, struct label *label)
{

	SLOT(label) = biba_alloc(M_WAITOK);
}

static void
mac_biba_destroy_bpfdesc(struct bpf_d *bpf_d, struct label *label)
{

	biba_free(SLOT(label));
	SLOT(label) = NULL;
}

static void
mac_biba_destroy_cred(struct ucred *ucred, struct label *label)
{

	biba_free(SLOT(label));
	SLOT(label) = NULL;
}

static void
mac_biba_destroy_devfsdirent(struct devfs_dirent *devfs_dirent,
    struct label *label)
{

	biba_free(SLOT(label));
	SLOT(label) = NULL;
}

static void
mac_biba_destroy_ifnet(struct ifnet *ifnet, struct label *label)
{

	biba_free(SLOT(label));
	SLOT(label) = NULL;
}

static void
mac_biba_destroy_ipq(struct ipq *ipq, struct label *label)
{

	biba_free(SLOT(label));
	SLOT(label) = NULL;
}

static void
mac_biba_destroy_mbuf(struct mbuf *mbuf, struct label *label)
{

	biba_free(SLOT(label));
	SLOT(label) = NULL;
}

static void
mac_biba_destroy_mount(struct mount *mount, struct label *mntlabel,
    struct label *fslabel)
{

	biba_free(SLOT(mntlabel));
	SLOT(mntlabel) = NULL;
	biba_free(SLOT(fslabel));
	SLOT(fslabel) = NULL;
}

static void
mac_biba_destroy_socket(struct socket *socket, struct label *label,
    struct label *peerlabel)
{

	biba_free(SLOT(label));
	SLOT(label) = NULL;
	biba_free(SLOT(peerlabel));
	SLOT(peerlabel) = NULL;
}

static void
mac_biba_destroy_pipe(struct pipe *pipe, struct label *label)
{

	biba_free(SLOT(label));
	SLOT(label) = NULL;
}

static void
mac_biba_destroy_temp(struct label *label)
{

	biba_free(SLOT(label));
	SLOT(label) = NULL;
}

static void
mac_biba_destroy_vnode(struct vnode *vp, struct label *label)
{

	biba_free(SLOT(label));
	SLOT(label) = NULL;
}

static int
mac_biba_externalize(struct label *label, struct mac *extmac)
{
	struct mac_biba *mac_biba;

	mac_biba = SLOT(label);

	if (mac_biba == NULL) {
		printf("mac_biba_externalize: NULL pointer\n");
		return (0);
	}

	extmac->m_biba = *mac_biba;

	return (0);
}

static int
mac_biba_internalize(struct label *label, struct mac *extmac)
{
	struct mac_biba *mac_biba;
	int error;

	mac_biba = SLOT(label);

	error = mac_biba_valid(mac_biba);
	if (error)
		return (error);

	*mac_biba = extmac->m_biba;

	return (0);
}

/*
 * Labeling event operations: file system objects, and things that look
 * a lot like file system objects.
 */
static void
mac_biba_create_devfs_device(dev_t dev, struct devfs_dirent *devfs_dirent,
    struct label *label)
{
	struct mac_biba *mac_biba;
	int biba_type;

	mac_biba = SLOT(label);
	if (strcmp(dev->si_name, "null") == 0 ||
	    strcmp(dev->si_name, "zero") == 0 ||
	    strcmp(dev->si_name, "random") == 0 ||
	    strncmp(dev->si_name, "fd/", strlen("fd/")) == 0)
		biba_type = MAC_BIBA_TYPE_EQUAL;
	else
		biba_type = MAC_BIBA_TYPE_HIGH;
	mac_biba_set_single(mac_biba, biba_type, 0);
}

static void
mac_biba_create_devfs_directory(char *dirname, int dirnamelen,
    struct devfs_dirent *devfs_dirent, struct label *label)
{
	struct mac_biba *mac_biba;

	mac_biba = SLOT(label);
	mac_biba_set_single(mac_biba, MAC_BIBA_TYPE_HIGH, 0);
}

static void
mac_biba_create_devfs_vnode(struct devfs_dirent *devfs_dirent,
    struct label *direntlabel, struct vnode *vp, struct label *vnodelabel)
{
	struct mac_biba *source, *dest;

	source = SLOT(direntlabel);
	dest = SLOT(vnodelabel);
	mac_biba_copy_single(source, dest);
}

static void
mac_biba_create_vnode(struct ucred *cred, struct vnode *parent,
    struct label *parentlabel, struct vnode *child, struct label *childlabel)
{
	struct mac_biba *source, *dest;

	source = SLOT(&cred->cr_label);
	dest = SLOT(childlabel);

	mac_biba_copy_single(source, dest);
}

static void
mac_biba_create_mount(struct ucred *cred, struct mount *mp,
    struct label *mntlabel, struct label *fslabel)
{
	struct mac_biba *source, *dest;

	source = SLOT(&cred->cr_label);
	dest = SLOT(mntlabel);
	mac_biba_copy_single(source, dest);
	dest = SLOT(fslabel);
	mac_biba_copy_single(source, dest);
}

static void
mac_biba_create_root_mount(struct ucred *cred, struct mount *mp,
    struct label *mntlabel, struct label *fslabel)
{
	struct mac_biba *mac_biba;

	/* Always mount root as high integrity. */
	mac_biba = SLOT(fslabel);
	mac_biba_set_single(mac_biba, MAC_BIBA_TYPE_HIGH, 0);
	mac_biba = SLOT(mntlabel);
	mac_biba_set_single(mac_biba, MAC_BIBA_TYPE_HIGH, 0);
}

static void
mac_biba_relabel_vnode(struct ucred *cred, struct vnode *vp,
    struct label *vnodelabel, struct label *label)
{
	struct mac_biba *source, *dest;

	source = SLOT(label);
	dest = SLOT(vnodelabel);

	mac_biba_copy_single(source, dest);
}

static void
mac_biba_update_devfsdirent(struct devfs_dirent *devfs_dirent,
    struct label *direntlabel, struct vnode *vp, struct label *vnodelabel)
{
	struct mac_biba *source, *dest;

	source = SLOT(vnodelabel);
	dest = SLOT(direntlabel);

	mac_biba_copy_single(source, dest);
}

static void
mac_biba_update_procfsvnode(struct vnode *vp, struct label *vnodelabel,
    struct ucred *cred)
{
	struct mac_biba *source, *dest;

	source = SLOT(&cred->cr_label);
	dest = SLOT(vnodelabel);

	/*
	 * Only copy the single, not the range, since vnodes only have
	 * a single.
	 */
	mac_biba_copy_single(source, dest);
}

static int
mac_biba_update_vnode_from_externalized(struct vnode *vp,
    struct label *vnodelabel, struct mac *extmac)
{
	struct mac_biba *source, *dest;
	int error;

	source = &extmac->m_biba;
	dest = SLOT(vnodelabel);

	error = mac_biba_valid(source);
	if (error)
		return (error);

	if ((source->mb_flags & MAC_BIBA_FLAGS_BOTH) != MAC_BIBA_FLAG_SINGLE)
		return (EINVAL);

	mac_biba_copy_single(source, dest);

	return (0);
}

static void
mac_biba_update_vnode_from_mount(struct vnode *vp, struct label *vnodelabel,
    struct mount *mp, struct label *fslabel)
{
	struct mac_biba *source, *dest;

	source = SLOT(fslabel);
	dest = SLOT(vnodelabel);

	mac_biba_copy_single(source, dest);
}

/*
 * Labeling event operations: IPC object.
 */
static void
mac_biba_create_mbuf_from_socket(struct socket *so, struct label *socketlabel,
    struct mbuf *m, struct label *mbuflabel)
{
	struct mac_biba *source, *dest;

	source = SLOT(socketlabel);
	dest = SLOT(mbuflabel);

	mac_biba_copy_single(source, dest);
}

static void
mac_biba_create_socket(struct ucred *cred, struct socket *socket,
    struct label *socketlabel)
{
	struct mac_biba *source, *dest;

	source = SLOT(&cred->cr_label);
	dest = SLOT(socketlabel);

	mac_biba_copy_single(source, dest);
	mac_biba_copy_single_to_range(source, dest);
}

static void
mac_biba_create_pipe(struct ucred *cred, struct pipe *pipe,
    struct label *pipelabel)
{
	struct mac_biba *source, *dest;

	source = SLOT(&cred->cr_label);
	dest = SLOT(pipelabel);

	mac_biba_copy_single(source, dest);
}

static void
mac_biba_create_socket_from_socket(struct socket *oldsocket,
    struct label *oldsocketlabel, struct socket *newsocket,
    struct label *newsocketlabel)
{
	struct mac_biba *source, *dest;

	source = SLOT(oldsocketlabel);
	dest = SLOT(newsocketlabel);

	mac_biba_copy_single(source, dest);
	mac_biba_copy_range(source, dest);
}

static void
mac_biba_relabel_socket(struct ucred *cred, struct socket *socket,
    struct label *socketlabel, struct label *newlabel)
{
	struct mac_biba *source, *dest;

	source = SLOT(newlabel);
	dest = SLOT(socketlabel);

	mac_biba_copy_single(source, dest);
	mac_biba_copy_range(source, dest);
}

static void
mac_biba_relabel_pipe(struct ucred *cred, struct pipe *pipe,
    struct label *pipelabel, struct label *newlabel)
{
	struct mac_biba *source, *dest;

	source = SLOT(newlabel);
	dest = SLOT(pipelabel);

	mac_biba_copy_single(source, dest);
}

static void
mac_biba_set_socket_peer_from_mbuf(struct mbuf *mbuf, struct label *mbuflabel,
    struct socket *socket, struct label *socketpeerlabel)
{
	struct mac_biba *source, *dest;

	source = SLOT(mbuflabel);
	dest = SLOT(socketpeerlabel);

	mac_biba_copy_single(source, dest);
}

/*
 * Labeling event operations: network objects.
 */
static void
mac_biba_set_socket_peer_from_socket(struct socket *oldsocket,
    struct label *oldsocketlabel, struct socket *newsocket,
    struct label *newsocketpeerlabel)
{
	struct mac_biba *source, *dest;

	source = SLOT(oldsocketlabel);
	dest = SLOT(newsocketpeerlabel);

	mac_biba_copy_single(source, dest);
}

static void
mac_biba_create_bpfdesc(struct ucred *cred, struct bpf_d *bpf_d,
    struct label *bpflabel)
{
	struct mac_biba *source, *dest;

	source = SLOT(&cred->cr_label);
	dest = SLOT(bpflabel);

	mac_biba_copy_single(source, dest);
}

static void
mac_biba_create_ifnet(struct ifnet *ifnet, struct label *ifnetlabel)
{
	char tifname[IFNAMSIZ], ifname[IFNAMSIZ], *p, *q;
	char tiflist[sizeof(trusted_interfaces)];
	struct mac_biba *dest;
	int len, grade;

	dest = SLOT(ifnetlabel);

	if (ifnet->if_type == IFT_LOOP) {
		grade = MAC_BIBA_TYPE_EQUAL;
		goto set;
	}

	if (trust_all_interfaces) {
		grade = MAC_BIBA_TYPE_HIGH;
		goto set;
	}

	grade = MAC_BIBA_TYPE_LOW;

	if (trusted_interfaces[0] == '\0' ||
	    !strvalid(trusted_interfaces, sizeof(trusted_interfaces)))
		goto set;

	for (p = trusted_interfaces, q = tiflist; *p != '\0'; p++, q++)
		if(*p != ' ' && *p != '\t')
			*q = *p;

	snprintf(ifname, IFNAMSIZ, "%s%d", ifnet->if_name, ifnet->if_unit);

	for (p = q = tiflist;; p++) {
		if (*p == ',' || *p == '\0') {
			len = p - q;
			if (len < IFNAMSIZ) {
				bzero(tifname, sizeof(tifname));
				bcopy(q, tifname, len);
				if (strcmp(tifname, ifname) == 0) {
					grade = MAC_BIBA_TYPE_HIGH;
					break;
				}
			}
			if (*p == '\0')
				break;
			q = p + 1;
		}
	}
set:
	mac_biba_set_single(dest, grade, 0);
	mac_biba_set_range(dest, grade, 0, grade, 0);
}

static void
mac_biba_create_ipq(struct mbuf *fragment, struct label *fragmentlabel,
    struct ipq *ipq, struct label *ipqlabel)
{
	struct mac_biba *source, *dest;

	source = SLOT(fragmentlabel);
	dest = SLOT(ipqlabel);

	mac_biba_copy_single(source, dest);
}

static void
mac_biba_create_datagram_from_ipq(struct ipq *ipq, struct label *ipqlabel,
    struct mbuf *datagram, struct label *datagramlabel)
{
	struct mac_biba *source, *dest;

	source = SLOT(ipqlabel);
	dest = SLOT(datagramlabel);

	/* Just use the head, since we require them all to match. */
	mac_biba_copy_single(source, dest);
}

static void
mac_biba_create_fragment(struct mbuf *datagram, struct label *datagramlabel,
    struct mbuf *fragment, struct label *fragmentlabel)
{
	struct mac_biba *source, *dest;

	source = SLOT(datagramlabel);
	dest = SLOT(fragmentlabel);

	mac_biba_copy_single(source, dest);
}

static void
mac_biba_create_mbuf_from_mbuf(struct mbuf *oldmbuf,
    struct label *oldmbuflabel, struct mbuf *newmbuf,
    struct label *newmbuflabel)
{
	struct mac_biba *source, *dest;

	source = SLOT(oldmbuflabel);
	dest = SLOT(newmbuflabel);

	mac_biba_copy_single(source, dest);
}

static void
mac_biba_create_mbuf_linklayer(struct ifnet *ifnet, struct label *ifnetlabel,
    struct mbuf *mbuf, struct label *mbuflabel)
{
	struct mac_biba *dest;

	dest = SLOT(mbuflabel);

	mac_biba_set_single(dest, MAC_BIBA_TYPE_EQUAL, 0);
}

static void
mac_biba_create_mbuf_from_bpfdesc(struct bpf_d *bpf_d, struct label *bpflabel,
    struct mbuf *mbuf, struct label *mbuflabel)
{
	struct mac_biba *source, *dest;

	source = SLOT(bpflabel);
	dest = SLOT(mbuflabel);

	mac_biba_copy_single(source, dest);
}

static void
mac_biba_create_mbuf_from_ifnet(struct ifnet *ifnet, struct label *ifnetlabel,
    struct mbuf *m, struct label *mbuflabel)
{
	struct mac_biba *source, *dest;

	source = SLOT(ifnetlabel);
	dest = SLOT(mbuflabel);

	mac_biba_copy_single(source, dest);
}

static void
mac_biba_create_mbuf_multicast_encap(struct mbuf *oldmbuf,
    struct label *oldmbuflabel, struct ifnet *ifnet, struct label *ifnetlabel,
    struct mbuf *newmbuf, struct label *newmbuflabel)
{
	struct mac_biba *source, *dest;

	source = SLOT(oldmbuflabel);
	dest = SLOT(newmbuflabel);

	mac_biba_copy_single(source, dest);
}

static void
mac_biba_create_mbuf_netlayer(struct mbuf *oldmbuf, struct label *oldmbuflabel,
    struct mbuf *newmbuf, struct label *newmbuflabel)
{
	struct mac_biba *source, *dest;

	source = SLOT(oldmbuflabel);
	dest = SLOT(newmbuflabel);

	mac_biba_copy_single(source, dest);
}

static int
mac_biba_fragment_match(struct mbuf *fragment, struct label *fragmentlabel,
    struct ipq *ipq, struct label *ipqlabel)
{
	struct mac_biba *a, *b;

	a = SLOT(ipqlabel);
	b = SLOT(fragmentlabel);

	return (mac_biba_equal_single(a, b));
}

static void
mac_biba_relabel_ifnet(struct ucred *cred, struct ifnet *ifnet,
    struct label *ifnetlabel, struct label *newlabel)
{
	struct mac_biba *source, *dest;

	source = SLOT(newlabel);
	dest = SLOT(ifnetlabel);

	mac_biba_copy_single(source, dest);
	mac_biba_copy_range(source, dest);
}

static void
mac_biba_update_ipq(struct mbuf *fragment, struct label *fragmentlabel,
    struct ipq *ipq, struct label *ipqlabel)
{

	/* NOOP: we only accept matching labels, so no need to update */
}

/*
 * Labeling event operations: processes.
 */
static void
mac_biba_create_cred(struct ucred *cred_parent, struct ucred *cred_child)
{
	struct mac_biba *source, *dest;

	source = SLOT(&cred_parent->cr_label);
	dest = SLOT(&cred_child->cr_label);

	mac_biba_copy_single(source, dest);
	mac_biba_copy_range(source, dest);
}

static void
mac_biba_execve_transition(struct ucred *old, struct ucred *new,
    struct vnode *vp, struct mac *vnodelabel)
{
	struct mac_biba *source, *dest;

	source = SLOT(&old->cr_label);
	dest = SLOT(&new->cr_label);

	mac_biba_copy_single(source, dest);
	mac_biba_copy_range(source, dest);
}

static int
mac_biba_execve_will_transition(struct ucred *old, struct vnode *vp,
    struct mac *vnodelabel)
{

	return (0);
}

static void
mac_biba_create_proc0(struct ucred *cred)
{
	struct mac_biba *dest;

	dest = SLOT(&cred->cr_label);

	mac_biba_set_single(dest, MAC_BIBA_TYPE_EQUAL, 0);
	mac_biba_set_range(dest, MAC_BIBA_TYPE_LOW, 0, MAC_BIBA_TYPE_HIGH, 0);
}

static void
mac_biba_create_proc1(struct ucred *cred)
{
	struct mac_biba *dest;

	dest = SLOT(&cred->cr_label);

	mac_biba_set_single(dest, MAC_BIBA_TYPE_HIGH, 0);
	mac_biba_set_range(dest, MAC_BIBA_TYPE_LOW, 0, MAC_BIBA_TYPE_HIGH, 0);
}

static void
mac_biba_relabel_cred(struct ucred *cred, struct label *newlabel)
{
	struct mac_biba *source, *dest;

	source = SLOT(newlabel);
	dest = SLOT(&cred->cr_label);

	mac_biba_copy_single(source, dest);
	mac_biba_copy_range(source, dest);
}

/*
 * Access control checks.
 */
static int
mac_biba_check_bpfdesc_receive(struct bpf_d *bpf_d, struct label *bpflabel,
    struct ifnet *ifnet, struct label *ifnetlabel)
{
	struct mac_biba *a, *b;

	if (!mac_biba_enabled)
		return (0);

	a = SLOT(bpflabel);
	b = SLOT(ifnetlabel);

	if (mac_biba_equal_single(a, b))
		return (0);
	return (EACCES);
}

static int
mac_biba_check_cred_relabel(struct ucred *cred, struct label *newlabel)
{
	struct mac_biba *subj, *new;

	subj = SLOT(&cred->cr_label);
	new = SLOT(newlabel);

	if ((new->mb_flags & MAC_BIBA_FLAGS_BOTH) != MAC_BIBA_FLAGS_BOTH)
		return (EINVAL);

	/*
	 * XXX: Allow processes with root privilege to set labels outside
	 * their range, so suid things like "su" work.  This WILL go away
	 * when we figure out the 'correct' solution...
	 */
	if (!suser_cred(cred, 0))
		return (0);

	/*
	 * The new single must be in the old range.
	 */
	if (!mac_biba_single_in_range(new, subj))
		return (EPERM);

	/*
	 * The new range must be in the old range.
	 */
	if (!mac_biba_range_in_range(new, subj))
		return (EPERM);

	/*
	 * XXX: Don't permit EQUAL in a label unless the subject has EQUAL.
	 */

	return (0);
}

static int
mac_biba_check_cred_visible(struct ucred *u1, struct ucred *u2)
{
	struct mac_biba *subj, *obj;

	if (!mac_biba_enabled)
		return (0);

	subj = SLOT(&u1->cr_label);
	obj = SLOT(&u2->cr_label);

	/* XXX: range */
	if (!mac_biba_dominate_single(obj, subj))
		return (ESRCH);

	return (0);
}

static int
mac_biba_check_ifnet_relabel(struct ucred *cred, struct ifnet *ifnet,
    struct label *ifnetlabel, struct label *newlabel)
{
	struct mac_biba *subj, *new;

	subj = SLOT(&cred->cr_label);
	new = SLOT(newlabel);

	if ((new->mb_flags & MAC_BIBA_FLAGS_BOTH) != MAC_BIBA_FLAGS_BOTH)
		return (EINVAL);

	/*
	 * XXX: Only Biba HIGH subjects may relabel interfaces. */
	if (!mac_biba_high_single(subj))
		return (EPERM);

	return (suser_cred(cred, 0));
}

static int 
mac_biba_check_ifnet_transmit(struct ifnet *ifnet, struct label *ifnetlabel,
    struct mbuf *m, struct label *mbuflabel)
{
	struct mac_biba *p, *i;
        
	if (!mac_biba_enabled)
		return (0);

	p = SLOT(mbuflabel);
	i = SLOT(ifnetlabel);
 
	return (mac_biba_single_in_range(p, i) ? 0 : EACCES);
}

static int
mac_biba_check_mount_stat(struct ucred *cred, struct mount *mp,
    struct label *mntlabel)
{
	struct mac_biba *subj, *obj;

	if (!mac_biba_enabled)
		return (0);

	subj = SLOT(&cred->cr_label);
	obj = SLOT(mntlabel);

	if (!mac_biba_dominate_single(obj, subj))
		return (EACCES);

	return (0);
}

static int
mac_biba_check_pipe_ioctl(struct ucred *cred, struct pipe *pipe,
    struct label *pipelabel, unsigned long cmd, void /* caddr_t */ *data)
{
	
	if(!mac_biba_enabled)
		return (0);

	/* XXX: This will be implemented soon... */

	return (0);
}

static int
mac_biba_check_pipe_op(struct ucred *cred, struct pipe *pipe,
    struct label *pipelabel, int op)
{
	struct mac_biba *subj, *obj;

	if (!mac_biba_enabled)
		return (0);

	subj = SLOT(&cred->cr_label);
	obj = SLOT((pipelabel));

	switch(op) {
	case MAC_OP_PIPE_READ:
	case MAC_OP_PIPE_STAT:
	case MAC_OP_PIPE_POLL:
		if (!mac_biba_dominate_single(obj, subj))
			return (EACCES);
		break;
	case MAC_OP_PIPE_WRITE:
		if (!mac_biba_dominate_single(subj, obj))
			return (EACCES);
		break;
	default:
		panic("mac_biba_check_pipe_op: invalid pipe operation");
	}

	return (0);
}

static int
mac_biba_check_pipe_relabel(struct ucred *cred, struct pipe *pipe,
    struct label *pipelabel, struct label *newlabel)
{
	struct mac_biba *subj, *obj, *new;

	new = SLOT(newlabel);
	subj = SLOT(&cred->cr_label);
	obj = SLOT(pipelabel);

	if ((new->mb_flags & MAC_BIBA_FLAGS_BOTH) != MAC_BIBA_FLAG_SINGLE)
		return (EINVAL);

	/*
	 * To relabel a pipe, the old pipe label must be in the subject
	 * range.
	 */
	if (!mac_biba_single_in_range(obj, subj))
		return (EPERM);

	/*
	 * To relabel a pipe, the new pipe label must be in the subject
	 * range.
	 */
	if (!mac_biba_single_in_range(new, subj))
		return (EPERM);

	/*
	 * XXX: Don't permit EQUAL in a label unless the subject has EQUAL.
	 */

	return (0);
}

static int
mac_biba_check_proc_debug(struct ucred *cred, struct proc *proc)
{
	struct mac_biba *subj, *obj;

	if (!mac_biba_enabled)
		return (0);

	subj = SLOT(&cred->cr_label);
	obj = SLOT(&proc->p_ucred->cr_label);

	/* XXX: range checks */
	if (!mac_biba_dominate_single(obj, subj))
		return (ESRCH);
	if (!mac_biba_dominate_single(subj, obj))
		return (EACCES);

	return (0);
}

static int
mac_biba_check_proc_sched(struct ucred *cred, struct proc *proc)
{
	struct mac_biba *subj, *obj;
 
	if (!mac_biba_enabled)
		return (0);

	subj = SLOT(&cred->cr_label);
	obj = SLOT(&proc->p_ucred->cr_label);
 
	/* XXX: range checks */
	if (!mac_biba_dominate_single(obj, subj))
		return (ESRCH);
	if (!mac_biba_dominate_single(subj, obj))
		return (EACCES);

	return (0);
}

static int
mac_biba_check_proc_signal(struct ucred *cred, struct proc *proc, int signum)
{
	struct mac_biba *subj, *obj;
 
	if (!mac_biba_enabled)
		return (0);

	subj = SLOT(&cred->cr_label);
	obj = SLOT(&proc->p_ucred->cr_label);
 
	/* XXX: range checks */
	if (!mac_biba_dominate_single(obj, subj))
		return (ESRCH);
	if (!mac_biba_dominate_single(subj, obj))
		return (EACCES);

	return (0);
}

static int
mac_biba_check_socket_receive(struct socket *so, struct label *socketlabel,
    struct mbuf *m, struct label *mbuflabel)
{
	struct mac_biba *p, *s;

	if (!mac_biba_enabled)
		return (0);

	p = SLOT(mbuflabel);
	s = SLOT(socketlabel);

	return (mac_biba_equal_single(p, s) ? 0 : EACCES);
}

static int
mac_biba_check_socket_relabel(struct ucred *cred, struct socket *socket,
    struct label *socketlabel, struct label *newlabel)
{
	struct mac_biba *subj, *obj, *new;

	new = SLOT(newlabel);
	subj = SLOT(&cred->cr_label);
	obj = SLOT(socketlabel);

	if ((new->mb_flags & MAC_BIBA_FLAGS_BOTH) != MAC_BIBA_FLAG_SINGLE)
		return (EINVAL);

	/*
	 * To relabel a socket, the old socket label must be in the subject
	 * range.
	 */
	if (!mac_biba_single_in_range(obj, subj))
		return (EPERM);

	/*
	 * To relabel a socket, the new socket label must be in the subject
	 * range.
	 */
	if (!mac_biba_single_in_range(new, subj))
		return (EPERM);

	/*
	 * XXX: Don't permit EQUAL in a label unless the subject has EQUAL.
	 */

	return (0);
}

static int
mac_biba_check_socket_visible(struct ucred *cred, struct socket *socket,
    struct label *socketlabel)
{
	struct mac_biba *subj, *obj;

	subj = SLOT(&cred->cr_label);
	obj = SLOT(socketlabel);

	if (!mac_biba_dominate_single(obj, subj))
		return (ENOENT);

	return (0);
}

static int
mac_biba_check_vnode_access(struct ucred *cred, struct vnode *vp,
    struct label *label, mode_t flags)
{

	return (mac_biba_check_vnode_open(cred, vp, label, flags));
}

static int
mac_biba_check_vnode_chdir(struct ucred *cred, struct vnode *dvp,
    struct label *dlabel)
{
	struct mac_biba *subj, *obj;

	if (!mac_biba_enabled)
		return (0);

	subj = SLOT(&cred->cr_label);
	obj = SLOT(dlabel);

	if (!mac_biba_dominate_single(obj, subj))
		return (EACCES);

	return (0);
}

static int
mac_biba_check_vnode_chroot(struct ucred *cred, struct vnode *dvp,
    struct label *dlabel)
{
	struct mac_biba *subj, *obj;

	if (!mac_biba_enabled)
		return (0);

	subj = SLOT(&cred->cr_label);
	obj = SLOT(dlabel);

	if (!mac_biba_dominate_single(obj, subj))
		return (EACCES);

	return (0);
}

static int
mac_biba_check_vnode_create(struct ucred *cred, struct vnode *dvp,
    struct label *dlabel, struct componentname *cnp, struct vattr *vap)
{
	struct mac_biba *subj, *obj;

	if (!mac_biba_enabled)
		return (0);

	subj = SLOT(&cred->cr_label);
	obj = SLOT(dlabel);

	if (!mac_biba_dominate_single(subj, obj))
		return (EACCES);

	return (0);
}

static int
mac_biba_check_vnode_delete(struct ucred *cred, struct vnode *dvp,
    struct label *dlabel, struct vnode *vp, struct label *label,
    struct componentname *cnp)
{
	struct mac_biba *subj, *obj;

	if (!mac_biba_enabled)
		return (0);

	subj = SLOT(&cred->cr_label);
	obj = SLOT(dlabel);

	if (!mac_biba_dominate_single(subj, obj))
		return (EACCES);

	obj = SLOT(label);

	if (!mac_biba_dominate_single(subj, obj))
		return (EACCES);

	return (0);
}

static int
mac_biba_check_vnode_deleteacl(struct ucred *cred, struct vnode *vp,
    struct label *label, acl_type_t type)
{
	struct mac_biba *subj, *obj;

	if (!mac_biba_enabled)
		return (0);

	subj = SLOT(&cred->cr_label);
	obj = SLOT(label);

	if (!mac_biba_dominate_single(subj, obj))
		return (EACCES);

	return (0);
}

static int
mac_biba_check_vnode_exec(struct ucred *cred, struct vnode *vp,
    struct label *label)
{
	struct mac_biba *subj, *obj;

	if (!mac_biba_enabled)
		return (0);

	subj = SLOT(&cred->cr_label);
	obj = SLOT(label);

	if (!mac_biba_dominate_single(obj, subj))
		return (EACCES);

	return (0);
}

static int
mac_biba_check_vnode_getacl(struct ucred *cred, struct vnode *vp,
    struct label *label, acl_type_t type)
{
	struct mac_biba *subj, *obj;

	if (!mac_biba_enabled)
		return (0);

	subj = SLOT(&cred->cr_label);
	obj = SLOT(label);

	if (!mac_biba_dominate_single(obj, subj))
		return (EACCES);

	return (0);
}

static int
mac_biba_check_vnode_getextattr(struct ucred *cred, struct vnode *vp,
    struct label *label, int attrnamespace, const char *name, struct uio *uio)
{
	struct mac_biba *subj, *obj;

	if (!mac_biba_enabled)
		return (0);

	subj = SLOT(&cred->cr_label);
	obj = SLOT(label);

	if (!mac_biba_dominate_single(obj, subj))
		return (EACCES);

	return (0);
}

static int
mac_biba_check_vnode_lookup(struct ucred *cred, struct vnode *dvp, 
    struct label *dlabel, struct componentname *cnp)
{
	struct mac_biba *subj, *obj;
 
	if (!mac_biba_enabled)
		return (0);
   
	subj = SLOT(&cred->cr_label);
	obj = SLOT(dlabel);
 
	if (!mac_biba_dominate_single(obj, subj))
		return (EACCES);

	return (0);   
}

static int
mac_biba_check_vnode_open(struct ucred *cred, struct vnode *vp,
    struct label *vnodelabel, mode_t acc_mode)
{
	struct mac_biba *subj, *obj;

	if (!mac_biba_enabled)
		return (0);

	subj = SLOT(&cred->cr_label);
	obj = SLOT(vnodelabel);

	/* XXX privilege override for admin? */
	if (acc_mode & (VREAD | VEXEC | VSTAT)) {
		if (!mac_biba_dominate_single(obj, subj))
			return (EACCES);
	}
	if (acc_mode & (VWRITE | VAPPEND | VADMIN)) {
		if (!mac_biba_dominate_single(subj, obj))
			return (EACCES);
	}

	return (0);
}

static int
mac_biba_check_vnode_readdir(struct ucred *cred, struct vnode *dvp,
    struct label *dlabel)
{
	struct mac_biba *subj, *obj;

	if (!mac_biba_enabled)
		return (0);

	subj = SLOT(&cred->cr_label);
	obj = SLOT(dlabel);

	if (!mac_biba_dominate_single(obj, subj))
		return (EACCES);

	return (0);
}

static int
mac_biba_check_vnode_readlink(struct ucred *cred, struct vnode *vp,
    struct label *label)
{
	struct mac_biba *subj, *obj;

	if (!mac_biba_enabled)
		return (0);

	subj = SLOT(&cred->cr_label);
	obj = SLOT(label);

	if (!mac_biba_dominate_single(obj, subj))
		return (EACCES);

	return (0);
}

static int
mac_biba_check_vnode_relabel(struct ucred *cred, struct vnode *vp,
    struct label *vnodelabel, struct label *newlabel)
{
	struct mac_biba *old, *new, *subj;

	old = SLOT(vnodelabel);
	new = SLOT(newlabel);
	subj = SLOT(&cred->cr_label);

	if ((new->mb_flags & MAC_BIBA_FLAGS_BOTH) != MAC_BIBA_FLAG_SINGLE)
		return (EINVAL);

	/*
	 * To relabel a vnode, the old vnode label must be in the subject
	 * range.
	 */
	if (!mac_biba_single_in_range(old, subj))
		return (EPERM);

	/*
	 * To relabel a vnode, the new vnode label must be in the subject
	 * range.
	 */
	if (!mac_biba_single_in_range(new, subj))
		return (EPERM);

	/*
	 * XXX: Don't permit EQUAL in a label unless the subject has EQUAL.
	 */

	return (suser_cred(cred, 0));
}

static int
mac_biba_check_vnode_rename_from(struct ucred *cred, struct vnode *dvp,
    struct label *dlabel, struct vnode *vp, struct label *label,
    struct componentname *cnp)
{
	struct mac_biba *subj, *obj;

	if (!mac_biba_enabled)
		return (0);

	subj = SLOT(&cred->cr_label);
	obj = SLOT(dlabel);

	if (!mac_biba_dominate_single(subj, obj))
		return (EACCES);

	obj = SLOT(label);

	if (!mac_biba_dominate_single(subj, obj))
		return (EACCES);

	return (0);
}

static int
mac_biba_check_vnode_rename_to(struct ucred *cred, struct vnode *dvp,
    struct label *dlabel, struct vnode *vp, struct label *label, int samedir,
    struct componentname *cnp)
{
	struct mac_biba *subj, *obj;

	if (!mac_biba_enabled)
		return (0);

	subj = SLOT(&cred->cr_label);
	obj = SLOT(dlabel);

	if (!mac_biba_dominate_single(subj, obj))
		return (EACCES);

	if (vp != NULL) {
		obj = SLOT(label);

		if (!mac_biba_dominate_single(subj, obj))
			return (EACCES);
	}

	return (0);
}

static int
mac_biba_check_vnode_revoke(struct ucred *cred, struct vnode *vp,
    struct label *label)
{
	struct mac_biba *subj, *obj;

	if (!mac_biba_enabled)
		return (0);

	subj = SLOT(&cred->cr_label);
	obj = SLOT(label);

	if (!mac_biba_dominate_single(subj, obj))
		return (EACCES);

	return (0);
}

static int
mac_biba_check_vnode_setacl(struct ucred *cred, struct vnode *vp,
    struct label *label, acl_type_t type, struct acl *acl)
{
	struct mac_biba *subj, *obj;

	if (!mac_biba_enabled)
		return (0);

	subj = SLOT(&cred->cr_label);
	obj = SLOT(label);

	if (!mac_biba_dominate_single(subj, obj))
		return (EACCES);

	return (0);
}

static int
mac_biba_check_vnode_setextattr(struct ucred *cred, struct vnode *vp,
    struct label *vnodelabel, int attrnamespace, const char *name,
    struct uio *uio)
{
	struct mac_biba *subj, *obj;

	if (!mac_biba_enabled)
		return (0);

	subj = SLOT(&cred->cr_label);
	obj = SLOT(vnodelabel);

	if (!mac_biba_dominate_single(subj, obj))
		return (EACCES);

	/* XXX: protect the MAC EA in a special way? */

	return (0);
}

static int
mac_biba_check_vnode_setflags(struct ucred *cred, struct vnode *vp,
    struct label *vnodelabel, u_long flags)
{
	struct mac_biba *subj, *obj;

	if (!mac_biba_enabled)
		return (0);

	subj = SLOT(&cred->cr_label);
	obj = SLOT(vnodelabel);

	if (!mac_biba_dominate_single(subj, obj))
		return (EACCES);

	return (0);
}

static int
mac_biba_check_vnode_setmode(struct ucred *cred, struct vnode *vp,
    struct label *vnodelabel, mode_t mode)
{
	struct mac_biba *subj, *obj;

	if (!mac_biba_enabled)
		return (0);

	subj = SLOT(&cred->cr_label);
	obj = SLOT(vnodelabel);

	if (!mac_biba_dominate_single(subj, obj))
		return (EACCES);

	return (0);
}

static int
mac_biba_check_vnode_setowner(struct ucred *cred, struct vnode *vp,
    struct label *vnodelabel, uid_t uid, gid_t gid)
{
	struct mac_biba *subj, *obj;

	if (!mac_biba_enabled)
		return (0);

	subj = SLOT(&cred->cr_label);
	obj = SLOT(vnodelabel);

	if (!mac_biba_dominate_single(subj, obj))
		return (EACCES);

	return (0);
}

static int
mac_biba_check_vnode_setutimes(struct ucred *cred, struct vnode *vp,
    struct label *vnodelabel, struct timespec atime, struct timespec mtime)
{
	struct mac_biba *subj, *obj;

	if (!mac_biba_enabled)
		return (0);

	subj = SLOT(&cred->cr_label);
	obj = SLOT(vnodelabel);

	if (!mac_biba_dominate_single(subj, obj))
		return (EACCES);

	return (0);
}

static int
mac_biba_check_vnode_stat(struct ucred *cred, struct vnode *vp,
    struct label *vnodelabel)
{
	struct mac_biba *subj, *obj;

	if (!mac_biba_enabled)
		return (0);

	subj = SLOT(&cred->cr_label);
	obj = SLOT(vnodelabel);

	if (!mac_biba_dominate_single(obj, subj))
		return (EACCES);

	return (0);
}

static vm_prot_t
mac_biba_check_vnode_mmap_perms(struct ucred *cred, struct vnode *vp,
    struct label *label, int newmapping)
{
	struct mac_biba *subj, *obj;
	vm_prot_t prot = 0;

	if (!mac_biba_enabled || (!mac_biba_revocation_enabled && !newmapping))
		return (VM_PROT_ALL);

	subj = SLOT(&cred->cr_label);
	obj = SLOT(label);

	if (mac_biba_dominate_single(obj, subj))
		prot |= VM_PROT_READ | VM_PROT_EXECUTE;
	if (mac_biba_dominate_single(subj, obj))
		prot |= VM_PROT_WRITE;
	return (prot);
}

static int
mac_biba_check_vnode_op(struct ucred *cred, struct vnode *vp,
    struct label *label, int op)
{
	struct mac_biba *subj, *obj;

	if (!mac_biba_enabled || !mac_biba_revocation_enabled)
		return (0);

	subj = SLOT(&cred->cr_label);
	obj = SLOT(label);

	switch (op) {
	case MAC_OP_VNODE_POLL:
	case MAC_OP_VNODE_READ:
		if (!mac_biba_dominate_single(obj, subj))
			return (EACCES);
		return (0);

	case MAC_OP_VNODE_WRITE:
		if (!mac_biba_dominate_single(subj, obj))
			return (EACCES);
		return (0);

	default:
		printf("mac_biba_check_vnode_op: unknown operation %d\n", op);
		return (EINVAL);
	}
}

static struct mac_policy_op_entry mac_biba_ops[] =
{
	{ MAC_DESTROY,
	    (macop_t)mac_biba_destroy },
	{ MAC_INIT,
	    (macop_t)mac_biba_init },
	{ MAC_INIT_BPFDESC,
	    (macop_t)mac_biba_init_bpfdesc },
	{ MAC_INIT_CRED,
	    (macop_t)mac_biba_init_cred },
	{ MAC_INIT_DEVFSDIRENT,
	    (macop_t)mac_biba_init_devfsdirent },
	{ MAC_INIT_IFNET,
	    (macop_t)mac_biba_init_ifnet },
	{ MAC_INIT_IPQ,
	    (macop_t)mac_biba_init_ipq },
	{ MAC_INIT_MBUF,
	    (macop_t)mac_biba_init_mbuf },
	{ MAC_INIT_MOUNT,
	    (macop_t)mac_biba_init_mount },
	{ MAC_INIT_PIPE,
	    (macop_t)mac_biba_init_pipe },
	{ MAC_INIT_SOCKET,
	    (macop_t)mac_biba_init_socket },
	{ MAC_INIT_TEMP,
	    (macop_t)mac_biba_init_temp },
	{ MAC_INIT_VNODE,
	    (macop_t)mac_biba_init_vnode },
	{ MAC_DESTROY_BPFDESC,
	    (macop_t)mac_biba_destroy_bpfdesc },
	{ MAC_DESTROY_CRED,
	    (macop_t)mac_biba_destroy_cred },
	{ MAC_DESTROY_DEVFSDIRENT,
	    (macop_t)mac_biba_destroy_devfsdirent },
	{ MAC_DESTROY_IFNET,
	    (macop_t)mac_biba_destroy_ifnet },
	{ MAC_DESTROY_IPQ,
	    (macop_t)mac_biba_destroy_ipq },
	{ MAC_DESTROY_MBUF,
	    (macop_t)mac_biba_destroy_mbuf },
	{ MAC_DESTROY_MOUNT,
	    (macop_t)mac_biba_destroy_mount },
	{ MAC_DESTROY_PIPE,
	    (macop_t)mac_biba_destroy_pipe },
	{ MAC_DESTROY_SOCKET,
	    (macop_t)mac_biba_destroy_socket },
	{ MAC_DESTROY_TEMP,
	    (macop_t)mac_biba_destroy_temp },
	{ MAC_DESTROY_VNODE,
	    (macop_t)mac_biba_destroy_vnode },
	{ MAC_EXTERNALIZE,
	    (macop_t)mac_biba_externalize },
	{ MAC_INTERNALIZE,
	    (macop_t)mac_biba_internalize },
	{ MAC_CREATE_DEVFS_DEVICE,
	    (macop_t)mac_biba_create_devfs_device },
	{ MAC_CREATE_DEVFS_DIRECTORY,
	    (macop_t)mac_biba_create_devfs_directory },
	{ MAC_CREATE_DEVFS_VNODE,
	    (macop_t)mac_biba_create_devfs_vnode },
	{ MAC_CREATE_VNODE,
	    (macop_t)mac_biba_create_vnode },
	{ MAC_CREATE_MOUNT,
	    (macop_t)mac_biba_create_mount },
	{ MAC_CREATE_ROOT_MOUNT,
	    (macop_t)mac_biba_create_root_mount },
	{ MAC_RELABEL_VNODE,
	    (macop_t)mac_biba_relabel_vnode },
	{ MAC_UPDATE_DEVFSDIRENT,
	    (macop_t)mac_biba_update_devfsdirent },
	{ MAC_UPDATE_PROCFSVNODE,
	    (macop_t)mac_biba_update_procfsvnode },
	{ MAC_UPDATE_VNODE_FROM_EXTERNALIZED,
	    (macop_t)mac_biba_update_vnode_from_externalized },
	{ MAC_UPDATE_VNODE_FROM_MOUNT,
	    (macop_t)mac_biba_update_vnode_from_mount },
	{ MAC_CREATE_MBUF_FROM_SOCKET,
	    (macop_t)mac_biba_create_mbuf_from_socket },
	{ MAC_CREATE_PIPE,
	    (macop_t)mac_biba_create_pipe },
	{ MAC_CREATE_SOCKET,
	    (macop_t)mac_biba_create_socket },
	{ MAC_CREATE_SOCKET_FROM_SOCKET,
	    (macop_t)mac_biba_create_socket_from_socket },
	{ MAC_RELABEL_PIPE,
	    (macop_t)mac_biba_relabel_pipe },
	{ MAC_RELABEL_SOCKET,
	    (macop_t)mac_biba_relabel_socket },
	{ MAC_SET_SOCKET_PEER_FROM_MBUF,
	    (macop_t)mac_biba_set_socket_peer_from_mbuf },
	{ MAC_SET_SOCKET_PEER_FROM_SOCKET,
	    (macop_t)mac_biba_set_socket_peer_from_socket },
	{ MAC_CREATE_BPFDESC,
	    (macop_t)mac_biba_create_bpfdesc },
	{ MAC_CREATE_DATAGRAM_FROM_IPQ,
	    (macop_t)mac_biba_create_datagram_from_ipq },
	{ MAC_CREATE_FRAGMENT,
	    (macop_t)mac_biba_create_fragment },
	{ MAC_CREATE_IFNET,
	    (macop_t)mac_biba_create_ifnet },
	{ MAC_CREATE_IPQ,
	    (macop_t)mac_biba_create_ipq },
	{ MAC_CREATE_MBUF_FROM_MBUF,
	    (macop_t)mac_biba_create_mbuf_from_mbuf },
	{ MAC_CREATE_MBUF_LINKLAYER,
	    (macop_t)mac_biba_create_mbuf_linklayer },
	{ MAC_CREATE_MBUF_FROM_BPFDESC,
	    (macop_t)mac_biba_create_mbuf_from_bpfdesc },
	{ MAC_CREATE_MBUF_FROM_IFNET,
	    (macop_t)mac_biba_create_mbuf_from_ifnet },
	{ MAC_CREATE_MBUF_MULTICAST_ENCAP,
	    (macop_t)mac_biba_create_mbuf_multicast_encap },
	{ MAC_CREATE_MBUF_NETLAYER,
	    (macop_t)mac_biba_create_mbuf_netlayer },
	{ MAC_FRAGMENT_MATCH,
	    (macop_t)mac_biba_fragment_match },
	{ MAC_RELABEL_IFNET,
	    (macop_t)mac_biba_relabel_ifnet },
	{ MAC_UPDATE_IPQ,
	    (macop_t)mac_biba_update_ipq },
	{ MAC_CREATE_CRED,
	    (macop_t)mac_biba_create_cred },
	{ MAC_EXECVE_TRANSITION,
	    (macop_t)mac_biba_execve_transition },
	{ MAC_EXECVE_WILL_TRANSITION,
	    (macop_t)mac_biba_execve_will_transition },
	{ MAC_CREATE_PROC0,
	    (macop_t)mac_biba_create_proc0 },
	{ MAC_CREATE_PROC1,
	    (macop_t)mac_biba_create_proc1 },
	{ MAC_RELABEL_CRED,
	    (macop_t)mac_biba_relabel_cred },
	{ MAC_CHECK_BPFDESC_RECEIVE,
	    (macop_t)mac_biba_check_bpfdesc_receive },
	{ MAC_CHECK_CRED_RELABEL,
	    (macop_t)mac_biba_check_cred_relabel },
	{ MAC_CHECK_CRED_VISIBLE,
	    (macop_t)mac_biba_check_cred_visible },
	{ MAC_CHECK_IFNET_RELABEL,
	    (macop_t)mac_biba_check_ifnet_relabel },
	{ MAC_CHECK_IFNET_TRANSMIT,
	    (macop_t)mac_biba_check_ifnet_transmit },
	{ MAC_CHECK_MOUNT_STAT,
	    (macop_t)mac_biba_check_mount_stat },
	{ MAC_CHECK_PIPE_IOCTL,
	    (macop_t)mac_biba_check_pipe_ioctl },
	{ MAC_CHECK_PIPE_OP,
	    (macop_t)mac_biba_check_pipe_op },
	{ MAC_CHECK_PIPE_RELABEL,
	    (macop_t)mac_biba_check_pipe_relabel },
	{ MAC_CHECK_PROC_DEBUG,
	    (macop_t)mac_biba_check_proc_debug },
	{ MAC_CHECK_PROC_SCHED,
	    (macop_t)mac_biba_check_proc_sched },
	{ MAC_CHECK_PROC_SIGNAL,
	    (macop_t)mac_biba_check_proc_signal },
	{ MAC_CHECK_SOCKET_RECEIVE,
	    (macop_t)mac_biba_check_socket_receive },
	{ MAC_CHECK_SOCKET_RELABEL,
	    (macop_t)mac_biba_check_socket_relabel },
	{ MAC_CHECK_SOCKET_VISIBLE,
	    (macop_t)mac_biba_check_socket_visible },
	{ MAC_CHECK_VNODE_ACCESS,
	    (macop_t)mac_biba_check_vnode_access },
	{ MAC_CHECK_VNODE_CHDIR,
	    (macop_t)mac_biba_check_vnode_chdir },
	{ MAC_CHECK_VNODE_CHROOT,
	    (macop_t)mac_biba_check_vnode_chroot },
	{ MAC_CHECK_VNODE_CREATE,
	    (macop_t)mac_biba_check_vnode_create },
	{ MAC_CHECK_VNODE_DELETE,
	    (macop_t)mac_biba_check_vnode_delete },
	{ MAC_CHECK_VNODE_DELETEACL,
	    (macop_t)mac_biba_check_vnode_deleteacl },
	{ MAC_CHECK_VNODE_EXEC,
	    (macop_t)mac_biba_check_vnode_exec },
	{ MAC_CHECK_VNODE_GETACL,
	    (macop_t)mac_biba_check_vnode_getacl },
	{ MAC_CHECK_VNODE_GETEXTATTR,
	    (macop_t)mac_biba_check_vnode_getextattr },
	{ MAC_CHECK_VNODE_LOOKUP,
	    (macop_t)mac_biba_check_vnode_lookup },
	{ MAC_CHECK_VNODE_OPEN,
	    (macop_t)mac_biba_check_vnode_open },
	{ MAC_CHECK_VNODE_READDIR,
	    (macop_t)mac_biba_check_vnode_readdir },
	{ MAC_CHECK_VNODE_READLINK,
	    (macop_t)mac_biba_check_vnode_readlink },
	{ MAC_CHECK_VNODE_RELABEL,
	    (macop_t)mac_biba_check_vnode_relabel },
	{ MAC_CHECK_VNODE_RENAME_FROM,
	    (macop_t)mac_biba_check_vnode_rename_from },
	{ MAC_CHECK_VNODE_RENAME_TO,
	    (macop_t)mac_biba_check_vnode_rename_to },
	{ MAC_CHECK_VNODE_REVOKE,
	    (macop_t)mac_biba_check_vnode_revoke },
	{ MAC_CHECK_VNODE_SETACL,
	    (macop_t)mac_biba_check_vnode_setacl },
	{ MAC_CHECK_VNODE_SETEXTATTR,
	    (macop_t)mac_biba_check_vnode_setextattr },
	{ MAC_CHECK_VNODE_SETFLAGS,
	    (macop_t)mac_biba_check_vnode_setflags },
	{ MAC_CHECK_VNODE_SETMODE,
	    (macop_t)mac_biba_check_vnode_setmode },
	{ MAC_CHECK_VNODE_SETOWNER,
	    (macop_t)mac_biba_check_vnode_setowner },
	{ MAC_CHECK_VNODE_SETUTIMES,
	    (macop_t)mac_biba_check_vnode_setutimes },
	{ MAC_CHECK_VNODE_STAT,
	    (macop_t)mac_biba_check_vnode_stat },
	{ MAC_CHECK_VNODE_MMAP_PERMS,
	    (macop_t)mac_biba_check_vnode_mmap_perms },
	{ MAC_CHECK_VNODE_OP,
	    (macop_t)mac_biba_check_vnode_op },
	{ MAC_OP_LAST, NULL }
};

MAC_POLICY_SET(mac_biba_ops, trustedbsd_mac_biba, "TrustedBSD MAC/Biba",
    MPC_LOADTIME_FLAG_NOTLATE, &mac_biba_slot);
