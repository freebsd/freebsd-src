/*-
 * Copyright (c) 2004 Lukas Ertl
 * Copyright (c) 1997, 1998, 1999
 *      Nan Yang Computer Services Limited.  All rights reserved.
 *
 *  Parts written by Greg Lehey
 *
 *  This software is distributed under the so-called ``Berkeley
 *  License'':
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Nan Yang Computer
 *      Services Limited.
 * 4. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * This software is provided ``as is'', and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall the company or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even if
 * advised of the possibility of such damage.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/geom/vinum/geom_vinum_subr.c,v 1.16.6.1 2008/11/25 02:59:29 kensmith Exp $");

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/libkern.h>
#include <sys/malloc.h>
#include <sys/systm.h>

#include <geom/geom.h>
#include <geom/geom_int.h>
#include <geom/vinum/geom_vinum_var.h>
#include <geom/vinum/geom_vinum.h>
#include <geom/vinum/geom_vinum_share.h>

static off_t gv_plex_smallest_sd(struct gv_plex *, off_t);

/* Find the VINUM class and it's associated geom. */
struct g_geom *
find_vinum_geom(void)
{
	struct g_class *mp;
	struct g_geom *gp;

	g_topology_assert();

	gp = NULL;

	LIST_FOREACH(mp, &g_classes, class) {
		if (!strcmp(mp->name, "VINUM")) {
			gp = LIST_FIRST(&mp->geom);
			break;
		}
	}

	return (gp);
}

/*
 * Parse the vinum config provided in *buf and store it in *gp's softc.
 * If parameter 'merge' is non-zero, then the given config is merged into
 * *gp.
 */
void
gv_parse_config(struct gv_softc *sc, u_char *buf, int merge)
{
	char *aptr, *bptr, *cptr;
	struct gv_volume *v, *v2;
	struct gv_plex *p, *p2;
	struct gv_sd *s, *s2;
	int tokens;
	char *token[GV_MAXARGS];

	g_topology_assert();

	KASSERT(sc != NULL, ("gv_parse_config: NULL softc"));

	/* Until the end of the string *buf. */
	for (aptr = buf; *aptr != '\0'; aptr = bptr) {
		bptr = aptr;
		cptr = aptr;

		/* Seperate input lines. */
		while (*bptr != '\n')
			bptr++;
		*bptr = '\0';
		bptr++;

		tokens = gv_tokenize(cptr, token, GV_MAXARGS);

		if (tokens > 0) {
			if (!strcmp(token[0], "volume")) {
				v = gv_new_volume(tokens, token);
				if (v == NULL) {
					printf("geom_vinum: failed volume\n");
					break;
				}

				if (merge) {
					v2 = gv_find_vol(sc, v->name);
					if (v2 != NULL) {
						g_free(v);
						continue;
					}
				}

				v->vinumconf = sc;
				LIST_INIT(&v->plexes);
				LIST_INSERT_HEAD(&sc->volumes, v, volume);

			} else if (!strcmp(token[0], "plex")) {
				p = gv_new_plex(tokens, token);
				if (p == NULL) {
					printf("geom_vinum: failed plex\n");
					break;
				}

				if (merge) {
					p2 = gv_find_plex(sc, p->name);
					if (p2 != NULL) {
						g_free(p);
						continue;
					}
				}

				p->vinumconf = sc;
				LIST_INIT(&p->subdisks);
				LIST_INSERT_HEAD(&sc->plexes, p, plex);

			} else if (!strcmp(token[0], "sd")) {
				s = gv_new_sd(tokens, token);

				if (s == NULL) {
					printf("geom_vinum: failed subdisk\n");
					break;
				}

				if (merge) {
					s2 = gv_find_sd(sc, s->name);
					if (s2 != NULL) {
						g_free(s);
						continue;
					}
				}

				s->vinumconf = sc;
				LIST_INSERT_HEAD(&sc->subdisks, s, sd);
			}
		}
	}
}

