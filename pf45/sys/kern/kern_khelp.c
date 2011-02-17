/*-
 * Copyright (c) 2010 Lawrence Stewart <lstewart@freebsd.org>
 * Copyright (c) 2010 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Lawrence Stewart while studying at the Centre
 * for Advanced Internet Architectures, Swinburne University, made possible in
 * part by grants from the FreeBSD Foundation and Cisco University Research
 * Program Fund at Community Foundation Silicon Valley.
 *
 * Portions of this software were developed at the Centre for Advanced
 * Internet Architectures, Swinburne University of Technology, Melbourne,
 * Australia by Lawrence Stewart under sponsorship from the FreeBSD Foundation.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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
#include <sys/kernel.h>
#include <sys/hhook.h>
#include <sys/jail.h>
#include <sys/khelp.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/module_khelp.h>
#include <sys/osd.h>
#include <sys/queue.h>
#include <sys/refcount.h>
#include <sys/rwlock.h>
#include <sys/systm.h>

#include <net/vnet.h>

static struct rwlock khelp_list_lock;
RW_SYSINIT(khelplistlock, &khelp_list_lock, "helper list lock");

static TAILQ_HEAD(helper_head, helper) helpers = TAILQ_HEAD_INITIALIZER(helpers);

/* Private function prototypes. */
static inline void khelp_remove_osd(struct helper *h, struct osd *hosd);

#define	KHELP_LIST_WLOCK() rw_wlock(&khelp_list_lock)
#define	KHELP_LIST_WUNLOCK() rw_wunlock(&khelp_list_lock)
#define	KHELP_LIST_RLOCK() rw_rlock(&khelp_list_lock)
#define	KHELP_LIST_RUNLOCK() rw_runlock(&khelp_list_lock)
#define	KHELP_LIST_LOCK_ASSERT() rw_assert(&khelp_list_lock, RA_LOCKED)

int
khelp_register_helper(struct helper *h)
{
	struct helper *tmph;
	int error, i, inserted;

	error = 0;
	inserted = 0;
	refcount_init(&h->h_refcount, 0);
	h->h_id = osd_register(OSD_KHELP, NULL, NULL);

	/* It's only safe to add the hooks after osd_register(). */
	if (h->h_nhooks > 0) {
		for (i = 0; i < h->h_nhooks && !error; i++) {
			/* We don't require the module to assign hook_helper. */
			h->h_hooks[i].hook_helper = h;
			error = khelp_add_hhook(&h->h_hooks[i], HHOOK_NOWAIT);
		}

		if (error) {
			for (i--; i >= 0; i--)
				khelp_remove_hhook(&h->h_hooks[i]);

			osd_deregister(OSD_KHELP, h->h_id);
		}
	}

	if (!error) {
		KHELP_LIST_WLOCK();
		/*
		 * Keep list of helpers sorted in descending h_id order. Due to
		 * the way osd_set() works, a sorted list ensures
		 * init_helper_osd() will operate with improved efficiency.
		 */
		TAILQ_FOREACH(tmph, &helpers, h_next) {
			if (tmph->h_id < h->h_id) {
				TAILQ_INSERT_BEFORE(tmph, h, h_next);
				inserted = 1;
				break;
			}
		}

		if (!inserted)
			TAILQ_INSERT_TAIL(&helpers, h, h_next);
		KHELP_LIST_WUNLOCK();
	}

	return (error);
}

int
khelp_deregister_helper(struct helper *h)
{
	struct helper *tmph;
	int error, i;

	error = 0;

	KHELP_LIST_WLOCK();
	if (h->h_refcount > 0)
		error = EBUSY;
	else {
		error = ENOENT;
		TAILQ_FOREACH(tmph, &helpers, h_next) {
			if (tmph == h) {
				TAILQ_REMOVE(&helpers, h, h_next);
				error = 0;
				break;
			}
		}
	}
	KHELP_LIST_WUNLOCK();

	if (!error) {
		if (h->h_nhooks > 0) {
			for (i = 0; i < h->h_nhooks; i++)
				khelp_remove_hhook(&h->h_hooks[i]);
		}
		osd_deregister(OSD_KHELP, h->h_id);
	}

	return (error);
}

