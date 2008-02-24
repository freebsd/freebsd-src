/*-
 * Copyright (c) 2004-2006 Pawel Jakub Dawidek <pjd@FreeBSD.org>
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
__FBSDID("$FreeBSD: src/sys/geom/mirror/g_mirror_ctl.c,v 1.18 2006/11/01 22:51:49 pjd Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/bio.h>
#include <sys/sysctl.h>
#include <sys/malloc.h>
#include <sys/bitstring.h>
#include <vm/uma.h>
#include <machine/atomic.h>
#include <geom/geom.h>
#include <sys/proc.h>
#include <sys/kthread.h>
#include <geom/mirror/g_mirror.h>


static struct g_mirror_softc *
g_mirror_find_device(struct g_class *mp, const char *name)
{
	struct g_mirror_softc *sc;
	struct g_geom *gp;

	g_topology_lock();
	LIST_FOREACH(gp, &mp->geom, geom) {
		sc = gp->softc;
		if (sc == NULL)
			continue;
		if ((sc->sc_flags & G_MIRROR_DEVICE_FLAG_DESTROY) != 0)
			continue;
		if (strcmp(gp->name, name) == 0 ||
		    strcmp(sc->sc_name, name) == 0) {
			g_topology_unlock();
			sx_xlock(&sc->sc_lock);
			return (sc);
		}
	}
	g_topology_unlock();
	return (NULL);
}

static struct g_mirror_disk *
g_mirror_find_disk(struct g_mirror_softc *sc, const char *name)
{
	struct g_mirror_disk *disk;

	sx_assert(&sc->sc_lock, SX_XLOCKED);
	if (strncmp(name, "/dev/", 5) == 0)
		name += 5;
	LIST_FOREACH(disk, &sc->sc_disks, d_next) {
		if (disk->d_consumer == NULL)
			continue;
		if (disk->d_consumer->provider == NULL)
			continue;
		if (strcmp(disk->d_consumer->provider->name, name) == 0)
			return (disk);
	}
	return (NULL);
}

static void
g_mirror_ctl_configure(struct gctl_req *req, struct g_class *mp)
{
	struct g_mirror_softc *sc;
	struct g_mirror_disk *disk;
	const char *name, *balancep;
	intmax_t *slicep;
	uint32_t slice;
	uint8_t balance;
	int *autosync, *noautosync, *failsync, *nofailsync, *hardcode, *dynamic;
	int *nargs, do_sync = 0, dirty = 1;

	nargs = gctl_get_paraml(req, "nargs", sizeof(*nargs));
	if (nargs == NULL) {
		gctl_error(req, "No '%s' argument.", "nargs");
		return;
	}
	if (*nargs != 1) {
		gctl_error(req, "Invalid number of arguments.");
		return;
	}
	name = gctl_get_asciiparam(req, "arg0");
	if (name == NULL) {
		gctl_error(req, "No 'arg%u' argument.", 0);
		return;
	}
	balancep = gctl_get_asciiparam(req, "balance");
	if (balancep == NULL) {
		gctl_error(req, "No '%s' argument.", "balance");
		return;
	}
	autosync = gctl_get_paraml(req, "autosync", sizeof(*autosync));
	if (autosync == NULL) {
		gctl_error(req, "No '%s' argument.", "autosync");
		return;
	}
	noautosync = gctl_get_paraml(req, "noautosync", sizeof(*noautosync));
	if (noautosync == NULL) {
		gctl_error(req, "No '%s' argument.", "noautosync");
		return;
	}
	failsync = gctl_get_paraml(req, "failsync", sizeof(*failsync));
	if (failsync == NULL) {
		gctl_error(req, "No '%s' argument.", "failsync");
		return;
	}
	nofailsync = gctl_get_paraml(req, "nofailsync", sizeof(*nofailsync));
	if (nofailsync == NULL) {
		gctl_error(req, "No '%s' argument.", "nofailsync");
		return;
	}
	hardcode = gctl_get_paraml(req, "hardcode", sizeof(*hardcode));
	if (hardcode == NULL) {
		gctl_error(req, "No '%s' argument.", "hardcode");
		return;
	}
	dynamic = gctl_get_paraml(req, "dynamic", sizeof(*dynamic));
	if (dynamic == NULL) {
		gctl_error(req, "No '%s' argument.", "dynamic");
		return;
	}
	if (*autosync && *noautosync) {
		gctl_error(req, "'%s' and '%s' specified.", "autosync",
		    "noautosync");
		return;
	}
	if (*failsync && *nofailsync) {
		gctl_error(req, "'%s' and '%s' specified.", "failsync",
		    "nofailsync");
		return;
	}
	if (*hardcode && *dynamic) {
		gctl_error(req, "'%s' and '%s' specified.", "hardcode",
		    "dynamic");
		return;
	}
	sc = g_mirror_find_device(mp, name);
	if (sc == NULL) {
		gctl_error(req, "No such device: %s.", name);
		return;
	}
	if (strcmp(balancep, "none") == 0)
		balance = sc->sc_balance;
	else {
		if (balance_id(balancep) == -1) {
			gctl_error(req, "Invalid balance algorithm.");
			sx_xunlock(&sc->sc_lock);
			return;
		}
		balance = balance_id(balancep);
	}
	slicep = gctl_get_paraml(req, "slice", sizeof(*slicep));
	if (slicep == NULL) {
		gctl_error(req, "No '%s' argument.", "slice");
		sx_xunlock(&sc->sc_lock);
		return;
	}
	if (*slicep == -1)
		slice = sc->sc_slice;
	else
		slice = *slicep;
	if (g_mirror_ndisks(sc, -1) < sc->sc_ndisks) {
		sx_xunlock(&sc->sc_lock);
		gctl_error(req, "Not all disks connected. Try 'forget' command "
		    "first.");
		return;
	}
	if (sc->sc_balance == balance && sc->sc_slice == slice && !*autosync &&
	    !*noautosync && !*failsync && !*nofailsync && !*hardcode &&
	    !*dynamic) {
		sx_xunlock(&sc->sc_lock);
		gctl_error(req, "Nothing has changed.");
		return;
	}
	sc->sc_balance = balance;
	sc->sc_slice = slice;
	if ((sc->sc_flags & G_MIRROR_DEVICE_FLAG_NOAUTOSYNC) != 0) {
		if (*autosync) {
			sc->sc_flags &= ~G_MIRROR_DEVICE_FLAG_NOAUTOSYNC;
			do_sync = 1;
		}
	} else {
		if (*noautosync)
			sc->sc_flags |= G_MIRROR_DEVICE_FLAG_NOAUTOSYNC;
	}
	if ((sc->sc_flags & G_MIRROR_DEVICE_FLAG_NOFAILSYNC) != 0) {
		if (*failsync)
			sc->sc_flags &= ~G_MIRROR_DEVICE_FLAG_NOFAILSYNC;
	} else {
		if (*nofailsync) {
			sc->sc_flags |= G_MIRROR_DEVICE_FLAG_NOFAILSYNC;
			dirty = 0;
		}
	}
	LIST_FOREACH(disk, &sc->sc_disks, d_next) {
		if (do_sync) {
			if (disk->d_state == G_MIRROR_DISK_STATE_SYNCHRONIZING)
				disk->d_flags &= ~G_MIRROR_DISK_FLAG_FORCE_SYNC;
		}
		if (*hardcode)
			disk->d_flags |= G_MIRROR_DISK_FLAG_HARDCODED;
		else if (*dynamic)
			disk->d_flags &= ~G_MIRROR_DISK_FLAG_HARDCODED;
		if (!dirty)
			disk->d_flags &= ~G_MIRROR_DISK_FLAG_DIRTY;
		g_mirror_update_metadata(disk);
		if (do_sync) {
			if (disk->d_state == G_MIRROR_DISK_STATE_STALE) {
				g_mirror_event_send(disk,
				    G_MIRROR_DISK_STATE_DISCONNECTED,
				    G_MIRROR_EVENT_DONTWAIT);
			}
		}
	}
	sx_xunlock(&sc->sc_lock);
}

static void
g_mirror_ctl_rebuild(struct gctl_req *req, struct g_class *mp)
{
	struct g_mirror_metadata md;
	struct g_mirror_softc *sc;
	struct g_mirror_disk *disk;
	struct g_provider *pp;
	const char *name;
	char param[16];
	int error, *nargs;
	u_int i;

	nargs = gctl_get_paraml(req, "nargs", sizeof(*nargs));
	if (nargs == NULL) {
		gctl_error(req, "No '%s' argument.", "nargs");
		return;
	}
	if (*nargs < 2) {
		gctl_error(req, "Too few arguments.");
		return;
	}
	name = gctl_get_asciiparam(req, "arg0");
	if (name == NULL) {
		gctl_error(req, "No 'arg%u' argument.", 0);
		return;
	}
	sc = g_mirror_find_device(mp, name);
	if (sc == NULL) {
		gctl_error(req, "No such device: %s.", name);
		return;
	}
	for (i = 1; i < (u_int)*nargs; i++) {
		snprintf(param, sizeof(param), "arg%u", i);
		name = gctl_get_asciiparam(req, param);
		if (name == NULL) {
			gctl_error(req, "No 'arg%u' argument.", i);
			continue;
		}
		disk = g_mirror_find_disk(sc, name);
		if (disk == NULL) {
			gctl_error(req, "No such provider: %s.", name);
			continue;
		}
		if (g_mirror_ndisks(sc, G_MIRROR_DISK_STATE_ACTIVE) == 1 &&
		    disk->d_state == G_MIRROR_DISK_STATE_ACTIVE) {
			/*
			 * This is the last active disk. There will be nothing
			 * to rebuild it from, so deny this request.
			 */
			gctl_error(req,
			    "Provider %s is the last active provider in %s.",
			    name, sc->sc_geom->name);
			break;
		}
		/*
		 * Do rebuild by resetting syncid, disconnecting the disk and
		 * connecting it again.
		 */
		disk->d_sync.ds_syncid = 0;
		if ((sc->sc_flags & G_MIRROR_DEVICE_FLAG_NOAUTOSYNC) != 0)
			disk->d_flags |= G_MIRROR_DISK_FLAG_FORCE_SYNC;
		g_mirror_update_metadata(disk);
		pp = disk->d_consumer->provider;
		g_topology_lock();
		error = g_mirror_read_metadata(disk->d_consumer, &md);
		g_topology_unlock();
		g_mirror_event_send(disk, G_MIRROR_DISK_STATE_DISCONNECTED,
		    G_MIRROR_EVENT_WAIT);
		if (error != 0) {
			gctl_error(req, "Cannot read metadata from %s.",
			    pp->name);
			continue;
		}
		error = g_mirror_add_disk(sc, pp, &md);
		if (error != 0) {
			gctl_error(req, "Cannot reconnect component %s.",
			    pp->name);
			continue;
		}
	}
	sx_xunlock(&sc->sc_lock);
}