/*
 * Format the vinum configuration properly.  If ondisk is non-zero then the
 * configuration is intended to be written to disk later.
 */
void
gv_format_config(struct gv_softc *sc, struct sbuf *sb, int ondisk, char *prefix)
{
	struct gv_drive *d;
	struct gv_sd *s;
	struct gv_plex *p;
	struct gv_volume *v;

	g_topology_assert();

	/*
	 * We don't need the drive configuration if we're not writing the
	 * config to disk.
	 */
	if (!ondisk) {
		LIST_FOREACH(d, &sc->drives, drive) {
			sbuf_printf(sb, "%sdrive %s device /dev/%s\n", prefix,
			    d->name, d->device);
		}
	}

	LIST_FOREACH(v, &sc->volumes, volume) {
		if (!ondisk)
			sbuf_printf(sb, "%s", prefix);
		sbuf_printf(sb, "volume %s", v->name);
		if (ondisk)
			sbuf_printf(sb, " state %s", gv_volstate(v->state));
		sbuf_printf(sb, "\n");
	}

	LIST_FOREACH(p, &sc->plexes, plex) {
		if (!ondisk)
			sbuf_printf(sb, "%s", prefix);
		sbuf_printf(sb, "plex name %s org %s ", p->name,
		    gv_plexorg(p->org));
		if (gv_is_striped(p))
			sbuf_printf(sb, "%ds ", p->stripesize / 512);
		if (p->vol_sc != NULL)
			sbuf_printf(sb, "vol %s", p->volume);
		if (ondisk)
			sbuf_printf(sb, " state %s", gv_plexstate(p->state));
		sbuf_printf(sb, "\n");
	}

	LIST_FOREACH(s, &sc->subdisks, sd) {
		if (!ondisk)
			sbuf_printf(sb, "%s", prefix);
		sbuf_printf(sb, "sd name %s drive %s len %jds driveoffset "
		    "%jds", s->name, s->drive, s->size / 512,
		    s->drive_offset / 512);
		if (s->plex_sc != NULL) {
			sbuf_printf(sb, " plex %s plexoffset %jds", s->plex,
			    s->plex_offset / 512);
		}
		if (ondisk)
			sbuf_printf(sb, " state %s", gv_sdstate(s->state));
		sbuf_printf(sb, "\n");
	}

	return;
}

static off_t
gv_plex_smallest_sd(struct gv_plex *p, off_t smallest)
{
	struct gv_sd *s;

	KASSERT(p != NULL, ("gv_plex_smallest_sd: NULL p"));

	LIST_FOREACH(s, &p->subdisks, in_plex) {
		if (s->size < smallest)
			smallest = s->size;
	}
	return (smallest);
}

int
gv_sd_to_plex(struct gv_plex *p, struct gv_sd *s, int check)
{
	struct gv_sd *s2;

	g_topology_assert();

	/* If this subdisk was already given to this plex, do nothing. */
	if (s->plex_sc == p)
		return (0);

	/* Check correct size of this subdisk. */
	s2 = LIST_FIRST(&p->subdisks);
	if (s2 != NULL && gv_is_striped(p) && (s2->size != s->size)) {
		printf("GEOM_VINUM: need equal sized subdisks for "
		    "this plex organisation - %s (%jd) <-> %s (%jd)\n",
		    s2->name, s2->size, s->name, s->size);
		return (-1);
	}

	/* Find the correct plex offset for this subdisk, if needed. */
	if (s->plex_offset == -1) {
		if (p->sdcount) {
			LIST_FOREACH(s2, &p->subdisks, in_plex) {
				if (gv_is_striped(p))
					s->plex_offset = p->sdcount *
					    p->stripesize;
				else
					s->plex_offset = s2->plex_offset +
					    s2->size;
			}
		} else
			s->plex_offset = 0;
	}

	p->sdcount++;

	/* Adjust the size of our plex. */
	switch (p->org) {
	case GV_PLEX_CONCAT:
	case GV_PLEX_STRIPED:
		p->size += s->size;
		break;

	case GV_PLEX_RAID5:
		p->size = (p->sdcount - 1) * gv_plex_smallest_sd(p, s->size);
		break;

	default:
		break;
	}

	/* There are no subdisks for this plex yet, just insert it. */
	if (LIST_EMPTY(&p->subdisks)) {
		LIST_INSERT_HEAD(&p->subdisks, s, in_plex);

	/* Insert in correct order, depending on plex_offset. */
	} else {
		LIST_FOREACH(s2, &p->subdisks, in_plex) {
			if (s->plex_offset < s2->plex_offset) {
				LIST_INSERT_BEFORE(s2, s, in_plex);
				break;
			} else if (LIST_NEXT(s2, in_plex) == NULL) {
				LIST_INSERT_AFTER(s2, s, in_plex);
				break;
			}
		}
	}

	s->plex_sc = p;

	return (0);
}

