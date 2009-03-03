/*-
 * Copyright (c) 2004-2008 University of Zagreb
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/kernel.h>
#include <sys/linker.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/vimage.h>

#ifndef VIMAGE_GLOBALS

MALLOC_DEFINE(M_VIMAGE, "vimage", "vimage resource container");

static TAILQ_HEAD(vnet_modlink_head, vnet_modlink) vnet_modlink_head;

void
vnet_mod_register(const struct vnet_modinfo *vmi)
{
	struct vnet_modlink *vml, *vml_iter;
	
	/* Do not register the same module instance more than once. */
	TAILQ_FOREACH(vml_iter, &vnet_modlink_head, vml_mod_le)
		if (vml_iter->vml_modinfo == vmi)
			panic("%s: %s", __func__, vmi->vmi_name);
	vml = malloc(sizeof(struct vnet_modlink), M_VIMAGE, M_NOWAIT);
	vml->vml_modinfo = vmi;
	TAILQ_INSERT_TAIL(&vnet_modlink_head, vml, vml_mod_le);
}

/*
 * vi_symlookup() attempts to resolve name to address queries for
 * variables which have been moved from global namespace to virtualization
 * container structures, but are still directly accessed from legacy
 * userspace processes via kldsym(2) and kmem(4) interfaces.
 */
int
vi_symlookup(struct kld_sym_lookup *lookup, char *symstr)
{
	struct vnet_modlink *vml;
	struct vnet_symmap *mapentry;

	TAILQ_FOREACH(vml, &vnet_modlink_head, vml_mod_le) {
		if (vml->vml_modinfo->vmi_symmap == NULL)
			continue;
		for (mapentry = vml->vml_modinfo->vmi_symmap;
		     mapentry->name != NULL; mapentry++) {
			if (strcmp(symstr, mapentry->name) == 0) {
				lookup->symvalue = (u_long) mapentry->base;
				lookup->symsize = mapentry->size;
				return (0);
			}
		}
	}
	return (ENOENT);
}

static void
vi_init(void *unused)
{

	TAILQ_INIT(&vnet_modlink_head);
}

SYSINIT(vimage, SI_SUB_VIMAGE, SI_ORDER_FIRST, vi_init, NULL);

#endif /* !VIMAGE_GLOBALS */