static void
g_mirror_ctl_insert(struct gctl_req *req, struct g_class *mp)
{
	struct g_mirror_softc *sc;
	struct g_mirror_disk *disk;
	struct g_mirror_metadata md;
	struct g_provider *pp;
	struct g_consumer *cp;
	intmax_t *priority;
	const char *name;
	char param[16];
	u_char *sector;
	u_int i, n;
	int error, *nargs, *hardcode, *inactive;
	struct {
		struct g_provider	*provider;
		struct g_consumer	*consumer;
	} *disks;

	nargs = gctl_get_paraml(req, "nargs", sizeof(*nargs));
	if (nargs == NULL) {
		gctl_error(req, "No '%s' argument.", "nargs");
		return;
	}
	if (*nargs < 2) {
		gctl_error(req, "Too few arguments.");
		return;
	}
	priority = gctl_get_paraml(req, "priority", sizeof(*priority));
	if (priority == NULL) {
		gctl_error(req, "No '%s' argument.", "priority");
		return;
	}
	inactive = gctl_get_paraml(req, "inactive", sizeof(*inactive));
	if (inactive == NULL) {
		gctl_error(req, "No '%s' argument.", "inactive");
		return;
	}
	hardcode = gctl_get_paraml(req, "hardcode", sizeof(*hardcode));
	if (hardcode == NULL) {
		gctl_error(req, "No '%s' argument.", "hardcode");
		return;
	}
	name = gctl_get_asciiparam(req, "arg0");
	if (name == NULL) {
		gctl_error(req, "No 'arg%u' argument.", 0);
		return;
	}
	sc = g_mirror_find_device(mp, name);
	if (sc == NULL) {
		gctl_error(req, "No such device: %s.", name);
		return;
	}
	if (g_mirror_ndisks(sc, -1) < sc->sc_ndisks) {
		gctl_error(req, "Not all disks connected.");
		sx_xunlock(&sc->sc_lock);
		return;
	}

	disks = g_malloc(sizeof(*disks) * (*nargs), M_WAITOK | M_ZERO);
	g_topology_lock();
	for (i = 1, n = 0; i < (u_int)*nargs; i++) {
		snprintf(param, sizeof(param), "arg%u", i);
		name = gctl_get_asciiparam(req, param);
		if (name == NULL) {
			gctl_error(req, "No 'arg%u' argument.", i);
			continue;
		}
		if (g_mirror_find_disk(sc, name) != NULL) {
			gctl_error(req, "Provider %s already inserted.", name);
			continue;
		}
		if (strncmp(name, "/dev/", 5) == 0)
			name += 5;
		pp = g_provider_by_name(name);
		if (pp == NULL) {
			gctl_error(req, "Unknown provider %s.", name);
			continue;
		}
		if (sc->sc_provider->mediasize >
		    pp->mediasize - pp->sectorsize) {
			gctl_error(req, "Provider %s too small.", name);
			continue;
		}
		if ((sc->sc_provider->sectorsize % pp->sectorsize) != 0) {
			gctl_error(req, "Invalid sectorsize of provider %s.",
			    name);
			continue;
		}
		cp = g_new_consumer(sc->sc_geom);
		if (g_attach(cp, pp) != 0) {
			g_destroy_consumer(cp);
			gctl_error(req, "Cannot attach to provider %s.", name);
			continue;
		}
		if (g_access(cp, 0, 1, 1) != 0) {
			g_detach(cp);
			g_destroy_consumer(cp);
			gctl_error(req, "Cannot access provider %s.", name);
			continue;
		}
		disks[n].provider = pp;
		disks[n].consumer = cp;
		n++;
	}
	if (n == 0) {
		g_topology_unlock();
		sx_xunlock(&sc->sc_lock);
		g_free(disks);
		return;
	}
	sc->sc_ndisks += n;
again:
	for (i = 0; i < n; i++) {
		if (disks[i].consumer == NULL)
			continue;
		g_mirror_fill_metadata(sc, NULL, &md);
		md.md_priority = *priority;
		if (*inactive)
			md.md_dflags |= G_MIRROR_DISK_FLAG_INACTIVE;
		pp = disks[i].provider;
		if (*hardcode) {
			strlcpy(md.md_provider, pp->name,
			    sizeof(md.md_provider));
		} else {
			bzero(md.md_provider, sizeof(md.md_provider));
		}
		md.md_provsize = pp->mediasize;
		sector = g_malloc(pp->sectorsize, M_WAITOK);
		mirror_metadata_encode(&md, sector);
		error = g_write_data(disks[i].consumer,
		    pp->mediasize - pp->sectorsize, sector, pp->sectorsize);
		g_free(sector);
		if (error != 0) {
			gctl_error(req, "Cannot store metadata on %s.",
			    pp->name);
			g_access(disks[i].consumer, 0, -1, -1);
			g_detach(disks[i].consumer);
			g_destroy_consumer(disks[i].consumer);
			disks[i].consumer = NULL;
			disks[i].provider = NULL;
			sc->sc_ndisks--;
			goto again;
		}
	}
	g_topology_unlock();
	if (i == 0) {
		/* All writes failed. */
		sx_xunlock(&sc->sc_lock);
		g_free(disks);
		return;
	}
	LIST_FOREACH(disk, &sc->sc_disks, d_next) {
		g_mirror_update_metadata(disk);
	}
	/*
	 * Release provider and wait for retaste.
	 */
	g_topology_lock();
	for (i = 0; i < n; i++) {
		if (disks[i].consumer == NULL)
			continue;
		g_access(disks[i].consumer, 0, -1, -1);
		g_detach(disks[i].consumer);
		g_destroy_consumer(disks[i].consumer);
	}
	g_topology_unlock();
	sx_xunlock(&sc->sc_lock);
	g_free(disks);
}

