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

#if defined(VIMAGE) && defined(VIMAGE_GLOBALS)
#error "You cannot have both option VIMAGE and option VIMAGE_GLOBALS!"
#endif

#ifdef INVARIANTS
#define	VNET_DEBUG
#endif

struct vprocg;
struct vnet;
struct vi_req;
struct ifnet;
struct kld_sym_lookup;

typedef int vnet_attach_fn(const void *);
typedef int vnet_detach_fn(const void *);

#ifndef VIMAGE_GLOBALS

struct vnet_symmap {
	char	*name;
	size_t	 offset;
	size_t	 size;
};
typedef struct vnet_symmap vnet_symmap_t;

struct vnet_modinfo {
	u_int				 vmi_id;
	u_int				 vmi_dependson;
	char				*vmi_name;
	vnet_attach_fn			*vmi_iattach;
	vnet_detach_fn			*vmi_idetach;
	size_t				 vmi_size;
	struct vnet_symmap		*vmi_symmap;
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
#define	VNET_MOD_ARP		28
#define	VNET_MOD_RTABLE		29
#define	VNET_MOD_LOIF		30
#define	VNET_MOD_DOMAIN		31
#define	VNET_MOD_DYNAMIC_START	32
#define	VNET_MOD_MAX		64

/* Major module IDs for vimage sysctl virtualization. */
#define	V_GLOBAL		0	/* global variable - no indirection */
#define	V_NET			1
#define	V_PROCG			2

/* Name mappings for minor module IDs in vimage sysctl virtualization. */
#define	V_MOD_vnet_net		VNET_MOD_NET
#define	V_MOD_vnet_netgraph	VNET_MOD_NETGRAPH
#define	V_MOD_vnet_inet		VNET_MOD_INET
#define	V_MOD_vnet_inet6	VNET_MOD_INET6
#define	V_MOD_vnet_ipfw		VNET_MOD_IPFW
#define	V_MOD_vnet_pf		VNET_MOD_PF
#define	V_MOD_vnet_gif		VNET_MOD_GIF
#define	V_MOD_vnet_ipsec	VNET_MOD_IPSEC
 
#define	V_MOD_vprocg		0	/* no minor module ids like in vnet */

int	vi_symlookup(struct kld_sym_lookup *, char *);
int	vi_td_ioctl(u_long, struct vi_req *, struct thread *);
int	vi_if_move(struct vi_req *, struct ifnet *, struct vimage *);
int	vi_child_of(struct vimage *, struct vimage *);
void	vnet_mod_register(const struct vnet_modinfo *);
void	vnet_mod_register_multi(const struct vnet_modinfo *, void *, char *);
void	vnet_mod_deregister(const struct vnet_modinfo *);
void	vnet_mod_deregister_multi(const struct vnet_modinfo *, void *, char *);

#endif /* !VIMAGE_GLOBALS */

#ifdef VIMAGE_GLOBALS
#define	VSYM(base, sym) (sym)
#else /* !VIMAGE_GLOBALS */
#ifdef VIMAGE
#define	VSYM(base, sym) ((base)->_ ## sym)
#else /* !VIMAGE */
#define	VSYM(base, sym) (base ## _0._ ## sym)
#endif /* VIMAGE */
#endif /* VIMAGE_GLOBALS */

#ifndef VIMAGE_GLOBALS
#ifdef VIMAGE
/*
 * Casted NULL hack is needed for harvesting sizeofs() of fields inside
 * struct vnet_* containers at compile time.
 */
#define	VNET_SYMMAP(mod, name)						\
	{ #name, offsetof(struct vnet_ ## mod, _ ## name),		\
	sizeof(((struct vnet_ ## mod *) NULL)->_ ## name) }
#else /* !VIMAGE */
#define	VNET_SYMMAP(mod, name)						\
	{ #name, (size_t) &(vnet_ ## mod ## _0._ ## name),		\
	sizeof(vnet_ ## mod ## _0._ ## name) }
#endif /* VIMAGE */
#define	VNET_SYMMAP_END		{ NULL, 0 }

struct vimage {
	LIST_ENTRY(vimage)	 vi_le;		/* all vimage list */
	LIST_ENTRY(vimage)	 vi_sibling;	/* vimages with same parent */
	LIST_HEAD(, vimage)	 vi_child_head;	/* direct offspring list */
	struct vimage		*vi_parent;	/* ptr to parent vimage */
	u_int			 vi_id;		/* ID num */
	u_int			 vi_ucredrefc;	/* # of ucreds pointing to us */
	char			 vi_name[MAXHOSTNAMELEN];
	struct vnet		*v_net;
	struct vprocg		*v_procg;
};

struct vnet {
	void			*mod_data[VNET_MOD_MAX];
	LIST_ENTRY(vnet)	 vnet_le;	/* all vnets list */
	u_int			 vnet_magic_n;
	u_int			 vnet_id;	/* ID num */
	u_int			 ifcnt;
	u_int			 sockcnt;
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
#endif /* !VIMAGE_GLOBALS */
 
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
#ifdef VNET_DEBUG
#define	INIT_FROM_VNET(vnet, modindex, modtype, sym)			\
	if (vnet == NULL || vnet != curvnet)				\
		panic("in %s:%d %s()\n vnet=%p curvnet=%p",		\
		    __FILE__, __LINE__, __FUNCTION__,			\
		    vnet, curvnet);					\
	modtype *sym = (vnet)->mod_data[modindex];
#else /* !VNET_DEBUG */
#define	INIT_FROM_VNET(vnet, modindex, modtype, sym)			\
	modtype *sym = (vnet)->mod_data[modindex];
#endif /* !VNET_DEBUG */
#else /* !VIMAGE */
#define	INIT_FROM_VNET(vnet, modindex, modtype, sym)
#endif /* VIMAGE */

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
#define	IS_DEFAULT_VNET(arg)	((arg)->vnet_id == 0)
#else
#define	IS_DEFAULT_VIMAGE(arg)	1
#define	IS_DEFAULT_VNET(arg)	1
#endif

#ifdef VIMAGE
#define	TD_TO_VIMAGE(td)	(td)->td_ucred->cr_vimage
#define	TD_TO_VNET(td)		(td)->td_ucred->cr_vimage->v_net
#define	TD_TO_VPROCG(td)	(td)->td_ucred->cr_vimage->v_procg
#define	P_TO_VIMAGE(p)		(p)->p_ucred->cr_vimage
#define	P_TO_VNET(p)		(p)->p_ucred->cr_vimage->v_net
#define	P_TO_VPROCG(p)		(p)->p_ucred->cr_vimage->v_procg
#else /* !VIMAGE */
#define	TD_TO_VIMAGE(td)	NULL
#define	TD_TO_VNET(td)		NULL
#define	P_TO_VIMAGE(p)		NULL
#define	P_TO_VNET(p)		NULL
#ifdef VIMAGE_GLOBALS
#define	TD_TO_VPROCG(td)	NULL
#define	P_TO_VPROCG(p)		NULL
#else /* !VIMAGE_GLOBALS */
#define	TD_TO_VPROCG(td)	&vprocg_0
#define	P_TO_VPROCG(p)		&vprocg_0
#endif /* VIMAGE_GLOBALS */
#endif /* VIMAGE */

/* Non-VIMAGE null-macros */
#define	VNET_LIST_RLOCK()
#define	VNET_LIST_RUNLOCK()

/* XXX those defines bellow should probably go into vprocg.h and vcpu.h */
#define	VPROCG(sym)		VSYM(vprocg, sym)

/*
 * Size-guards for the vimage structures.
 * If you need to update the values you MUST increment __FreeBSD_version.
 * See description further down to see how to get the new values.
 */
#ifdef __amd64__
#define	SIZEOF_vnet_net		176
#define	SIZEOF_vnet_inet	4424
#define	SIZEOF_vnet_inet6	8808
#define	SIZEOF_vnet_ipsec	31160
#endif
#ifdef __arm__
#define	SIZEOF_vnet_net		96
#define	SIZEOF_vnet_inet	2616
#define	SIZEOF_vnet_inet6	8524
#define	SIZEOF_vnet_ipsec	1
#endif
#ifdef __i386__ /* incl. pc98 */
#define	SIZEOF_vnet_net		96
#define	SIZEOF_vnet_inet	2612
#define	SIZEOF_vnet_inet6	8512
#define	SIZEOF_vnet_ipsec	31024
#endif
#ifdef __ia64__
#define	SIZEOF_vnet_net		176
#define	SIZEOF_vnet_inet	4424
#define	SIZEOF_vnet_inet6	8808
#define	SIZEOF_vnet_ipsec	31160
#endif
#ifdef __mips__
#define	SIZEOF_vnet_net		96
#define	SIZEOF_vnet_inet	2648
#define	SIZEOF_vnet_inet6	8544
#define	SIZEOF_vnet_ipsec	1
#endif
#ifdef __powerpc__
#define	SIZEOF_vnet_net		96
#define	SIZEOF_vnet_inet	2640
#define	SIZEOF_vnet_inet6	8520
#define	SIZEOF_vnet_ipsec	31048
#endif
#ifdef __sparc64__ /* incl. sun4v */
#define	SIZEOF_vnet_net		176
#define	SIZEOF_vnet_inet	4424
#define	SIZEOF_vnet_inet6	8808
#define	SIZEOF_vnet_ipsec	31160
#endif

#ifndef	SIZEOF_vnet_net
#error "SIZEOF_vnet_net no defined for this architecture."
#endif
#ifndef	SIZEOF_vnet_inet
#error "SIZEOF_vnet_inet no defined for this architecture."
#endif
#ifndef	SIZEOF_vnet_inet6
#error "SIZEOF_vnet_inet6 no defined for this architecture."
#endif
#ifndef	SIZEOF_vnet_ipsec
#error "SIZEOF_vnet_ipsec no defined for this architecture."
#endif

/*
 * x must be a positive integer constant (expected value),
 * y must be compile-time evaluated to a positive integer,
 * e.g. CTASSERT_EQUAL(FOO_EXPECTED_SIZE, sizeof (struct foo));
 * One needs to compile with -Wuninitialized and thus at least -O
 * for this to trigger and -Werror if it should be fatal.
 */
#define	CTASSERT_EQUAL(x, y)						\
	static int __attribute__((__used__))				\
	    __attribute__((__section__(".debug_ctassert_equal")))	\
	__CONCAT(__ctassert_equal_at_line_, __LINE__)(void);		\
									\
	static int __attribute__((__used__))				\
	    __attribute__((__section__(".debug_ctassert_equal")))	\
	__CONCAT(__ctassert_equal_at_line_, __LINE__)(void)		\
	{								\
		int __CONCAT(__CONCAT(__expected_, x),			\
		    _but_got)[(y) + (x)];				\
		__CONCAT(__CONCAT(__expected_, x), _but_got)[(x)] = 1;	\
		return (__CONCAT(__CONCAT(__expected_, x),		\
		    _but_got)[(y)]);					\
	}								\
	struct __hack

/*
 * x shall be the expected value (SIZEOF_vnet_* from above)
 * and y shall be the real size (sizeof(struct vnet_*)).
 * If you run into the CTASSERT() you want to compile a universe
 * with COPTFLAGS+="-O -Wuninitialized -DVIMAGE_CHECK_SIZES".
 * This should give you the errors for the proper values defined above.
 * Make sure to re-run universe with the proper values afterwards -
 * -DMAKE_JUST_KERNELS should be enough.
 * 
 * Note: 
 * CTASSERT() takes precedence in the current FreeBSD world thus the
 * CTASSERT_EQUAL() will not neccessarily trigger if one uses both.
 * But as CTASSERT_EQUAL() needs special compile time options, we
 * want the default case to be backed by CTASSERT().
 */
#if 0
#ifndef VIMAGE_CTASSERT
#ifdef VIMAGE_CHECK_SIZES
#define	VIMAGE_CTASSERT(x, y)						\
	CTASSERT_EQUAL(x, y)
#else
#define	VIMAGE_CTASSERT(x, y)						\
	CTASSERT_EQUAL(x, y);						\
	CTASSERT(x == 0 || x == y)
#endif
#endif
#else
#define	VIMAGE_CTASSERT(x, y)		struct __hack
#endif

#endif /* _KERNEL */

#endif /* !_SYS_VIMAGE_H_ */