void
gv_update_vol_size(struct gv_volume *v, off_t size)
{
	struct g_geom *gp;
	struct g_provider *pp;

	if (v == NULL)
		return;

	gp = v->geom;
	if (gp == NULL)
		return;

	LIST_FOREACH(pp, &gp->provider, provider) {
		pp->mediasize = size;
	}

	v->size = size;
}

/* Calculates the plex size. */
off_t
gv_plex_size(struct gv_plex *p)
{
	struct gv_sd *s;
	off_t size;

	KASSERT(p != NULL, ("gv_plex_size: NULL p"));

	if (p->sdcount == 0)
		return (0);

	/* Adjust the size of our plex. */
	size = 0;
	switch (p->org) {
	case GV_PLEX_CONCAT:
		LIST_FOREACH(s, &p->subdisks, in_plex)
			size += s->size;
		break;
	case GV_PLEX_STRIPED:
		s = LIST_FIRST(&p->subdisks);
		size = p->sdcount * s->size;
		break;
	case GV_PLEX_RAID5:
		s = LIST_FIRST(&p->subdisks);
		size = (p->sdcount - 1) * s->size;
		break;
	}

	return (size);
}

/* Returns the size of a volume. */
off_t
gv_vol_size(struct gv_volume *v)
{
	struct gv_plex *p;
	off_t minplexsize;

	KASSERT(v != NULL, ("gv_vol_size: NULL v"));

	p = LIST_FIRST(&v->plexes);
	if (p == NULL)
		return (0);

	minplexsize = p->size;
	LIST_FOREACH(p, &v->plexes, plex) {
		if (p->size < minplexsize) {
			minplexsize = p->size;
		}
	}
	return (minplexsize);
}

