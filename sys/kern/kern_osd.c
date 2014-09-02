/*-
 * Copyright (c) 2007 Pawel Jakub Dawidek <pjd@FreeBSD.org>
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
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
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/errno.h>
#include <sys/jail.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/rmlock.h>
#include <sys/sx.h>
#include <sys/queue.h>
#include <sys/proc.h>
#include <sys/osd.h>

/* OSD (Object Specific Data) */

static MALLOC_DEFINE(M_OSD, "osd", "Object Specific Data");

static int osd_debug = 0;
SYSCTL_INT(_debug, OID_AUTO, osd, CTLFLAG_RWTUN, &osd_debug, 0, "OSD debug level");

#define	OSD_DEBUG(...)	do {						\
	if (osd_debug) {						\
		printf("OSD (%s:%u): ", __func__, __LINE__);		\
		printf(__VA_ARGS__);					\
		printf("\n");						\
	}								\
} while (0)

static void do_osd_del(u_int type, struct osd *osd, u_int slot,
    int list_locked);

/*
 * Lists of objects with OSD.
 *
 * Lock key:
 *  (m) osd_module_lock
 *  (o) osd_object_lock
 *  (l) osd_list_lock
 */
static LIST_HEAD(, osd)	osd_list[OSD_LAST + 1];		/* (m) */
static osd_method_t *osd_methods[OSD_LAST + 1];		/* (m) */
static u_int osd_nslots[OSD_LAST + 1];			/* (m) */
static osd_destructor_t *osd_destructors[OSD_LAST + 1];	/* (o) */
static const u_int osd_nmethods[OSD_LAST + 1] = {
	[OSD_JAIL] = PR_MAXMETHOD,
};

static struct sx osd_module_lock[OSD_LAST + 1];
static struct rmlock osd_object_lock[OSD_LAST + 1];
static struct mtx osd_list_lock[OSD_LAST + 1];

static void
osd_default_destructor(void *value __unused)
{
	/* Do nothing. */
}

int
osd_register(u_int type, osd_destructor_t destructor, osd_method_t *methods)
{
	void *newptr;
	u_int i, m;

	KASSERT(type >= OSD_FIRST && type <= OSD_LAST, ("Invalid type."));

	/*
	 * If no destructor is given, use default one. We need to use some
	 * destructor, because NULL destructor means unused slot.
	 */
	if (destructor == NULL)
		destructor = osd_default_destructor;

	sx_xlock(&osd_module_lock[type]);
	/*
	 * First, we try to find unused slot.
	 */
	for (i = 0; i < osd_nslots[type]; i++) {
		if (osd_destructors[type][i] == NULL) {
			OSD_DEBUG("Unused slot found (type=%u, slot=%u).",
			    type, i);
			break;
		}
	}
	/*
	 * If no unused slot was found, allocate one.
	 */
	if (i == osd_nslots[type]) {
		osd_nslots[type]++;
		if (osd_nmethods[type] != 0)
			osd_methods[type] = realloc(osd_methods[type],
			    sizeof(osd_method_t) * osd_nslots[type] *
			    osd_nmethods[type], M_OSD, M_WAITOK);
		newptr = malloc(sizeof(osd_destructor_t) * osd_nslots[type],
		    M_OSD, M_WAITOK);
		rm_wlock(&osd_object_lock[type]);
		bcopy(osd_destructors[type], newptr,
		    sizeof(osd_destructor_t) * i);
		free(osd_destructors[type], M_OSD);
		osd_destructors[type] = newptr;
		rm_wunlock(&osd_object_lock[type]);
		OSD_DEBUG("New slot allocated (type=%u, slot=%u).",
		    type, i + 1);
	}

	osd_destructors[type][i] = destructor;
	if (osd_nmethods[type] != 0) {
		for (m = 0; m < osd_nmethods[type]; m++)
			osd_methods[type][i * osd_nmethods[type] + m] =
			    methods != NULL ? methods[m] : NULL;
	}
	sx_xunlock(&osd_module_lock[type]);
	return (i + 1);
}

