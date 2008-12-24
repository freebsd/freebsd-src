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
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/queue.h>
#include <sys/proc.h>
#include <sys/osd.h>

/* OSD (Object Specific Data) */

static MALLOC_DEFINE(M_OSD, "osd", "Object Specific Data");

static int osd_debug = 0;
TUNABLE_INT("debug.osd", &osd_debug);
SYSCTL_INT(_debug, OID_AUTO, osd, CTLFLAG_RW, &osd_debug, 0, "OSD debug level");

#define	OSD_DEBUG(...)	do {						\
	if (osd_debug) {						\
		printf("OSD (%s:%u): ", __func__, __LINE__);		\
		printf(__VA_ARGS__);					\
		printf("\n");						\
	}								\
} while (0)

/*
 * Lists of objects with OSD.
 */
static LIST_HEAD(, osd)	osd_list[OSD_LAST + 1];
static osd_destructor_t *osd_destructors[OSD_LAST + 1];
static u_int osd_nslots[OSD_LAST + 1];
static struct mtx osd_lock[OSD_LAST + 1];

static void
osd_default_destructor(void *value __unused)
{
	/* Do nothing. */
}

int
osd_register(u_int type, osd_destructor_t destructor)
{
	void *newptr;
	u_int i;

	KASSERT(type >= OSD_FIRST && type <= OSD_LAST, ("Invalid type."));

	/*
	 * If no destructor is given, use default one. We need to use some
	 * destructor, because NULL destructor means unused slot.
	 */
	if (destructor == NULL)
		destructor = osd_default_destructor;

	mtx_lock(&osd_lock[type]);
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
		newptr = realloc(osd_destructors[type],
		    sizeof(osd_destructor_t) * osd_nslots[type], M_OSD,
		    M_NOWAIT | M_ZERO);
		if (newptr == NULL) {
			mtx_unlock(&osd_lock[type]);
			return (0);
		}
		osd_destructors[type] = newptr;
		OSD_DEBUG("New slot allocated (type=%u, slot=%u).",
		    type, i + 1);
	}
	osd_destructors[type][i] = destructor;
	mtx_unlock(&osd_lock[type]);
	return (i + 1);
}

void
osd_deregister(u_int type, u_int slot)
{
	struct osd *osd, *tosd;

	KASSERT(type >= OSD_FIRST && type <= OSD_LAST, ("Invalid type."));
	KASSERT(slot > 0, ("Invalid slot."));
	KASSERT(osd_destructors[type][slot - 1] != NULL, ("Unused slot."));

	mtx_lock(&osd_lock[type]);
	/*
	 * Free all OSD for the given slot.
	 */
	LIST_FOREACH_SAFE(osd, &osd_list[type], osd_next, tosd) {
		osd_del(type, osd, slot);
	}
	/*
	 * Set destructor to NULL to free the slot.
	 */
	osd_destructors[type][slot - 1] = NULL;
	if (slot == osd_nslots[type]) {
		osd_nslots[type]--;
		osd_destructors[type] = realloc(osd_destructors[type],
		    sizeof(osd_destructor_t) * osd_nslots[type], M_OSD,
		    M_NOWAIT | M_ZERO);
		/*
		 * We always reallocate to smaller size, so we assume it will
		 * always succeed.
		 */
		KASSERT(osd_destructors[type] != NULL, ("realloc() failed"));
		OSD_DEBUG("Deregistration of the last slot (type=%u, slot=%u).",
		    type, slot);
	} else {
		OSD_DEBUG("Slot deregistration (type=%u, slot=%u).",
		    type, slot);
	}
	mtx_unlock(&osd_lock[type]);
}