void
gv_update_plex_config(struct gv_plex *p)
{
	struct gv_sd *s, *s2;
	off_t remainder;
	int required_sds, state;

	KASSERT(p != NULL, ("gv_update_plex_config: NULL p"));

	/* This is what we want the plex to be. */
	state = GV_PLEX_UP;

	/* The plex was added to an already running volume. */
	if (p->flags & GV_PLEX_ADDED)
		state = GV_PLEX_DOWN;

	switch (p->org) {
	case GV_PLEX_STRIPED:
		required_sds = 2;
		break;
	case GV_PLEX_RAID5:
		required_sds = 3;
		break;
	case GV_PLEX_CONCAT:
	default:
		required_sds = 0;
		break;
	}

	if (required_sds) {
		if (p->sdcount < required_sds) {
			state = GV_PLEX_DOWN;
		}

		/*
		 * The subdisks in striped plexes must all have the same size.
		 */
		s = LIST_FIRST(&p->subdisks);
		LIST_FOREACH(s2, &p->subdisks, in_plex) {
			if (s->size != s2->size) {
				printf("geom_vinum: subdisk size mismatch "
				    "%s (%jd) <> %s (%jd)\n", s->name, s->size,
				    s2->name, s2->size);
				state = GV_PLEX_DOWN;
			}
		}

		/* Trim subdisk sizes so that they match the stripe size. */
		LIST_FOREACH(s, &p->subdisks, in_plex) {
			remainder = s->size % p->stripesize;
			if (remainder) {
				printf("gvinum: size of sd %s is not a "
				    "multiple of plex stripesize, taking off "
				    "%jd bytes\n", s->name,
				    (intmax_t)remainder);
				gv_adjust_freespace(s, remainder);
			}
		}
	}

	/* Adjust the size of our plex. */
	if (p->sdcount > 0) {
		p->size = 0;
		switch (p->org) {
		case GV_PLEX_CONCAT:
			LIST_FOREACH(s, &p->subdisks, in_plex)
				p->size += s->size;
			break;

		case GV_PLEX_STRIPED:
			s = LIST_FIRST(&p->subdisks);
			p->size = p->sdcount * s->size;
			break;
		
		case GV_PLEX_RAID5:
			s = LIST_FIRST(&p->subdisks);
			p->size = (p->sdcount - 1) * s->size;
			break;
		
		default:
			break;
		}
	}

	if (p->sdcount == 0)
		state = GV_PLEX_DOWN;
	else if ((p->flags & GV_PLEX_ADDED) ||
	    ((p->org == GV_PLEX_RAID5) && (p->flags & GV_PLEX_NEWBORN))) {
		LIST_FOREACH(s, &p->subdisks, in_plex)
			s->state = GV_SD_STALE;
		p->flags &= ~GV_PLEX_ADDED;
		p->flags &= ~GV_PLEX_NEWBORN;
		p->state = GV_PLEX_DOWN;
	}
}

/*
 * Give a subdisk to a drive, check and adjust several parameters, adjust
 * freelist.
 */
