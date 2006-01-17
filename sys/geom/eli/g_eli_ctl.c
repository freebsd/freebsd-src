/*-
 * Copyright (c) 2005 Pawel Jakub Dawidek <pjd@FreeBSD.org>
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
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/bio.h>
#include <sys/sysctl.h>
#include <sys/malloc.h>
#include <sys/kthread.h>
#include <sys/proc.h>
#include <sys/sched.h>
#include <sys/uio.h>

#include <vm/uma.h>

#include <geom/geom.h>
#include <geom/eli/g_eli.h>


MALLOC_DECLARE(M_ELI);


static void
g_eli_ctl_attach(struct gctl_req *req, struct g_class *mp)
{
	struct g_eli_metadata md;
	struct g_provider *pp;
	const char *name;
	u_char *key, mkey[G_ELI_DATAIVKEYLEN];
	int *nargs, *detach;
	int keysize, error;
	u_int nkey;

	g_topology_assert();

	nargs = gctl_get_paraml(req, "nargs", sizeof(*nargs));
	if (nargs == NULL) {
		gctl_error(req, "No '%s' argument.", "nargs");
		return;
	}
	if (*nargs != 1) {
		gctl_error(req, "Invalid number of arguments.");
		return;
	}

	detach = gctl_get_paraml(req, "detach", sizeof(*detach));
	if (detach == NULL) {
		gctl_error(req, "No '%s' argument.", "detach");
		return;
	}

	name = gctl_get_asciiparam(req, "arg0");
	if (name == NULL) {
		gctl_error(req, "No 'arg%u' argument.", 0);
		return;
	}
	if (strncmp(name, "/dev/", strlen("/dev/")) == 0)
		name += strlen("/dev/");
	pp = g_provider_by_name(name);
	if (pp == NULL) {
		gctl_error(req, "Provider %s is invalid.", name);
		return;
	}
	error = g_eli_read_metadata(mp, pp, &md);
	if (error != 0) {
		gctl_error(req, "Cannot read metadata from %s (error=%d).",
		    name, error);
		return;
	}
	if (md.md_keys == 0x00) {
		bzero(&md, sizeof(md));
		gctl_error(req, "No valid keys on %s.", pp->name);
		return;
	}

	key = gctl_get_param(req, "key", &keysize);
	if (key == NULL || keysize != G_ELI_USERKEYLEN) {
		bzero(&md, sizeof(md));
		gctl_error(req, "No '%s' argument.", "key");
		return;
	}

	error = g_eli_mkey_decrypt(&md, key, mkey, &nkey);
	bzero(key, keysize);
	if (error == -1) {
		bzero(&md, sizeof(md));
		gctl_error(req, "Wrong key for %s.", pp->name);
		return;
	} else if (error > 0) {
		bzero(&md, sizeof(md));
		gctl_error(req, "Cannot decrypt Master Key for %s (error=%d).",
		    pp->name, error);
		return;
	}
	G_ELI_DEBUG(1, "Using Master Key %u for %s.", nkey, pp->name);

	if (*detach)
		md.md_flags |= G_ELI_FLAG_WO_DETACH;
	g_eli_create(req, mp, pp, &md, mkey, nkey);
	bzero(mkey, sizeof(mkey));
	bzero(&md, sizeof(md));
}

static struct g_eli_softc *
g_eli_find_device(struct g_class *mp, const char *prov)
{
	struct g_eli_softc *sc;
	struct g_geom *gp;
	struct g_provider *pp;
	struct g_consumer *cp;

	if (strncmp(prov, "/dev/", strlen("/dev/")) == 0)
		prov += strlen("/dev/");
	LIST_FOREACH(gp, &mp->geom, geom) {
		sc = gp->softc;
		if (sc == NULL)
			continue;
		pp = LIST_FIRST(&gp->provider);
		if (pp != NULL && strcmp(pp->name, prov) == 0)
			return (sc);
		cp = LIST_FIRST(&gp->consumer);
		if (cp != NULL && cp->provider != NULL &&
		    strcmp(cp->provider->name, prov) == 0) {
			return (sc);
		}
	}
	return (NULL);
}

static void
g_eli_ctl_detach(struct gctl_req *req, struct g_class *mp)
{
	struct g_eli_softc *sc;
	int *force, *last, *nargs, error;
	const char *prov;
	char param[16];
	u_int i;

	g_topology_assert();

	nargs = gctl_get_paraml(req, "nargs", sizeof(*nargs));
	if (nargs == NULL) {
		gctl_error(req, "No '%s' argument.", "nargs");
		return;
	}
	if (*nargs <= 0) {
		gctl_error(req, "Missing device(s).");
		return;
	}
	force = gctl_get_paraml(req, "force", sizeof(*force));
	if (force == NULL) {
		gctl_error(req, "No '%s' argument.", "force");
		return;
	}
	last = gctl_get_paraml(req, "last", sizeof(*last));
	if (last == NULL) {
		gctl_error(req, "No '%s' argument.", "last");
		return;
	}

	for (i = 0; i < (u_int)*nargs; i++) {
		snprintf(param, sizeof(param), "arg%u", i);
		prov = gctl_get_asciiparam(req, param);
		if (prov == NULL) {
			gctl_error(req, "No 'arg%u' argument.", i);
			return;
		}
		sc = g_eli_find_device(mp, prov);
		if (sc == NULL) {
			gctl_error(req, "No such device: %s.", prov);
			return;
		}
		if (*last) {
			sc->sc_flags |= G_ELI_FLAG_RW_DETACH;
			sc->sc_geom->access = g_eli_access;
		} else {
			error = g_eli_destroy(sc, *force);
			if (error != 0) {
				gctl_error(req,
				    "Cannot destroy device %s (error=%d).",
				    sc->sc_name, error);
				return;
			}
		}
	}
}

static void
g_eli_ctl_onetime(struct gctl_req *req, struct g_class *mp)
{
	struct g_eli_metadata md;
	struct g_provider *pp;
	const char *name;
	intmax_t *keylen, *sectorsize;
	u_char mkey[G_ELI_DATAIVKEYLEN];
	int *nargs, *detach;

	g_topology_assert();
	bzero(&md, sizeof(md));

	nargs = gctl_get_paraml(req, "nargs", sizeof(*nargs));
	if (nargs == NULL) {
		gctl_error(req, "No '%s' argument.", "nargs");
		return;
	}
	if (*nargs != 1) {
		gctl_error(req, "Invalid number of arguments.");
		return;
	}

	detach = gctl_get_paraml(req, "detach", sizeof(*detach));
	if (detach == NULL) {
		gctl_error(req, "No '%s' argument.", "detach");
		return;
	}

	strlcpy(md.md_magic, G_ELI_MAGIC, sizeof(md.md_magic));
	md.md_version = G_ELI_VERSION;
	md.md_flags |= G_ELI_FLAG_ONETIME;
	if (*detach)
		md.md_flags |= G_ELI_FLAG_WO_DETACH;

	name = gctl_get_asciiparam(req, "algo");
	if (name == NULL) {
		gctl_error(req, "No '%s' argument.", "algo");
		return;
	}
	md.md_algo = g_eli_str2algo(name);
	if (md.md_algo < CRYPTO_ALGORITHM_MIN ||
	    md.md_algo > CRYPTO_ALGORITHM_MAX) {
		gctl_error(req, "Invalid '%s' argument.", "algo");
		return;
	}

	keylen = gctl_get_paraml(req, "keylen", sizeof(*keylen));
	if (keylen == NULL) {
		gctl_error(req, "No '%s' argument.", "keylen");
		return;
	}
	md.md_keylen = g_eli_keylen(md.md_algo, *keylen);
	if (md.md_keylen == 0) {
		gctl_error(req, "Invalid '%s' argument.", "keylen");
		return;
	}

	/* Not important here. */
	md.md_provsize = 0;
	/* Not important here. */
	bzero(md.md_salt, sizeof(md.md_salt));

	md.md_keys = 0x01;
	arc4rand(mkey, sizeof(mkey), 0);

	/* Not important here. */
	bzero(md.md_hash, sizeof(md.md_hash));

	name = gctl_get_asciiparam(req, "arg0");
	if (name == NULL) {
		gctl_error(req, "No 'arg%u' argument.", 0);
		return;
	}
	if (strncmp(name, "/dev/", strlen("/dev/")) == 0)
		name += strlen("/dev/");
	pp = g_provider_by_name(name);
	if (pp == NULL) {
		gctl_error(req, "Provider %s is invalid.", name);
		return;
	}

	sectorsize = gctl_get_paraml(req, "sectorsize", sizeof(*sectorsize));
	if (sectorsize == NULL) {
		gctl_error(req, "No '%s' argument.", "sectorsize");
		return;
	}
	if (*sectorsize == 0)
		md.md_sectorsize = pp->sectorsize;
	else {
		if (*sectorsize < 0 || (*sectorsize % pp->sectorsize) != 0) {
			gctl_error(req, "Invalid sector size.");
			return;
		}
		md.md_sectorsize = *sectorsize;
	}

	g_eli_create(req, mp, pp, &md, mkey, -1);
	bzero(mkey, sizeof(mkey));
	bzero(&md, sizeof(md));
}