int
osd_set(u_int type, struct osd *osd, u_int slot, void *value)
{

	KASSERT(type >= OSD_FIRST && type <= OSD_LAST, ("Invalid type."));
	KASSERT(slot > 0, ("Invalid slot."));
	KASSERT(osd_destructors[type][slot - 1] != NULL, ("Unused slot."));

	if (osd->osd_nslots == 0) {
		/*
		 * First OSD for this object, so we need to allocate space and
		 * put it onto the list.
		 */
		osd->osd_slots = malloc(sizeof(void *) * slot, M_OSD,
		    M_NOWAIT | M_ZERO);
		if (osd->osd_slots == NULL)
			return (ENOMEM);
		osd->osd_nslots = slot;
		mtx_lock(&osd_lock[type]);
		LIST_INSERT_HEAD(&osd_list[type], osd, osd_next);
		mtx_unlock(&osd_lock[type]);
		OSD_DEBUG("Setting first slot (type=%u).", type);
	} else if (slot > osd->osd_nslots) {
		void *newptr;

		/*
		 * Too few slots allocated here, needs to extend the array.
		 */
		newptr = realloc(osd->osd_slots, sizeof(void *) * slot, M_OSD,
		    M_NOWAIT | M_ZERO);
		if (newptr == NULL)
			return (ENOMEM);
		osd->osd_slots = newptr;
		osd->osd_nslots = slot;
		OSD_DEBUG("Growing slots array (type=%u).", type);
	}
	OSD_DEBUG("Setting slot value (type=%u, slot=%u, value=%p).", type,
	    slot, value);
	osd->osd_slots[slot - 1] = value;
	return (0);
}

void *
osd_get(u_int type, struct osd *osd, u_int slot)
{

	KASSERT(type >= OSD_FIRST && type <= OSD_LAST, ("Invalid type."));
	KASSERT(slot > 0, ("Invalid slot."));
	KASSERT(osd_destructors[type][slot - 1] != NULL, ("Unused slot."));

	if (slot > osd->osd_nslots) {
		OSD_DEBUG("Slot doesn't exist (type=%u, slot=%u).", type, slot);
		return (NULL);
	}

	OSD_DEBUG("Returning slot value (type=%u, slot=%u, value=%p).", type,
	    slot, osd->osd_slots[slot - 1]);
	return (osd->osd_slots[slot - 1]);
}

void
osd_del(u_int type, struct osd *osd, u_int slot)
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
	osd_destructors[type][slot - 1](osd->osd_slots[slot - 1]);
	osd->osd_slots[slot - 1] = NULL;
	for (i = osd->osd_nslots - 1; i >= 0; i--) {
		if (osd->osd_slots[i] != NULL) {
			OSD_DEBUG("Slot still has a value (type=%u, slot=%u).", type, i + 1);
			break;
		}
	}
	if (i == -1) {
		int unlock;

		/* No values left for this object. */
		OSD_DEBUG("No more slots left (type=%u).", type);
		if ((unlock = !mtx_owned(&osd_lock[type])))
			mtx_lock(&osd_lock[type]);
		LIST_REMOVE(osd, osd_next);
		if (unlock)
			mtx_unlock(&osd_lock[type]);
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

void
osd_exit(u_int type, struct osd *osd)
{
	u_int i;

	KASSERT(type >= OSD_FIRST && type <= OSD_LAST, ("Invalid type."));

	if (osd->osd_nslots == 0) {
		KASSERT(osd->osd_slots == NULL, ("Non-null osd_slots."));
		/* No OSD attached, just leave. */
		return;
	}

	mtx_lock(&osd_lock[type]);
	for (i = 1; i <= osd->osd_nslots; i++)
		osd_del(type, osd, i);
	mtx_unlock(&osd_lock[type]);
	OSD_DEBUG("Object exit (type=%u).", type);
}

static void
osd_init(void *arg __unused)
{
	u_int i;

	for (i = OSD_FIRST; i <= OSD_LAST; i++) {
		osd_nslots[i] = 0;
		LIST_INIT(&osd_list[i]);
		mtx_init(&osd_lock[i], "osd", NULL, MTX_DEF);
		osd_destructors[i] = NULL;
	}
}
SYSINIT(osd, SI_SUB_LOCK, SI_ORDER_ANY, osd_init, NULL);