int
gv_sd_to_drive(struct gv_softc *sc, struct gv_drive *d, struct gv_sd *s,
    char *errstr, int errlen)
{
	struct gv_sd *s2;
	struct gv_freelist *fl, *fl2;
	off_t tmp;
	int i;

	g_topology_assert();

	fl2 = NULL;

	KASSERT(sc != NULL, ("gv_sd_to_drive: NULL softc"));
	KASSERT(d != NULL, ("gv_sd_to_drive: NULL drive"));
	KASSERT(s != NULL, ("gv_sd_to_drive: NULL subdisk"));
	KASSERT(errstr != NULL, ("gv_sd_to_drive: NULL errstr"));
	KASSERT(errlen >= ERRBUFSIZ, ("gv_sd_to_drive: short errlen (%d)",
	    errlen));

	/* Check if this subdisk was already given to this drive. */
	if (s->drive_sc == d)
		return (0);

	/* Preliminary checks. */
	if (s->size > d->avail || d->freelist_entries == 0) {
		snprintf(errstr, errlen, "not enough space on '%s' for '%s'",
		    d->name, s->name);
		return (-1);
	}

	/* No size given, autosize it. */
	if (s->size == -1) {
		/* Find the largest available slot. */
		LIST_FOREACH(fl, &d->freelist, freelist) {
			if (fl->size >= s->size) {
				s->size = fl->size;
				s->drive_offset = fl->offset;
				fl2 = fl;
			}
		}

		/* No good slot found? */
		if (s->size == -1) {
			snprintf(errstr, errlen, "couldn't autosize '%s' on "
			    "'%s'", s->name, d->name);
			return (-1);
		}

	/*
	 * Check if we have a free slot that's large enough for the given size.
	 */
	} else {
		i = 0;
		LIST_FOREACH(fl, &d->freelist, freelist) {
			/* Yes, this subdisk fits. */
			if (fl->size >= s->size) {
				i++;
				/* Assign drive offset, if not given. */
				if (s->drive_offset == -1)
					s->drive_offset = fl->offset;
				fl2 = fl;
				break;
			}
		}

		/* Couldn't find a good free slot. */
		if (i == 0) {
			snprintf(errstr, errlen, "free slots to small for '%s' "
			    "on '%s'", s->name, d->name);
			return (-1);
		}
	}

	/* No drive offset given, try to calculate it. */
	if (s->drive_offset == -1) {

		/* Add offsets and sizes from other subdisks on this drive. */
		LIST_FOREACH(s2, &d->subdisks, from_drive) {
			s->drive_offset = s2->drive_offset + s2->size;
		}

		/*
		 * If there are no other subdisks yet, then set the default
		 * offset to GV_DATA_START.
		 */
		if (s->drive_offset == -1)
			s->drive_offset = GV_DATA_START;

	/* Check if we have a free slot at the given drive offset. */
	} else {
		i = 0;
		LIST_FOREACH(fl, &d->freelist, freelist) {
			/* Yes, this subdisk fits. */
			if ((fl->offset <= s->drive_offset) &&
			    (fl->offset + fl->size >=
			    s->drive_offset + s->size)) {
				i++;
				fl2 = fl;
				break;
			}
		}

		/* Couldn't find a good free slot. */
		if (i == 0) {
			snprintf(errstr, errlen, "given drive_offset for '%s' "
			    "won't fit on '%s'", s->name, d->name);
			return (-1);
		}
	}

	/*
	 * Now that all parameters are checked and set up, we can give the
	 * subdisk to the drive and adjust the freelist.
	 */

	/* First, adjust the freelist. */
	LIST_FOREACH(fl, &d->freelist, freelist) {

		/* This is the free slot that we have found before. */
		if (fl == fl2) {
	
			/*
			 * The subdisk starts at the beginning of the free
			 * slot.
			 */
			if (fl->offset == s->drive_offset) {
				fl->offset += s->size;
				fl->size -= s->size;

				/*
				 * The subdisk uses the whole slot, so remove
				 * it.
				 */
				if (fl->size == 0) {
					d->freelist_entries--;
					LIST_REMOVE(fl, freelist);
				}
			/*
			 * The subdisk does not start at the beginning of the
			 * free slot.
			 */
			} else {
				tmp = fl->offset + fl->size;
				fl->size = s->drive_offset - fl->offset;

				/*
				 * The subdisk didn't use the complete rest of
				 * the free slot, so we need to split it.
				 */
				if (s->drive_offset + s->size != tmp) {
					fl2 = g_malloc(sizeof(*fl2),
					    M_WAITOK | M_ZERO);
					fl2->offset = s->drive_offset + s->size;
					fl2->size = tmp - fl2->offset;
					LIST_INSERT_AFTER(fl, fl2, freelist);
					d->freelist_entries++;
				}
			}
			break;
		}
	}

	/*
	 * This is the first subdisk on this drive, just insert it into the
	 * list.
	 */
	if (LIST_EMPTY(&d->subdisks)) {
		LIST_INSERT_HEAD(&d->subdisks, s, from_drive);

	/* There are other subdisks, so insert this one in correct order. */
	} else {
		LIST_FOREACH(s2, &d->subdisks, from_drive) {
			if (s->drive_offset < s2->drive_offset) {
				LIST_INSERT_BEFORE(s2, s, from_drive);
				break;
			} else if (LIST_NEXT(s2, from_drive) == NULL) {
				LIST_INSERT_AFTER(s2, s, from_drive);
				break;
			}
		}
	}

	d->sdcount++;
	d->avail -= s->size;

	/* Link back from the subdisk to this drive. */
	s->drive_sc = d;

	return (0);
}