static void
g_mirror_ctl_remove(struct gctl_req *req, struct g_class *mp)
{
	struct g_mirror_softc *sc;
	struct g_mirror_disk *disk;
	const char *name;
	char param[16];
	int *nargs;
	u_int i;

	nargs = gctl_get_paraml(req, "nargs", sizeof(*nargs));
	if (nargs == NULL) {
		gctl_error(req, "No '%s' argument.", "nargs");
		return;
	}
	if (*nargs < 2) {
		gctl_error(req, "Too few arguments.");
		return;
	}
	name = gctl_get_asciiparam(req, "arg0");
	if (name == NULL) {
		gctl_error(req, "No 'arg%u' argument.", 0);
		return;
	}
	sc = g_mirror_find_device(mp, name);
	if (sc == NULL) {
		gctl_error(req, "No such device: %s.", name);
		return;
	}
	if (g_mirror_ndisks(sc, -1) < sc->sc_ndisks) {
		sx_xunlock(&sc->sc_lock);
		gctl_error(req, "Not all disks connected. Try 'forget' command "
		    "first.");
		return;
	}
	for (i = 1; i < (u_int)*nargs; i++) {
		snprintf(param, sizeof(param), "arg%u", i);
		name = gctl_get_asciiparam(req, param);
		if (name == NULL) {
			gctl_error(req, "No 'arg%u' argument.", i);
			continue;
		}
		disk = g_mirror_find_disk(sc, name);
		if (disk == NULL) {
			gctl_error(req, "No such provider: %s.", name);
			continue;
		}
		g_mirror_event_send(disk, G_MIRROR_DISK_STATE_DESTROY,
		    G_MIRROR_EVENT_DONTWAIT);
	}
	sx_xunlock(&sc->sc_lock);
}

