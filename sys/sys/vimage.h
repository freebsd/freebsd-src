/*-
 * Copyright (c) 2006-2009 University of Zagreb
 * Copyright (c) 2006-2009 FreeBSD Foundation
 *
 * This software was developed by the University of Zagreb and the
 * FreeBSD Foundation under sponsorship by the Stichting NLnet and the
 * FreeBSD Foundation.
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

#ifndef	_SYS_VIMAGE_H_
#define	_SYS_VIMAGE_H_

#include <sys/proc.h>
#include <sys/queue.h>

/* Interim userspace API. */
struct vi_req {
	int	vi_api_cookie;		/* Catch API mismatch. */
	int	vi_req_action;		/* What to do with this request? */
	u_short	vi_proc_count;		/* Current number of processes. */
	int	vi_if_count;		/* Current number of ifnets. */
	int	vi_sock_count;
	char	vi_name[MAXPATHLEN];
	char	vi_if_xname[MAXPATHLEN]; /* XXX should be IFNAMSIZ */
};

#define	VI_CREATE		0x00000001
#define	VI_DESTROY		0x00000002
#define	VI_SWITCHTO		0x00000008
#define	VI_IFACE		0x00000010
#define	VI_GET			0x00000100
#define	VI_GETNEXT		0x00000200
#define	VI_GETNEXT_RECURSE	0x00000300

#define	VI_API_VERSION		1		/* Bump on struct changes. */

#define	VI_API_COOKIE		((sizeof(struct vi_req) << 16) | VI_API_VERSION)

#ifdef _KERNEL

#ifdef INVARIANTS
#define	VNET_DEBUG
#endif

struct vimage;
struct vprocg;
struct vnet;
struct vi_req;
struct ifnet;
struct kld_sym_lookup;
struct thread;

typedef int vnet_attach_fn(const void *);
typedef int vnet_detach_fn(const void *);

#ifdef VIMAGE

struct vnet_modinfo {
	u_int				 vmi_id;
	u_int				 vmi_dependson;
	char				*vmi_name;
	vnet_attach_fn			*vmi_iattach;
	vnet_detach_fn			*vmi_idetach;
};
typedef struct vnet_modinfo vnet_modinfo_t;

struct vnet_modlink {
	TAILQ_ENTRY(vnet_modlink)	 vml_mod_le;
	const struct vnet_modinfo	*vml_modinfo;
	const void			*vml_iarg;
	const char			*vml_iname;
};

/* Stateful modules. */
#define	VNET_MOD_NET		 0	/* MUST be 0 - implicit dependency */
#define	VNET_MOD_NETGRAPH	 1
#define	VNET_MOD_INET		 2
#define	VNET_MOD_INET6		 3
#define	VNET_MOD_IPSEC		 4
#define	VNET_MOD_IPFW		 5
#define	VNET_MOD_DUMMYNET	 6
#define	VNET_MOD_PF		 7
#define	VNET_MOD_ALTQ		 8
#define	VNET_MOD_IPX		 9
#define	VNET_MOD_ATALK		10
#define	VNET_MOD_ACCF_HTTP	11
#define	VNET_MOD_IGMP		12
#define	VNET_MOD_MLD		13
#define	VNET_MOD_RTABLE		14

/* Stateless modules. */
#define	VNET_MOD_IF_CLONE	19
#define	VNET_MOD_NG_ETHER	20
#define	VNET_MOD_NG_IFACE	21
#define	VNET_MOD_NG_EIFACE	22
#define	VNET_MOD_ESP		23
#define	VNET_MOD_IPIP		24
#define	VNET_MOD_AH		25
#define	VNET_MOD_IPCOMP	 	26	
#define	VNET_MOD_GIF		27
	/*	 		28 */
#define	VNET_MOD_FLOWTABLE	29
#define	VNET_MOD_LOIF		30
#define	VNET_MOD_DOMAIN		31
#define	VNET_MOD_DYNAMIC_START	32
#define	VNET_MOD_MAX		64

/* Major module IDs for vimage sysctl virtualization. */
#define	V_GLOBAL		0	/* global variable - no indirection */
#define	V_NET			1
#define	V_PROCG			2

int	vi_td_ioctl(u_long, struct vi_req *, struct thread *);
int	vi_if_move(struct thread *, struct ifnet *, char *, int,
	    struct vi_req *);
int	vi_child_of(struct vimage *, struct vimage *);
struct vimage *vimage_by_name(struct vimage *, char *);
void	vnet_mod_register(const struct vnet_modinfo *);
void	vnet_mod_register_multi(const struct vnet_modinfo *, void *, char *);
void	vnet_mod_deregister(const struct vnet_modinfo *);
void	vnet_mod_deregister_multi(const struct vnet_modinfo *, void *, char *);
struct vnet *vnet_alloc(void);
void	vnet_destroy(struct vnet *);
void	vnet_foreach(void (*vnet_foreach_fn)(struct vnet *, void *),
	    void *arg);

#endif /* VIMAGE */

struct vimage {
	LIST_ENTRY(vimage)	 vi_le;		/* all vimage list */
	LIST_ENTRY(vimage)	 vi_sibling;	/* vimages with same parent */
	LIST_HEAD(, vimage)	 vi_child_head;	/* direct offspring list */
	struct vimage		*vi_parent;	/* ptr to parent vimage */
	u_int			 vi_id;		/* ID num */
	volatile u_int		 vi_ucredrefc;	/* # of ucreds pointing to us */
	char			 vi_name[MAXHOSTNAMELEN];
	struct vnet		*v_net;
	struct vprocg		*v_procg;
};