static void
g_eli_ctl_setkey(struct gctl_req *req, struct g_class *mp)
{
	struct g_eli_softc *sc;
	struct g_eli_metadata md;
	struct g_provider *pp;
	struct g_consumer *cp;
	const char *name;
	u_char *key, *mkeydst, *sector;
	intmax_t *valp;
	int keysize, nkey, error;

	g_topology_assert();

	name = gctl_get_asciiparam(req, "arg0");
	if (name == NULL) {
		gctl_error(req, "No 'arg%u' argument.", 0);
		return;
	}
	sc = g_eli_find_device(mp, name);
	if (sc == NULL) {
		gctl_error(req, "Provider %s is invalid.", name);
		return;
	}
	cp = LIST_FIRST(&sc->sc_geom->consumer);
	pp = cp->provider;

	error = g_eli_read_metadata(mp, pp, &md);
	if (error != 0) {
		gctl_error(req, "Cannot read metadata from %s (error=%d).",
		    name, error);
		return;
	}

	valp = gctl_get_paraml(req, "keyno", sizeof(*valp));
	if (valp == NULL) {
		gctl_error(req, "No '%s' argument.", "keyno");
		return;
	}
	if (*valp != -1)
		nkey = *valp;
	else
		nkey = sc->sc_nkey;
	if (nkey < 0 || nkey >= G_ELI_MAXMKEYS) {
		gctl_error(req, "Invalid '%s' argument.", "keyno");
		return;
	}

	valp = gctl_get_paraml(req, "iterations", sizeof(*valp));
	if (valp == NULL) {
		gctl_error(req, "No '%s' argument.", "iterations");
		return;
	}
	/* Check if iterations number should and can be changed. */
	if (*valp != -1) {
		if (bitcount32(md.md_keys) != 1) {
			gctl_error(req, "To be able to use '-i' option, only "
			    "one key can be defined.");
			return;
		}
		if (md.md_keys != (1 << nkey)) {
			gctl_error(req, "Only already defined key can be "
			    "changed when '-i' option is used.");
			return;
		}
		md.md_iterations = *valp;
	}

	key = gctl_get_param(req, "key", &keysize);
	if (key == NULL || keysize != G_ELI_USERKEYLEN) {
		bzero(&md, sizeof(md));
		gctl_error(req, "No '%s' argument.", "key");
		return;
	}

	mkeydst = md.md_mkeys + nkey * G_ELI_MKEYLEN;
	md.md_keys |= (1 << nkey);

	bcopy(sc->sc_ivkey, mkeydst, sizeof(sc->sc_ivkey));
	bcopy(sc->sc_datakey, mkeydst + sizeof(sc->sc_ivkey),
	    sizeof(sc->sc_datakey));

	/* Encrypt Master Key with the new key. */
	error = g_eli_mkey_encrypt(md.md_algo, key, md.md_keylen, mkeydst);
	bzero(key, sizeof(key));
	if (error != 0) {
		bzero(&md, sizeof(md));
		gctl_error(req, "Cannot encrypt Master Key (error=%d).", error);
		return;
	}

	sector = malloc(pp->sectorsize, M_ELI, M_WAITOK | M_ZERO);
	/* Store metadata with fresh key. */
	eli_metadata_encode(&md, sector);
	bzero(&md, sizeof(md));
	error = g_write_data(cp, pp->mediasize - pp->sectorsize, sector,
	    pp->sectorsize);
	bzero(sector, sizeof(sector));
	free(sector, M_ELI);
	if (error != 0) {
		gctl_error(req, "Cannot store metadata on %s (error=%d).",
		    pp->name, error);
		return;
	}
	G_ELI_DEBUG(1, "Key %u changed on %s.", nkey, pp->name);
}