static void
g_mirror_ctl_deactivate(struct gctl_req *req, struct g_class *mp)
{
	struct g_mirror_softc *sc;
	struct g_mirror_disk *disk;
	const char *name;
	char param[16];
	int *nargs;
	u_int i;

	nargs = gctl_get_paraml(req, "nargs", sizeof(*nargs));
	if (nargs == NULL) {
		gctl_error(req, "No '%s' argument.", "nargs");
		return;
	}
	if (*nargs < 2) {
		gctl_error(req, "Too few arguments.");
		return;
	}
	name = gctl_get_asciiparam(req, "arg0");
	if (name == NULL) {
		gctl_error(req, "No 'arg%u' argument.", 0);
		return;
	}
	sc = g_mirror_find_device(mp, name);
	if (sc == NULL) {
		gctl_error(req, "No such device: %s.", name);
		return;
	}
	for (i = 1; i < (u_int)*nargs; i++) {
		snprintf(param, sizeof(param), "arg%u", i);
		name = gctl_get_asciiparam(req, param);
		if (name == NULL) {
			gctl_error(req, "No 'arg%u' argument.", i);
			continue;
		}
		disk = g_mirror_find_disk(sc, name);
		if (disk == NULL) {
			gctl_error(req, "No such provider: %s.", name);
			continue;
		}
		disk->d_flags |= G_MIRROR_DISK_FLAG_INACTIVE;
		disk->d_flags &= ~G_MIRROR_DISK_FLAG_FORCE_SYNC;
		g_mirror_update_metadata(disk);
		sc->sc_bump_id |= G_MIRROR_BUMP_SYNCID;
		g_mirror_event_send(disk, G_MIRROR_DISK_STATE_DISCONNECTED,
		    G_MIRROR_EVENT_DONTWAIT);
	}
	sx_xunlock(&sc->sc_lock);
}