void
osd_deregister(u_int type, u_int slot)
{
	struct osd *osd, *tosd;

	KASSERT(type >= OSD_FIRST && type <= OSD_LAST, ("Invalid type."));
	KASSERT(slot > 0, ("Invalid slot."));
	KASSERT(osd_destructors[type][slot - 1] != NULL, ("Unused slot."));

	sx_xlock(&osd_module_lock[type]);
	rm_wlock(&osd_object_lock[type]);
	/*
	 * Free all OSD for the given slot.
	 */
	mtx_lock(&osd_list_lock[type]);
	LIST_FOREACH_SAFE(osd, &osd_list[type], osd_next, tosd)
		do_osd_del(type, osd, slot, 1);
	mtx_unlock(&osd_list_lock[type]);
	/*
	 * Set destructor to NULL to free the slot.
	 */
	osd_destructors[type][slot - 1] = NULL;
	if (slot == osd_nslots[type]) {
		osd_nslots[type]--;
		osd_destructors[type] = realloc(osd_destructors[type],
		    sizeof(osd_destructor_t) * osd_nslots[type], M_OSD,
		    M_NOWAIT | M_ZERO);
		if (osd_nmethods[type] != 0)
			osd_methods[type] = realloc(osd_methods[type],
			    sizeof(osd_method_t) * osd_nslots[type] *
			    osd_nmethods[type], M_OSD, M_NOWAIT | M_ZERO);
		/*
		 * We always reallocate to smaller size, so we assume it will
		 * always succeed.
		 */
		KASSERT(osd_destructors[type] != NULL &&
		    (osd_nmethods[type] == 0 || osd_methods[type] != NULL),
		    ("realloc() failed"));
		OSD_DEBUG("Deregistration of the last slot (type=%u, slot=%u).",
		    type, slot);
	} else {
		OSD_DEBUG("Slot deregistration (type=%u, slot=%u).",
		    type, slot);
	}
	rm_wunlock(&osd_object_lock[type]);
	sx_xunlock(&osd_module_lock[type]);
}

int
osd_set(u_int type, struct osd *osd, u_int slot, void *value)
{
	struct rm_priotracker tracker;

	KASSERT(type >= OSD_FIRST && type <= OSD_LAST, ("Invalid type."));
	KASSERT(slot > 0, ("Invalid slot."));
	KASSERT(osd_destructors[type][slot - 1] != NULL, ("Unused slot."));

	rm_rlock(&osd_object_lock[type], &tracker);
	if (slot > osd->osd_nslots) {
		if (value == NULL) {
			OSD_DEBUG(
			    "Not allocating null slot (type=%u, slot=%u).",
			    type, slot);
			rm_runlock(&osd_object_lock[type], &tracker);
			return (0);
		} else if (osd->osd_nslots == 0) {
			/*
			 * First OSD for this object, so we need to allocate
			 * space and put it onto the list.
			 */
			osd->osd_slots = malloc(sizeof(void *) * slot, M_OSD,
			    M_NOWAIT | M_ZERO);
			if (osd->osd_slots == NULL) {
				rm_runlock(&osd_object_lock[type], &tracker);
				return (ENOMEM);
			}
			osd->osd_nslots = slot;
			mtx_lock(&osd_list_lock[type]);
			LIST_INSERT_HEAD(&osd_list[type], osd, osd_next);
			mtx_unlock(&osd_list_lock[type]);
			OSD_DEBUG("Setting first slot (type=%u).", type);
		} else {
			void *newptr;

			/*
			 * Too few slots allocated here, needs to extend
			 * the array.
			 */
			newptr = realloc(osd->osd_slots, sizeof(void *) * slot,
			    M_OSD, M_NOWAIT | M_ZERO);
			if (newptr == NULL) {
				rm_runlock(&osd_object_lock[type], &tracker);
				return (ENOMEM);
			}
			osd->osd_slots = newptr;
			osd->osd_nslots = slot;
			OSD_DEBUG("Growing slots array (type=%u).", type);
		}
	}
	OSD_DEBUG("Setting slot value (type=%u, slot=%u, value=%p).", type,
	    slot, value);
	osd->osd_slots[slot - 1] = value;
	rm_runlock(&osd_object_lock[type], &tracker);
	return (0);
}

void *
osd_get(u_int type, struct osd *osd, u_int slot)
{
	struct rm_priotracker tracker;
	void *value;

	KASSERT(type >= OSD_FIRST && type <= OSD_LAST, ("Invalid type."));
	KASSERT(slot > 0, ("Invalid slot."));
	KASSERT(osd_destructors[type][slot - 1] != NULL, ("Unused slot."));

	rm_rlock(&osd_object_lock[type], &tracker);
	if (slot > osd->osd_nslots) {
		value = NULL;
		OSD_DEBUG("Slot doesn't exist (type=%u, slot=%u).", type, slot);
	} else {
		value = osd->osd_slots[slot - 1];
		OSD_DEBUG("Returning slot value (type=%u, slot=%u, value=%p).",
		    type, slot, value);
	}
	rm_runlock(&osd_object_lock[type], &tracker);
	return (value);
}