void
gv_free_sd(struct gv_sd *s)
{
	struct gv_drive *d;
	struct gv_freelist *fl, *fl2;

	KASSERT(s != NULL, ("gv_free_sd: NULL s"));

	d = s->drive_sc;
	if (d == NULL)
		return;

	/*
	 * First, find the free slot that's immediately before or after this
	 * subdisk.
	 */
	fl = NULL;
	LIST_FOREACH(fl, &d->freelist, freelist) {
		if (fl->offset == s->drive_offset + s->size)
			break;
		if (fl->offset + fl->size == s->drive_offset)
			break;
	}

	/* If there is no free slot behind this subdisk, so create one. */
	if (fl == NULL) {

		fl = g_malloc(sizeof(*fl), M_WAITOK | M_ZERO);
		fl->size = s->size;
		fl->offset = s->drive_offset;

		if (d->freelist_entries == 0) {
			LIST_INSERT_HEAD(&d->freelist, fl, freelist);
		} else {
			LIST_FOREACH(fl2, &d->freelist, freelist) {
				if (fl->offset < fl2->offset) {
					LIST_INSERT_BEFORE(fl2, fl, freelist);
					break;
				} else if (LIST_NEXT(fl2, freelist) == NULL) {
					LIST_INSERT_AFTER(fl2, fl, freelist);
					break;
				}
			}
		}

		d->freelist_entries++;

	/* Expand the free slot we just found. */
	} else {
		fl->size += s->size;
		if (fl->offset > s->drive_offset)
			fl->offset = s->drive_offset;
	}

	d->avail += s->size;
	d->sdcount--;
}

void
gv_adjust_freespace(struct gv_sd *s, off_t remainder)
{
	struct gv_drive *d;
	struct gv_freelist *fl, *fl2;

	KASSERT(s != NULL, ("gv_adjust_freespace: NULL s"));
	d = s->drive_sc;
	KASSERT(d != NULL, ("gv_adjust_freespace: NULL d"));

	/* First, find the free slot that's immediately after this subdisk. */
	fl = NULL;
	LIST_FOREACH(fl, &d->freelist, freelist) {
		if (fl->offset == s->drive_offset + s->size)
			break;
	}

	/* If there is no free slot behind this subdisk, so create one. */
	if (fl == NULL) {

		fl = g_malloc(sizeof(*fl), M_WAITOK | M_ZERO);
		fl->size = remainder;
		fl->offset = s->drive_offset + s->size - remainder;

		if (d->freelist_entries == 0) {
			LIST_INSERT_HEAD(&d->freelist, fl, freelist);
		} else {
			LIST_FOREACH(fl2, &d->freelist, freelist) {
				if (fl->offset < fl2->offset) {
					LIST_INSERT_BEFORE(fl2, fl, freelist);
					break;
				} else if (LIST_NEXT(fl2, freelist) == NULL) {
					LIST_INSERT_AFTER(fl2, fl, freelist);
					break;
				}
			}
		}

		d->freelist_entries++;

	/* Expand the free slot we just found. */
	} else {
		fl->offset -= remainder;
		fl->size += remainder;
	}

	s->size -= remainder;
	d->avail += remainder;
}

/* Check if the given plex is a striped one. */
int
gv_is_striped(struct gv_plex *p)
{
	KASSERT(p != NULL, ("gv_is_striped: NULL p"));
	switch(p->org) {
	case GV_PLEX_STRIPED:
	case GV_PLEX_RAID5:
		return (1);
	default:
		return (0);
	}
}

/* Find a volume by name. */
struct gv_volume *
gv_find_vol(struct gv_softc *sc, char *name)
{
	struct gv_volume *v;

	LIST_FOREACH(v, &sc->volumes, volume) {
		if (!strncmp(v->name, name, GV_MAXVOLNAME))
			return (v);
	}

	return (NULL);
}

/* Find a plex by name. */
struct gv_plex *
gv_find_plex(struct gv_softc *sc, char *name)
{
	struct gv_plex *p;

	LIST_FOREACH(p, &sc->plexes, plex) {
		if (!strncmp(p->name, name, GV_MAXPLEXNAME))
			return (p);
	}

	return (NULL);
}

/* Find a subdisk by name. */
struct gv_sd *
gv_find_sd(struct gv_softc *sc, char *name)
{
	struct gv_sd *s;

	LIST_FOREACH(s, &sc->subdisks, sd) {
		if (!strncmp(s->name, name, GV_MAXSDNAME))
			return (s);
	}

	return (NULL);
}

