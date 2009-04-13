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
static TAILQ_HEAD(vnet_modpending_head, vnet_modlink) vnet_modpending_head;
static void vnet_mod_complete_registration(struct vnet_modlink *);
static int vnet_mod_constructor(struct vnet_modlink *);

void
vnet_mod_register(const struct vnet_modinfo *vmi)
{

	vnet_mod_register_multi(vmi, NULL, NULL);
}

void
vnet_mod_register_multi(const struct vnet_modinfo *vmi, void *iarg,
    char *iname)
{
	struct vnet_modlink *vml, *vml_iter;
	
	/* Do not register the same {module, iarg} pair more than once. */
	TAILQ_FOREACH(vml_iter, &vnet_modlink_head, vml_mod_le)
		if (vml_iter->vml_modinfo == vmi && vml_iter->vml_iarg == iarg)
			break;
	if (vml_iter != NULL)
		panic("registering an already registered vnet module: %s",
		    vml_iter->vml_modinfo->vmi_name);
	vml = malloc(sizeof(struct vnet_modlink), M_VIMAGE, M_NOWAIT);

	/*
	 * XXX we support only statically assigned module IDs at the time.
	 * In principle modules should be able to get a dynamically
	 * assigned ID at registration time.
	 *
	 * If a module is registered in multiple instances, then each
	 * instance must have both iarg and iname set.
	 */
	if (vmi->vmi_id >= VNET_MOD_MAX)
		panic("invalid vnet module ID: %d", vmi->vmi_id);
	if (vmi->vmi_name == NULL)
		panic("vnet module with no name: %d", vmi->vmi_id);
	if ((iarg == NULL) ^ (iname == NULL))
		panic("invalid vnet module instance: %s", vmi->vmi_name);

	vml->vml_modinfo = vmi;
	vml->vml_iarg = iarg;
	vml->vml_iname = iname;

	/* Check whether the module we depend on is already registered. */
	if (vmi->vmi_dependson != vmi->vmi_id) {
		TAILQ_FOREACH(vml_iter, &vnet_modlink_head, vml_mod_le)
			if (vml_iter->vml_modinfo->vmi_id ==
			    vmi->vmi_dependson)
				break;	/* Depencency found, we are done. */
		if (vml_iter == NULL) {
#ifdef DEBUG_ORDERING
			printf("dependency %d missing for vnet mod %s,"
			    "postponing registration\n",
			    vmi->vmi_dependson, vmi->vmi_name);
#endif /* DEBUG_ORDERING */
			TAILQ_INSERT_TAIL(&vnet_modpending_head, vml,
			    vml_mod_le);
			return;
		}
	}

	vnet_mod_complete_registration(vml);
}
	
void
vnet_mod_complete_registration(struct vnet_modlink *vml)
{
	VNET_ITERATOR_DECL(vnet_iter);
	struct vnet_modlink *vml_iter;

	TAILQ_INSERT_TAIL(&vnet_modlink_head, vml, vml_mod_le);

	VNET_FOREACH(vnet_iter) {
		CURVNET_SET_QUIET(vnet_iter);
		vnet_mod_constructor(vml);
		CURVNET_RESTORE();
	}

	/* Check for pending modules depending on us. */
	do {
		TAILQ_FOREACH(vml_iter, &vnet_modpending_head, vml_mod_le)
			if (vml_iter->vml_modinfo->vmi_dependson ==
			    vml->vml_modinfo->vmi_id)
				break;
		if (vml_iter != NULL) {
#ifdef DEBUG_ORDERING
			printf("vnet mod %s now registering,"
			    "dependency %d loaded\n",
			    vml_iter->vml_modinfo->vmi_name,
			    vml->vml_modinfo->vmi_id);
#endif /* DEBUG_ORDERING */
			TAILQ_REMOVE(&vnet_modpending_head, vml_iter,
			    vml_mod_le);
			vnet_mod_complete_registration(vml_iter);
		}
	} while (vml_iter != NULL);
}

static int vnet_mod_constructor(struct vnet_modlink *vml)
{
	const struct vnet_modinfo *vmi = vml->vml_modinfo;

#ifdef DEBUG_ORDERING
	printf("instantiating vnet_%s", vmi->vmi_name);
	if (vml->vml_iarg)
		printf("/%s", vml->vml_iname);
	printf(": ");
	if (vmi->vmi_struct_size)
		printf("malloc(%zu); ", vmi->vmi_struct_size);
	if (vmi->vmi_iattach != NULL)
		printf("iattach()");
	printf("\n");
#endif

#ifdef VIMAGE
	if (vmi->vmi_struct_size) {
		void *mem = malloc(vmi->vmi_struct_size, M_VNET,
		    M_NOWAIT | M_ZERO);
		if (mem == NULL) /* XXX should return error, not panic. */
			panic("vi_alloc: malloc for %s\n", vmi->vmi_name);
		curvnet->mod_data[vmi->vmi_id] = mem;
	}
#endif

	if (vmi->vmi_iattach != NULL)
		vmi->vmi_iattach(vml->vml_iarg);

	return (0);
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
	TAILQ_INIT(&vnet_modpending_head);
}

static void
vi_init_done(void *unused)
{
	struct vnet_modlink *vml_iter;

	if (TAILQ_EMPTY(&vnet_modpending_head))
		return;

	printf("vnet modules with unresolved dependencies:\n");
	TAILQ_FOREACH(vml_iter, &vnet_modpending_head, vml_mod_le)
		printf("    %d:%s depending on %d\n",
		    vml_iter->vml_modinfo->vmi_id,
		    vml_iter->vml_modinfo->vmi_name,
		    vml_iter->vml_modinfo->vmi_dependson);
	panic("going nowhere without my vnet modules!");
}

SYSINIT(vimage, SI_SUB_VIMAGE, SI_ORDER_FIRST, vi_init, NULL);
SYSINIT(vimage_done, SI_SUB_VIMAGE_DONE, SI_ORDER_FIRST, vi_init_done, NULL);

#endif /* !VIMAGE_GLOBALS */