int
khelp_init_osd(uint32_t classes, struct osd *hosd)
{
	struct helper *h;
	void *hdata;
	int error;

	KASSERT(hosd != NULL, ("struct osd not initialised!"));

	error = 0;

	KHELP_LIST_RLOCK();
	TAILQ_FOREACH(h, &helpers, h_next) {
		/* If helper is correct class and needs to store OSD... */
		if (h->h_classes & classes && h->h_flags & HELPER_NEEDS_OSD) {
			hdata = uma_zalloc(h->h_zone, M_NOWAIT);
			if (hdata == NULL) {
				error = ENOMEM;
				break;
			}
			osd_set(OSD_KHELP, hosd, h->h_id, hdata);
			refcount_acquire(&h->h_refcount);
		}
	}

	if (error) {
		/* Delete OSD that was assigned prior to the error. */
		TAILQ_FOREACH(h, &helpers, h_next) {
			if (h->h_classes & classes)
				khelp_remove_osd(h, hosd);
		}
	}
	KHELP_LIST_RUNLOCK();

	return (error);
}

int
khelp_destroy_osd(struct osd *hosd)
{
	struct helper *h;
	int error;

	KASSERT(hosd != NULL, ("struct osd not initialised!"));

	error = 0;

	KHELP_LIST_RLOCK();
	/*
	 * Clean up all khelp related OSD.
	 *
	 * XXXLAS: Would be nice to use something like osd_exit() here but it
	 * doesn't have the right semantics for this purpose.
	 */
	TAILQ_FOREACH(h, &helpers, h_next)
		khelp_remove_osd(h, hosd);
	KHELP_LIST_RUNLOCK();

	return (error);
}

static inline void
khelp_remove_osd(struct helper *h, struct osd *hosd)
{
	void *hdata;

	if (h->h_flags & HELPER_NEEDS_OSD) {
		/*
		 * If the current helper uses OSD and calling osd_get()
		 * on the helper's h_id returns non-NULL, the helper has
		 * OSD attached to 'hosd' which needs to be cleaned up.
		 */
		hdata = osd_get(OSD_KHELP, hosd, h->h_id);
		if (hdata != NULL) {
			uma_zfree(h->h_zone, hdata);
			osd_del(OSD_KHELP, hosd, h->h_id);
			refcount_release(&h->h_refcount);
		}
	}
}

void *
khelp_get_osd(struct osd *hosd, int32_t id)
{

	return (osd_get(OSD_KHELP, hosd, id));
}

int32_t
khelp_get_id(char *hname)
{
	struct helper *h;
	int32_t id;

	id = -1;

	KHELP_LIST_RLOCK();
	TAILQ_FOREACH(h, &helpers, h_next) {
		if (strncmp(h->h_name, hname, HELPER_NAME_MAXLEN) == 0) {
			id = h->h_id;
			break;
		}
	}
	KHELP_LIST_RUNLOCK();

	return (id);
}

int
khelp_add_hhook(struct hookinfo *hki, uint32_t flags)
{
	VNET_ITERATOR_DECL(vnet_iter);
	int error;

	error = 0;

	/*
	 * XXXLAS: If a helper is dynamically adding a helper hook function at
	 * runtime using this function, we should update the helper's h_hooks
	 * struct member to include the additional hookinfo struct.
	 */

	VNET_LIST_RLOCK_NOSLEEP();
	VNET_FOREACH(vnet_iter) {
		CURVNET_SET(vnet_iter);
		error = hhook_add_hook_lookup(hki, flags);
		CURVNET_RESTORE();
#ifdef VIMAGE
		if (error)
			break;
#endif
	}
	VNET_LIST_RUNLOCK_NOSLEEP();

	return (error);
}

int
khelp_remove_hhook(struct hookinfo *hki)
{
	VNET_ITERATOR_DECL(vnet_iter);
	int error;

	error = 0;

	/*
	 * XXXLAS: If a helper is dynamically removing a helper hook function at
	 * runtime using this function, we should update the helper's h_hooks
	 * struct member to remove the defunct hookinfo struct.
	 */

	VNET_LIST_RLOCK_NOSLEEP();
	VNET_FOREACH(vnet_iter) {
		CURVNET_SET(vnet_iter);
		error = hhook_remove_hook_lookup(hki);
		CURVNET_RESTORE();
#ifdef VIMAGE
		if (error)
			break;
#endif
	}
	VNET_LIST_RUNLOCK_NOSLEEP();

	return (error);
}