static void
g_eli_ctl_delkey(struct gctl_req *req, struct g_class *mp)
{
	struct g_eli_softc *sc;
	struct g_eli_metadata md;
	struct g_provider *pp;
	struct g_consumer *cp;
	const char *name;
	u_char *mkeydst, *sector;
	intmax_t *valp;
	size_t keysize;
	int error, nkey, *all, *force;
	u_int i;

	g_topology_assert();

	nkey = 0;	/* fixes causeless gcc warning */

	name = gctl_get_asciiparam(req, "arg0");
	if (name == NULL) {
		gctl_error(req, "No 'arg%u' argument.", 0);
		return;
	}
	sc = g_eli_find_device(mp, name);
	if (sc == NULL) {
		gctl_error(req, "Provider %s is invalid.", name);
		return;
	}
	cp = LIST_FIRST(&sc->sc_geom->consumer);
	pp = cp->provider;

	error = g_eli_read_metadata(mp, pp, &md);
	if (error != 0) {
		gctl_error(req, "Cannot read metadata from %s (error=%d).",
		    name, error);
		return;
	}

	all = gctl_get_paraml(req, "all", sizeof(*all));
	if (all == NULL) {
		gctl_error(req, "No '%s' argument.", "all");
		return;
	}

	if (*all) {
		mkeydst = md.md_mkeys;
		keysize = sizeof(md.md_mkeys);
	} else {
		force = gctl_get_paraml(req, "force", sizeof(*force));
		if (force == NULL) {
			gctl_error(req, "No '%s' argument.", "force");
			return;
		}
	
		valp = gctl_get_paraml(req, "keyno", sizeof(*valp));
		if (valp == NULL) {
			gctl_error(req, "No '%s' argument.", "keyno");
			return;
		}
		if (*valp != -1)
			nkey = *valp;
		else
			nkey = sc->sc_nkey;
		if (nkey < 0 || nkey >= G_ELI_MAXMKEYS) {
			gctl_error(req, "Invalid '%s' argument.", "keyno");
			return;
		}
		if (!(md.md_keys & (1 << nkey)) && !*force) {
			gctl_error(req, "Master Key %u is not set.", nkey);
			return;
		}
		md.md_keys &= ~(1 << nkey);
		if (md.md_keys == 0 && !*force) {
			gctl_error(req, "This is the last Master Key. Use '-f' "
			    "flag if you really want to remove it.");
			return;
		}
		mkeydst = md.md_mkeys + nkey * G_ELI_MKEYLEN;
		keysize = G_ELI_MKEYLEN;
	}

	sector = malloc(pp->sectorsize, M_ELI, M_WAITOK | M_ZERO);
	for (i = 0; i <= g_eli_overwrites; i++) {
		if (i == g_eli_overwrites)
			bzero(mkeydst, keysize);
		else
			arc4rand(mkeydst, keysize, 0);
		/* Store metadata with destroyed key. */
		eli_metadata_encode(&md, sector);
		error = g_write_data(cp, pp->mediasize - pp->sectorsize, sector,
		    pp->sectorsize);
		if (error != 0) {
			G_ELI_DEBUG(0, "Cannot store metadata on %s "
			    "(error=%d).", pp->name, error);
		}
	}
	bzero(&md, sizeof(md));
	bzero(sector, sizeof(sector));
	free(sector, M_ELI);
	if (*all)
		G_ELI_DEBUG(1, "All keys removed from %s.", pp->name);
	else
		G_ELI_DEBUG(1, "Key %d removed from %s.", nkey, pp->name);
}

