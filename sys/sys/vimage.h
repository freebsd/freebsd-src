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

#endif /* !_SYS_VIMAGE_H_ */