int
khelp_modevent(module_t mod, int event_type, void *data)
{
	struct khelp_modevent_data *kmd;
	int error;

	kmd = (struct khelp_modevent_data *)data;
	error = 0;

	switch(event_type) {
	case MOD_LOAD:
		if (kmd->helper->h_flags & HELPER_NEEDS_OSD) {
			if (kmd->uma_zsize <= 0) {
				printf("Use KHELP_DECLARE_MOD_UMA() instead!\n");
				error = EDOOFUS;
				break;
			}
			kmd->helper->h_zone = uma_zcreate(kmd->name,
			    kmd->uma_zsize, kmd->umactor, kmd->umadtor, NULL,
			    NULL, 0, 0);
			if (kmd->helper->h_zone == NULL) {
				error = ENOMEM;
				break;
			}
		}
		strlcpy(kmd->helper->h_name, kmd->name, HELPER_NAME_MAXLEN);
		kmd->helper->h_hooks = kmd->hooks;
		kmd->helper->h_nhooks = kmd->nhooks;
		if (kmd->helper->mod_init != NULL)
			error = kmd->helper->mod_init();
		if (!error)
			error = khelp_register_helper(kmd->helper);
		break;

	case MOD_QUIESCE:
	case MOD_SHUTDOWN:
	case MOD_UNLOAD:
		error = khelp_deregister_helper(kmd->helper);
		if (!error) {
			if (kmd->helper->h_flags & HELPER_NEEDS_OSD)
				uma_zdestroy(kmd->helper->h_zone);
			if (kmd->helper->mod_destroy != NULL)
				kmd->helper->mod_destroy();
		} else if (error == ENOENT)
			/* Do nothing and allow unload if helper not in list. */
			error = 0;
		else if (error == EBUSY)
			printf("Khelp module \"%s\" can't unload until its "
			    "refcount drops from %d to 0.\n", kmd->name,
			    kmd->helper->h_refcount);
		break;

	default:
		error = EINVAL;
		break;
	}

	return (error);
}

/*
 * This function is called in two separate situations:
 *
 * - When the kernel is booting, it is called directly by the SYSINIT framework
 * to allow Khelp modules which were compiled into the kernel or loaded by the
 * boot loader to insert their non-virtualised hook functions into the kernel.
 *
 * - When the kernel is booting or a vnet is created, this function is also
 * called indirectly through khelp_vnet_init() by the vnet initialisation code.
 * In this situation, Khelp modules are able to insert their virtualised hook
 * functions into the virtualised hook points in the vnet which is being
 * initialised. In the case where the kernel is not compiled with "options
 * VIMAGE", this step is still run once at boot, but the hook functions get
 * transparently inserted into the standard unvirtualised network stack.
 */
static void
khelp_init(const void *vnet)
{
	struct helper *h;
	int error, i, vinit;
	int32_t htype, hid;

	error = 0;
	vinit = vnet != NULL;

	KHELP_LIST_RLOCK();
	TAILQ_FOREACH(h, &helpers, h_next) {
		for (i = 0; i < h->h_nhooks && !error; i++) {
			htype = h->h_hooks[i].hook_type;
			hid = h->h_hooks[i].hook_id;

			/*
			 * If we're doing a virtualised init (vinit != 0) and
			 * the hook point is virtualised, or we're doing a plain
			 * sysinit at boot and the hook point is not
			 * virtualised, insert the hook.
			 */
			if ((hhook_head_is_virtualised_lookup(htype, hid) ==
			    HHOOK_HEADISINVNET && vinit) ||
			    (!hhook_head_is_virtualised_lookup(htype, hid) &&
			    !vinit)) {
				error = hhook_add_hook_lookup(&h->h_hooks[i],
				    HHOOK_NOWAIT);
			}
		}

		if (error) {
			 /* Remove any helper's hooks we successfully added. */
			for (i--; i >= 0; i--)
				hhook_remove_hook_lookup(&h->h_hooks[i]);

			printf("%s: Failed to add hooks for helper \"%s\" (%p)",
				__func__, h->h_name, h);
			if (vinit)
				    printf(" to vnet %p.\n", vnet);
			else
				printf(".\n");

			error = 0;
		}
	}
	KHELP_LIST_RUNLOCK();
}

/*
 * Vnet created and being initialised.
 */
static void
khelp_vnet_init(const void *unused __unused)
{

	khelp_init(TD_TO_VNET(curthread));
}


/*
 * As the kernel boots, allow Khelp modules which were compiled into the kernel
 * or loaded by the boot loader to insert their non-virtualised hook functions
 * into the kernel.
 */
SYSINIT(khelp_init, SI_SUB_PROTO_END, SI_ORDER_FIRST, khelp_init, NULL);

/*
 * When a vnet is created and being initialised, we need to insert the helper
 * hook functions for all currently registered Khelp modules into the vnet's
 * helper hook points.  The hhook KPI provides a mechanism for subsystems which
 * export helper hook points to clean up on vnet shutdown, so we don't need a
 * VNET_SYSUNINIT for Khelp.
 */
VNET_SYSINIT(khelp_vnet_init, SI_SUB_PROTO_END, SI_ORDER_FIRST,
    khelp_vnet_init, NULL);