struct vnet {
	LIST_ENTRY(vnet)	 vnet_le;	/* all vnets list */
	u_int			 vnet_magic_n;
	u_int			 ifcnt;
	u_int			 sockcnt;
	void			*vnet_data_mem;
	uintptr_t		 vnet_data_base;
};

struct vprocg {
	LIST_ENTRY(vprocg)	 vprocg_le;
	u_int			 vprocg_id;	/* ID num */
	u_int			 nprocs;
};

#ifdef VIMAGE
LIST_HEAD(vimage_list_head, vimage);
extern struct vimage_list_head vimage_head;
#else /* !VIMAGE */
extern struct vprocg vprocg_0;
#endif /* VIMAGE */
 
#define	curvnet curthread->td_vnet

#define	VNET_MAGIC_N 0x3e0d8f29

#ifdef VIMAGE
#ifdef VNET_DEBUG
#define	VNET_ASSERT(condition)						\
	if (!(condition)) {						\
		printf("VNET_ASSERT @ %s:%d %s():\n",			\
			__FILE__, __LINE__, __FUNCTION__);		\
		panic(#condition);					\
	}

#define	CURVNET_SET_QUIET(arg)						\
	VNET_ASSERT((arg)->vnet_magic_n == VNET_MAGIC_N);		\
	struct vnet *saved_vnet = curvnet;				\
	const char *saved_vnet_lpush = curthread->td_vnet_lpush;	\
	curvnet = arg;							\
	curthread->td_vnet_lpush = __FUNCTION__;
 
#define	CURVNET_SET_VERBOSE(arg)					\
	CURVNET_SET_QUIET(arg)						\
	if (saved_vnet)							\
		printf("CURVNET_SET(%p) in %s() on cpu %d, prev %p in %s()\n", \
		       curvnet,	curthread->td_vnet_lpush, curcpu,	\
		       saved_vnet, saved_vnet_lpush);

#define	CURVNET_SET(arg)	CURVNET_SET_VERBOSE(arg)
 
#define	CURVNET_RESTORE()						\
	VNET_ASSERT(saved_vnet == NULL ||				\
		    saved_vnet->vnet_magic_n == VNET_MAGIC_N);		\
	curvnet = saved_vnet;						\
	curthread->td_vnet_lpush = saved_vnet_lpush;
#else /* !VNET_DEBUG */
#define	VNET_ASSERT(condition)

#define	CURVNET_SET(arg)						\
	struct vnet *saved_vnet = curvnet;				\
	curvnet = arg;	
 
#define	CURVNET_SET_VERBOSE(arg)	CURVNET_SET(arg)
#define	CURVNET_SET_QUIET(arg)		CURVNET_SET(arg)
 
#define	CURVNET_RESTORE()						\
	curvnet = saved_vnet;
#endif /* VNET_DEBUG */
#else /* !VIMAGE */
#define	VNET_ASSERT(condition)
#define	CURVNET_SET(arg)
#define	CURVNET_SET_QUIET(arg)
#define	CURVNET_RESTORE()
#endif /* !VIMAGE */

#ifdef VIMAGE
LIST_HEAD(vnet_list_head, vnet);
extern struct vnet_list_head vnet_head;
extern struct vnet *vnet0;
#define	VNET_ITERATOR_DECL(arg) struct vnet *arg;
#define	VNET_FOREACH(arg) LIST_FOREACH(arg, &vnet_head, vnet_le)
#else
#define	VNET_ITERATOR_DECL(arg)
#define	VNET_FOREACH(arg)
#endif

#ifdef VIMAGE
LIST_HEAD(vprocg_list_head, vprocg);
extern struct vprocg_list_head vprocg_head;
#define	INIT_VPROCG(arg)	struct vprocg *vprocg = (arg);
#else
#define	INIT_VPROCG(arg)
#endif

#ifdef VIMAGE
#define	IS_DEFAULT_VIMAGE(arg)	((arg)->vi_id == 0)
#define	IS_DEFAULT_VNET(arg)	((arg) == vnet0)
#else
#define	IS_DEFAULT_VIMAGE(arg)	1
#define	IS_DEFAULT_VNET(arg)	1
#endif

#ifdef VIMAGE
#define	CRED_TO_VNET(cr)						\
	(IS_DEFAULT_VIMAGE((cr)->cr_vimage) ? (cr)->cr_prison->pr_vnet	\
	    : (cr)->cr_vimage->v_net)
#define	TD_TO_VIMAGE(td)	(td)->td_ucred->cr_vimage
#define	TD_TO_VNET(td)		CRED_TO_VNET((td)->td_ucred)
#define	TD_TO_VPROCG(td)	(td)->td_ucred->cr_vimage->v_procg
#define	P_TO_VIMAGE(p)		(p)->p_ucred->cr_vimage
#define	P_TO_VNET(p)		CRED_TO_VNET((p)->p_ucred)
#define	P_TO_VPROCG(p)		(p)->p_ucred->cr_vimage->v_procg
#else /* !VIMAGE */
#define	CRED_TO_VNET(cr)	NULL
#define	TD_TO_VIMAGE(td)	NULL
#define	TD_TO_VNET(td)		NULL
#define	P_TO_VIMAGE(p)		NULL
#define	P_TO_VNET(p)		NULL
#define	TD_TO_VPROCG(td)	&vprocg_0
#define	P_TO_VPROCG(p)		&vprocg_0
#endif /* VIMAGE */

/* Non-VIMAGE null-macros */
#define	VNET_LIST_RLOCK()
#define	VNET_LIST_RUNLOCK()

#endif /* _KERNEL */

#endif /* !_SYS_VIMAGE_H_ */