void
osd_del(u_int type, struct osd *osd, u_int slot)
{
	struct rm_priotracker tracker;

	rm_rlock(&osd_object_lock[type], &tracker);
	do_osd_del(type, osd, slot, 0);
	rm_runlock(&osd_object_lock[type], &tracker);
}

static void
do_osd_del(u_int type, struct osd *osd, u_int slot, int list_locked)
{
	int i;

	KASSERT(type >= OSD_FIRST && type <= OSD_LAST, ("Invalid type."));
	KASSERT(slot > 0, ("Invalid slot."));
	KASSERT(osd_destructors[type][slot - 1] != NULL, ("Unused slot."));

	OSD_DEBUG("Deleting slot (type=%u, slot=%u).", type, slot);

	if (slot > osd->osd_nslots) {
		OSD_DEBUG("Slot doesn't exist (type=%u, slot=%u).", type, slot);
		return;
	}
	if (osd->osd_slots[slot - 1] != NULL) {
		osd_destructors[type][slot - 1](osd->osd_slots[slot - 1]);
		osd->osd_slots[slot - 1] = NULL;
	}
	for (i = osd->osd_nslots - 1; i >= 0; i--) {
		if (osd->osd_slots[i] != NULL) {
			OSD_DEBUG("Slot still has a value (type=%u, slot=%u).",
			    type, i + 1);
			break;
		}
	}
	if (i == -1) {
		/* No values left for this object. */
		OSD_DEBUG("No more slots left (type=%u).", type);
		if (!list_locked)
			mtx_lock(&osd_list_lock[type]);
		LIST_REMOVE(osd, osd_next);
		if (!list_locked)
			mtx_unlock(&osd_list_lock[type]);
		free(osd->osd_slots, M_OSD);
		osd->osd_slots = NULL;
		osd->osd_nslots = 0;
	} else if (slot == osd->osd_nslots) {
		/* This was the last slot. */
		osd->osd_slots = realloc(osd->osd_slots,
		    sizeof(void *) * (i + 1), M_OSD, M_NOWAIT | M_ZERO);
		/*
		 * We always reallocate to smaller size, so we assume it will
		 * always succeed.
		 */
		KASSERT(osd->osd_slots != NULL, ("realloc() failed"));
		osd->osd_nslots = i + 1;
		OSD_DEBUG("Reducing slots array to %u (type=%u).",
		    osd->osd_nslots, type);
	}
}

int
osd_call(u_int type, u_int method, void *obj, void *data)
{
	osd_method_t methodfun;
	int error, i;

	KASSERT(type >= OSD_FIRST && type <= OSD_LAST, ("Invalid type."));
	KASSERT(method < osd_nmethods[type], ("Invalid method."));

	/*
	 * Call this method for every slot that defines it, stopping if an
	 * error is encountered.
	 */
	error = 0;
	sx_slock(&osd_module_lock[type]);
	for (i = 0; i < osd_nslots[type]; i++) {
		methodfun =
		    osd_methods[type][i * osd_nmethods[type] + method];
		if (methodfun != NULL && (error = methodfun(obj, data)) != 0)
			break;
	}
	sx_sunlock(&osd_module_lock[type]);
	return (error);
}

void
osd_exit(u_int type, struct osd *osd)
{
	struct rm_priotracker tracker;
	u_int i;

	KASSERT(type >= OSD_FIRST && type <= OSD_LAST, ("Invalid type."));

	if (osd->osd_nslots == 0) {
		KASSERT(osd->osd_slots == NULL, ("Non-null osd_slots."));
		/* No OSD attached, just leave. */
		return;
	}

	rm_rlock(&osd_object_lock[type], &tracker);
	for (i = 1; i <= osd->osd_nslots; i++) {
		if (osd_destructors[type][i - 1] != NULL)
			do_osd_del(type, osd, i, 0);
		else
			OSD_DEBUG("Unused slot (type=%u, slot=%u).", type, i);
	}
	rm_runlock(&osd_object_lock[type], &tracker);
	OSD_DEBUG("Object exit (type=%u).", type);
}

static void
osd_init(void *arg __unused)
{
	u_int i;

	for (i = OSD_FIRST; i <= OSD_LAST; i++) {
		osd_nslots[i] = 0;
		LIST_INIT(&osd_list[i]);
		sx_init(&osd_module_lock[i], "osd_module");
		rm_init(&osd_object_lock[i], "osd_object");
		mtx_init(&osd_list_lock[i], "osd_list", NULL, MTX_DEF);
		osd_destructors[i] = NULL;
		osd_methods[i] = NULL;
	}
}
SYSINIT(osd, SI_SUB_LOCK, SI_ORDER_ANY, osd_init, NULL);