static void
g_mirror_ctl_forget(struct gctl_req *req, struct g_class *mp)
{
	struct g_mirror_softc *sc;
	struct g_mirror_disk *disk;
	const char *name;
	char param[16];
	int *nargs;
	u_int i;

	nargs = gctl_get_paraml(req, "nargs", sizeof(*nargs));
	if (nargs == NULL) {
		gctl_error(req, "No '%s' argument.", "nargs");
		return;
	}
	if (*nargs < 1) {
		gctl_error(req, "Missing device(s).");
		return;
	}

	for (i = 0; i < (u_int)*nargs; i++) {
		snprintf(param, sizeof(param), "arg%u", i);
		name = gctl_get_asciiparam(req, param);
		if (name == NULL) {
			gctl_error(req, "No 'arg%u' argument.", i);
			return;
		}
		sc = g_mirror_find_device(mp, name);
		if (sc == NULL) {
			gctl_error(req, "No such device: %s.", name);
			return;
		}
		if (g_mirror_ndisks(sc, -1) == sc->sc_ndisks) {
			sx_xunlock(&sc->sc_lock);
			G_MIRROR_DEBUG(1,
			    "All disks connected in %s, skipping.",
			    sc->sc_name);
			continue;
		}
		sc->sc_ndisks = g_mirror_ndisks(sc, -1);
		LIST_FOREACH(disk, &sc->sc_disks, d_next) {
			g_mirror_update_metadata(disk);
		}
		sx_xunlock(&sc->sc_lock);
	}
}

