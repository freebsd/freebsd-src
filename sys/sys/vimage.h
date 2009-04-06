/*-
 * Copyright (c) 2006-2008 University of Zagreb
 * Copyright (c) 2006-2008 FreeBSD Foundation
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

#include <sys/queue.h>

struct kld_sym_lookup;

struct vnet_symmap {
	char	*name;
	void	*base;
	size_t	size;
};

struct vnet_modinfo {
	char				*vmi_name;
	struct vnet_symmap		*vmi_symmap;
};

struct vnet_modlink {
	TAILQ_ENTRY(vnet_modlink)	vml_mod_le;
	const struct vnet_modinfo	*vml_modinfo;
};

#define VNET_MOD_DECLARE(m_name_uc, m_name_lc, m_iattach, m_idetach, 	\
    m_dependson, m_symmap)						\
	static const struct vnet_modinfo vnet_##m_name_lc##_modinfo = {	\
		.vmi_name		= #m_name_lc,			\
		.vmi_symmap		= m_symmap			\
};

#if defined(VIMAGE) && defined(VIMAGE_GLOBALS)
#error "You cannot have both option VIMAGE and option VIMAGE_GLOBALS!"
#endif

#ifdef VIMAGE_GLOBALS
#define	VSYM(base, sym) (sym)
#else
#ifdef VIMAGE
#error "No option VIMAGE yet!"
#else
#define	VSYM(base, sym) (base ## _0._ ## sym)
#endif
#endif

#define VNET_SYMMAP(mod, name)						\
	{ #name, &(vnet_ ## mod ## _0._ ## name),			\
	sizeof(vnet_ ## mod ## _0._ ## name) }

#define VNET_SYMMAP_END		{ NULL, 0 }

/* Non-VIMAGE null-macros */
#define	IS_DEFAULT_VNET(arg) 1
#define	CURVNET_SET(arg)
#define	CURVNET_SET_QUIET(arg)
#define	CURVNET_RESTORE()
#define	VNET_ASSERT(condition)
#define	INIT_FROM_VNET(vnet, modindex, modtype, sym)
#define	VNET_ITERATOR_DECL(arg)
#define	VNET_FOREACH(arg)
#define	VNET_LIST_RLOCK()
#define	VNET_LIST_RUNLOCK()
#define	INIT_VPROCG(arg)
#define	INIT_VCPU(arg)
#define	TD_TO_VIMAGE(td)
#define	TD_TO_VNET(td)
#define	TD_TO_VPROCG(td)
#define	TD_TO_VCPU(td)
#define	P_TO_VIMAGE(p)
#define	P_TO_VNET(p)
#define	P_TO_VPROCG(p)
#define	P_TO_VCPU(p)

/* XXX those defines bellow should probably go into vprocg.h and vcpu.h */
#define	VPROCG(sym)		(sym)
#define	VCPU(sym)		(sym)

#define	V_hostname		VPROCG(hostname)
#define	G_hostname		VPROCG(hostname) /* global hostname */
#define	V_domainname		VPROCG(domainname)

int	vi_symlookup(struct kld_sym_lookup *, char *);
void	vnet_mod_register(const struct vnet_modinfo *);

/*
 * Size-guards for the vimage structures.
 * If you need to update the values you MUST increment __FreeBSD_version.
 * See description further down to see how to get the new values.
 */
#ifdef __amd64__
#define	SIZEOF_vnet_net		464
#define	SIZEOF_vnet_net_LINT	5144
#define	SIZEOF_vnet_inet	4352
#define	SIZEOF_vnet_inet6	8800
#define	SIZEOF_vnet_ipsec	31160
#endif
#ifdef __arm__
#define	SIZEOF_vnet_net		236
#define	SIZEOF_vnet_net_LINT	1	/* No LINT kernel yet. */
#define	SIZEOF_vnet_inet	2580
#define	SIZEOF_vnet_inet6	8536
#define	SIZEOF_vnet_ipsec	1
#endif
#ifdef __i386__ /* incl. pc98 */
#define	SIZEOF_vnet_net		236
#define	SIZEOF_vnet_net_LINT	2576
#define	SIZEOF_vnet_inet	2576
#define	SIZEOF_vnet_inet6	8528
#define	SIZEOF_vnet_ipsec	31016
#endif
#ifdef __ia64__
#define	SIZEOF_vnet_net		464
#define	SIZEOF_vnet_net_LINT	5144
#define	SIZEOF_vnet_inet	4352
#define	SIZEOF_vnet_inet6	8800
#define	SIZEOF_vnet_ipsec	31160
#endif
#ifdef __mips__
#define	SIZEOF_vnet_net		236
#define	SIZEOF_vnet_net_LINT	1	/* No LINT kernel yet. */
#define	SIZEOF_vnet_inet	2624
#define	SIZEOF_vnet_inet6	8552
#define	SIZEOF_vnet_ipsec	1
#endif
#ifdef __powerpc__
#define	SIZEOF_vnet_net		236
#define	SIZEOF_vnet_net_LINT	2576
#define	SIZEOF_vnet_inet	2616
#define	SIZEOF_vnet_inet6	8536
#define	SIZEOF_vnet_ipsec	31048
#endif
#ifdef __sparc64__ /* incl. sun4v */
#define	SIZEOF_vnet_net		464
#define	SIZEOF_vnet_net_LINT	5144
#define	SIZEOF_vnet_inet	4352
#define	SIZEOF_vnet_inet6	8800
#define	SIZEOF_vnet_ipsec	31160
#endif

#ifdef COMPILING_LINT
#undef	SIZEOF_vnet_net
#define	SIZEOF_vnet_net	SIZEOF_vnet_net_LINT
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

#endif /* !_SYS_VIMAGE_H_ */