static int
g_eli_kill_one(struct g_eli_softc *sc)
{
	struct g_provider *pp;
	struct g_consumer *cp;
	u_char *sector;
	int err, error = 0;
	u_int i;

	g_topology_assert();

	if (sc == NULL)
		return (ENOENT);

	pp = LIST_FIRST(&sc->sc_geom->provider);
	g_error_provider(pp, ENXIO);

	cp = LIST_FIRST(&sc->sc_geom->consumer);
	pp = cp->provider;

	sector = malloc(pp->sectorsize, M_ELI, M_WAITOK);
	for (i = 0; i <= g_eli_overwrites; i++) {
		if (i == g_eli_overwrites)
			bzero(sector, pp->sectorsize);
		else
			arc4rand(sector, pp->sectorsize, 0);
		err = g_write_data(cp, pp->mediasize - pp->sectorsize, sector,
		    pp->sectorsize);
		if (err != 0) {
			G_ELI_DEBUG(0, "Cannot erase metadata on %s "
			    "(error=%d).", pp->name, err);
			if (error == 0)
				error = err;
		}
	}
	free(sector, M_ELI);
	if (error == 0)
		G_ELI_DEBUG(0, "%s has been killed.", pp->name);
	g_eli_destroy(sc, 1);
	return (error);
}