static void
g_mirror_ctl_stop(struct gctl_req *req, struct g_class *mp)
{
	struct g_mirror_softc *sc;
	int *force, *nargs, error;
	const char *name;
	char param[16];
	u_int i;
	int how;

	nargs = gctl_get_paraml(req, "nargs", sizeof(*nargs));
	if (nargs == NULL) {
		gctl_error(req, "No '%s' argument.", "nargs");
		return;
	}
	if (*nargs < 1) {
		gctl_error(req, "Missing device(s).");
		return;
	}
	force = gctl_get_paraml(req, "force", sizeof(*force));
	if (force == NULL) {
		gctl_error(req, "No '%s' argument.", "force");
		return;
	}
	if (*force)
		how = G_MIRROR_DESTROY_HARD;
	else
		how = G_MIRROR_DESTROY_SOFT;

	for (i = 0; i < (u_int)*nargs; i++) {
		snprintf(param, sizeof(param), "arg%u", i);
		name = gctl_get_asciiparam(req, param);
		if (name == NULL) {
			gctl_error(req, "No 'arg%u' argument.", i);
			return;
		}
		sc = g_mirror_find_device(mp, name);
		if (sc == NULL) {
			gctl_error(req, "No such device: %s.", name);
			return;
		}
		g_cancel_event(sc);
		error = g_mirror_destroy(sc, how);
		if (error != 0) {
			gctl_error(req, "Cannot destroy device %s (error=%d).",
			    sc->sc_geom->name, error);
			sx_xunlock(&sc->sc_lock);
			return;
		}
		/* No need to unlock, because lock is already dead. */
	}
}

void
g_mirror_config(struct gctl_req *req, struct g_class *mp, const char *verb)
{
	uint32_t *version;

	g_topology_assert();

	version = gctl_get_paraml(req, "version", sizeof(*version));
	if (version == NULL) {
		gctl_error(req, "No '%s' argument.", "version");
		return;
	}
	if (*version != G_MIRROR_VERSION) {
		gctl_error(req, "Userland and kernel parts are out of sync.");
		return;
	}

	g_topology_unlock();
	if (strcmp(verb, "configure") == 0)
		g_mirror_ctl_configure(req, mp);
	else if (strcmp(verb, "rebuild") == 0)
		g_mirror_ctl_rebuild(req, mp);
	else if (strcmp(verb, "insert") == 0)
		g_mirror_ctl_insert(req, mp);
	else if (strcmp(verb, "remove") == 0)
		g_mirror_ctl_remove(req, mp);
	else if (strcmp(verb, "deactivate") == 0)
		g_mirror_ctl_deactivate(req, mp);
	else if (strcmp(verb, "forget") == 0)
		g_mirror_ctl_forget(req, mp);
	else if (strcmp(verb, "stop") == 0)
		g_mirror_ctl_stop(req, mp);
	else
		gctl_error(req, "Unknown verb.");
	g_topology_lock();
}