/* Find a drive by name. */
struct gv_drive *
gv_find_drive(struct gv_softc *sc, char *name)
{
	struct gv_drive *d;

	LIST_FOREACH(d, &sc->drives, drive) {
		if (!strncmp(d->name, name, GV_MAXDRIVENAME))
			return (d);
	}

	return (NULL);
}

/* Check if any consumer of the given geom is open. */
int
gv_is_open(struct g_geom *gp)
{
	struct g_consumer *cp;

	if (gp == NULL)
		return (0);

	LIST_FOREACH(cp, &gp->consumer, consumer) {
		if (cp->acr || cp->acw || cp->ace)
			return (1);
	}

	return (0);
}

/* Return the type of object identified by string 'name'. */
int
gv_object_type(struct gv_softc *sc, char *name)
{
	struct gv_drive *d;
	struct gv_plex *p;
	struct gv_sd *s;
	struct gv_volume *v;

	LIST_FOREACH(v, &sc->volumes, volume) {
		if (!strncmp(v->name, name, GV_MAXVOLNAME))
			return (GV_TYPE_VOL);
	}

	LIST_FOREACH(p, &sc->plexes, plex) {
		if (!strncmp(p->name, name, GV_MAXPLEXNAME))
			return (GV_TYPE_PLEX);
	}

	LIST_FOREACH(s, &sc->subdisks, sd) {
		if (!strncmp(s->name, name, GV_MAXSDNAME))
			return (GV_TYPE_SD);
	}

	LIST_FOREACH(d, &sc->drives, drive) {
		if (!strncmp(d->name, name, GV_MAXDRIVENAME))
			return (GV_TYPE_DRIVE);
	}

	return (-1);
}

void
gv_kill_drive_thread(struct gv_drive *d)
{
	if (d->flags & GV_DRIVE_THREAD_ACTIVE) {
		d->flags |= GV_DRIVE_THREAD_DIE;
		wakeup(d);
		while (!(d->flags & GV_DRIVE_THREAD_DEAD))
			tsleep(d, PRIBIO, "gv_die", hz);
		d->flags &= ~GV_DRIVE_THREAD_ACTIVE;
		d->flags &= ~GV_DRIVE_THREAD_DIE;
		d->flags &= ~GV_DRIVE_THREAD_DEAD;
		g_free(d->bqueue);
		d->bqueue = NULL;
		mtx_destroy(&d->bqueue_mtx);
	}
}

void
gv_kill_plex_thread(struct gv_plex *p)
{
	if (p->flags & GV_PLEX_THREAD_ACTIVE) {
		p->flags |= GV_PLEX_THREAD_DIE;
		wakeup(p);
		while (!(p->flags & GV_PLEX_THREAD_DEAD))
			tsleep(p, PRIBIO, "gv_die", hz);
		p->flags &= ~GV_PLEX_THREAD_ACTIVE;
		p->flags &= ~GV_PLEX_THREAD_DIE;
		p->flags &= ~GV_PLEX_THREAD_DEAD;
		g_free(p->bqueue);
		g_free(p->wqueue);
		p->bqueue = NULL;
		p->wqueue = NULL;
		mtx_destroy(&p->bqueue_mtx);
	}
}

void
gv_kill_vol_thread(struct gv_volume *v)
{
	if (v->flags & GV_VOL_THREAD_ACTIVE) {
		v->flags |= GV_VOL_THREAD_DIE;
		wakeup(v);
		while (!(v->flags & GV_VOL_THREAD_DEAD))
			tsleep(v, PRIBIO, "gv_die", hz);
		v->flags &= ~GV_VOL_THREAD_ACTIVE;
		v->flags &= ~GV_VOL_THREAD_DIE;
		v->flags &= ~GV_VOL_THREAD_DEAD;
		g_free(v->bqueue);
		v->bqueue = NULL;
		mtx_destroy(&v->bqueue_mtx);
	}
}