static void
g_eli_ctl_kill(struct gctl_req *req, struct g_class *mp)
{
	int *all, *nargs;
	int error;

	g_topology_assert();

	nargs = gctl_get_paraml(req, "nargs", sizeof(*nargs));
	if (nargs == NULL) {
		gctl_error(req, "No '%s' argument.", "nargs");
		return;
	}
	all = gctl_get_paraml(req, "all", sizeof(*all));
	if (all == NULL) {
		gctl_error(req, "No '%s' argument.", "all");
		return;
	}
	if (!*all && *nargs == 0) {
		gctl_error(req, "Too few arguments.");
		return;
	}

	if (*all) {
		struct g_geom *gp, *gp2;

		LIST_FOREACH_SAFE(gp, &mp->geom, geom, gp2) {
			error = g_eli_kill_one(gp->softc);
			if (error != 0)
				gctl_error(req, "Not fully done.");
		}
	} else {
		struct g_eli_softc *sc;
		const char *prov;
		char param[16];
		int i;

		for (i = 0; i < *nargs; i++) {
			snprintf(param, sizeof(param), "arg%u", i);
			prov = gctl_get_asciiparam(req, param);
			if (prov == NULL) {
				G_ELI_DEBUG(0, "No 'arg%d' argument.", i);
				continue;
			}

			sc = g_eli_find_device(mp, prov);
			if (sc == NULL) {
				G_ELI_DEBUG(1, "No such provider: %s.", prov);
				continue;
			}
			error = g_eli_kill_one(sc);
			if (error != 0)
				gctl_error(req, "Not fully done.");
		}
	}
}

void
g_eli_config(struct gctl_req *req, struct g_class *mp, const char *verb)
{
	uint32_t *version;

	g_topology_assert();

	version = gctl_get_paraml(req, "version", sizeof(*version));
	if (version == NULL) {
		gctl_error(req, "No '%s' argument.", "version");
		return;
	}
	if (*version != G_ELI_VERSION) {
		gctl_error(req, "Userland and kernel parts are out of sync.");
		return;
	}

	if (strcmp(verb, "attach") == 0)
		g_eli_ctl_attach(req, mp);
	else if (strcmp(verb, "detach") == 0 || strcmp(verb, "stop") == 0)
		g_eli_ctl_detach(req, mp);
	else if (strcmp(verb, "onetime") == 0)
		g_eli_ctl_onetime(req, mp);
	else if (strcmp(verb, "setkey") == 0)
		g_eli_ctl_setkey(req, mp);
	else if (strcmp(verb, "delkey") == 0)
		g_eli_ctl_delkey(req, mp);
	else if (strcmp(verb, "kill") == 0)
		g_eli_ctl_kill(req, mp);
	else
		gctl_error(req, "Unknown verb.");
}
