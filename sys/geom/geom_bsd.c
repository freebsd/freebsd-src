/*-
 * Copyright (c) 2002 Poul-Henning Kamp
 * Copyright (c) 2002 Networks Associates Technology, Inc.
 * All rights reserved.
 *
 * This software was developed for the FreeBSD Project by Poul-Henning Kamp
 * and NAI Labs, the Security Research Division of Network Associates, Inc.
 * under DARPA/SPAWAR contract N66001-01-C-8035 ("CBOSS"), as part of the
 * DARPA CHATS research program.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The names of the authors may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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


#include <sys/param.h>
#ifndef _KERNEL
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <err.h>
#else
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/bio.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#endif
#include <sys/errno.h>
#include <sys/disklabel.h>
#include <geom/geom.h>
#include <geom/geom_slice.h>

#define BSD_CLASS_NAME "BSD-class"

struct g_bsd_softc {
	struct disklabel ondisk;
	struct disklabel inram;
};

static void
g_bsd_ledec_partition(u_char *ptr, struct partition *d)
{
	d->p_size = g_dec_le4(ptr + 0);
	d->p_offset = g_dec_le4(ptr + 4);
	d->p_fsize = g_dec_le4(ptr + 8);
	d->p_fstype = ptr[12];
	d->p_frag = ptr[13];
	d->p_cpg = g_dec_le2(ptr + 14);
}

static void
g_bsd_ledec_disklabel(u_char *ptr, struct disklabel *d)
{
	d->d_magic = g_dec_le4(ptr + 0);
	d->d_type = g_dec_le2(ptr + 4);
	d->d_subtype = g_dec_le2(ptr + 6);
	bcopy(ptr + 8, d->d_typename, 16);
	bcopy(d->d_packname, ptr + 24, 16);
	d->d_secsize = g_dec_le4(ptr + 40);
	d->d_nsectors = g_dec_le4(ptr + 44);
	d->d_ntracks = g_dec_le4(ptr + 48);
	d->d_ncylinders = g_dec_le4(ptr + 52);
	d->d_secpercyl = g_dec_le4(ptr + 56);
	d->d_secperunit = g_dec_le4(ptr + 60);
	d->d_sparespertrack = g_dec_le2(ptr + 64);
	d->d_sparespercyl = g_dec_le2(ptr + 66);
	d->d_acylinders = g_dec_le4(ptr + 68);
	d->d_rpm = g_dec_le2(ptr + 72);
	d->d_interleave = g_dec_le2(ptr + 74);
	d->d_trackskew = g_dec_le2(ptr + 76);
	d->d_cylskew = g_dec_le2(ptr + 78);
	d->d_headswitch = g_dec_le4(ptr + 80);
	d->d_trkseek = g_dec_le4(ptr + 84);
	d->d_flags = g_dec_le4(ptr + 88);
	d->d_drivedata[0] = g_dec_le4(ptr + 92);
	d->d_drivedata[1] = g_dec_le4(ptr + 96);
	d->d_drivedata[2] = g_dec_le4(ptr + 100);
	d->d_drivedata[3] = g_dec_le4(ptr + 104);
	d->d_drivedata[4] = g_dec_le4(ptr + 108);
	d->d_spare[0] = g_dec_le4(ptr + 112);
	d->d_spare[1] = g_dec_le4(ptr + 116);
	d->d_spare[2] = g_dec_le4(ptr + 120);
	d->d_spare[3] = g_dec_le4(ptr + 124);
	d->d_spare[4] = g_dec_le4(ptr + 128);
	d->d_magic2 = g_dec_le4(ptr + 132);
	d->d_checksum = g_dec_le2(ptr + 136);
	d->d_npartitions = g_dec_le2(ptr + 138);
	d->d_bbsize = g_dec_le4(ptr + 140);
	d->d_sbsize = g_dec_le4(ptr + 144);
	g_bsd_ledec_partition(ptr + 148, &d->d_partitions[0]);
	g_bsd_ledec_partition(ptr + 164, &d->d_partitions[1]);
	g_bsd_ledec_partition(ptr + 180, &d->d_partitions[2]);
	g_bsd_ledec_partition(ptr + 196, &d->d_partitions[3]);
	g_bsd_ledec_partition(ptr + 212, &d->d_partitions[4]);
	g_bsd_ledec_partition(ptr + 228, &d->d_partitions[5]);
	g_bsd_ledec_partition(ptr + 244, &d->d_partitions[6]);
	g_bsd_ledec_partition(ptr + 260, &d->d_partitions[7]);
}

#if 0
static void
g_bsd_leenc_partition(u_char *ptr, struct partition *d)
{
	g_enc_le4(ptr + 0, d->p_size);
	g_enc_le4(ptr + 4, d->p_offset);
	g_enc_le4(ptr + 8, d->p_fsize);
	ptr[12] = d->p_fstype;
	ptr[13] = d->p_frag;
	g_enc_le2(ptr + 14, d->p_cpg);
}

static void
g_bsd_leenc_disklabel(u_char *ptr, struct disklabel *d)
{
	g_enc_le4(ptr + 0, d->d_magic);
	g_enc_le2(ptr + 4, d->d_type);
	g_enc_le2(ptr + 6, d->d_subtype);
	bcopy(d->d_typename, ptr + 8, 16);
	bcopy(d->d_packname, ptr + 24, 16);
	g_enc_le4(ptr + 40, d->d_secsize);
	g_enc_le4(ptr + 44, d->d_nsectors);
	g_enc_le4(ptr + 48, d->d_ntracks);
	g_enc_le4(ptr + 52, d->d_ncylinders);
	g_enc_le4(ptr + 56, d->d_secpercyl);
	g_enc_le4(ptr + 60, d->d_secperunit);
	g_enc_le2(ptr + 64, d->d_sparespertrack);
	g_enc_le2(ptr + 66, d->d_sparespercyl);
	g_enc_le4(ptr + 68, d->d_acylinders);
	g_enc_le2(ptr + 72, d->d_rpm);
	g_enc_le2(ptr + 74, d->d_interleave);
	g_enc_le2(ptr + 76, d->d_trackskew);
	g_enc_le2(ptr + 78, d->d_cylskew);
	g_enc_le4(ptr + 80, d->d_headswitch);
	g_enc_le4(ptr + 84, d->d_trkseek);
	g_enc_le4(ptr + 88, d->d_flags);
	g_enc_le4(ptr + 92, d->d_drivedata[0]);
	g_enc_le4(ptr + 96, d->d_drivedata[1]);
	g_enc_le4(ptr + 100, d->d_drivedata[2]);
	g_enc_le4(ptr + 104, d->d_drivedata[3]);
	g_enc_le4(ptr + 108, d->d_drivedata[4]);
	g_enc_le4(ptr + 112, d->d_spare[0]);
	g_enc_le4(ptr + 116, d->d_spare[1]);
	g_enc_le4(ptr + 120, d->d_spare[2]);
	g_enc_le4(ptr + 124, d->d_spare[3]);
	g_enc_le4(ptr + 128, d->d_spare[4]);
	g_enc_le4(ptr + 132, d->d_magic2);
	g_enc_le2(ptr + 136, d->d_checksum);
	g_enc_le2(ptr + 138, d->d_npartitions);
	g_enc_le4(ptr + 140, d->d_bbsize);
	g_enc_le4(ptr + 144, d->d_sbsize);
	g_bsd_leenc_partition(ptr + 148, &d->d_partitions[0]);
	g_bsd_leenc_partition(ptr + 164, &d->d_partitions[1]);
	g_bsd_leenc_partition(ptr + 180, &d->d_partitions[2]);
	g_bsd_leenc_partition(ptr + 196, &d->d_partitions[3]);
	g_bsd_leenc_partition(ptr + 212, &d->d_partitions[4]);
	g_bsd_leenc_partition(ptr + 228, &d->d_partitions[5]);
	g_bsd_leenc_partition(ptr + 244, &d->d_partitions[6]);
	g_bsd_leenc_partition(ptr + 260, &d->d_partitions[7]);
}

#endif

static void
ondisk2inram(struct g_bsd_softc *sc)
{
	struct partition *ppp;
	unsigned offset;
	int i;

	sc->inram = sc->ondisk;
	offset = sc->inram.d_partitions[RAW_PART].p_offset;
	for (i = 0; i < 8; i++) {
		ppp = &sc->inram.d_partitions[i];
		if (ppp->p_offset >= offset)
			ppp->p_offset -= offset;
	}
	sc->inram.d_checksum = 0;
	sc->inram.d_checksum = dkcksum(&sc->inram);
}

/*
 * It is rather fortunate that this checksum only covers up to the
 * actual end of actual data, otherwise the pointer-screwup in
 * alpha architectures would have been much harder to handle.
 */
static int
g_bsd_lesum(struct disklabel *dl, u_char *p)
{
	u_char *pe;
	uint16_t sum;

	pe = p + 148 + 16 * dl->d_npartitions;
	sum = 0;
	while (p < pe) {
		sum ^= g_dec_le2(p);
		p += 2;
	}
	return (sum);
}

static int
g_bsd_i386(struct g_consumer *cp, int secsize, struct disklabel *dl)
{
	int error;
	u_char *buf;

	buf = g_read_data(cp, secsize * 1, secsize, &error);
	if (buf == NULL || error != 0)
		return(ENOENT);
	g_bsd_ledec_disklabel(buf, dl);
	if (dl->d_magic == DISKMAGIC &&
	    dl->d_magic2 == DISKMAGIC &&
	    g_bsd_lesum(dl, buf) == 0) 
		error = 0;
	else
		error = ENOENT;
	g_free(buf);
	return(error);
}

static int
g_bsd_alpha(struct g_consumer *cp, int secsize, struct disklabel *dl)
{
	int error;
	u_char *buf;

	buf = g_read_data(cp, 0, secsize, &error);
	if (buf == NULL || error != 0)
		return(ENOENT);
	g_bsd_ledec_disklabel(buf + 64, dl);
	if (dl->d_magic == DISKMAGIC &&
	    dl->d_magic2 == DISKMAGIC &&
	    g_bsd_lesum(dl, buf) == 0) 
		error = 0;
	else
		error = ENOENT;
	g_free(buf);
	return(error);
}

static int
g_bsd_start(struct bio *bp)
{
	struct g_geom *gp;
	struct g_bsd_softc *ms;
	struct g_slicer *gsp;
	struct g_ioctl *gio;

	gp = bp->bio_to->geom;
	gsp = gp->softc;
	ms = gsp->softc;
	if (strcmp(bp->bio_attribute, "GEOM::ioctl"))
		return(0);
	else if (bp->bio_length != sizeof *gio)
		return(0);
	gio = (struct g_ioctl *)bp->bio_data;
	if (gio->cmd == DIOCGDINFO) {
		bcopy(&ms->inram, gio->data, sizeof ms->inram);
		bp->bio_error = 0;
		g_io_deliver(bp);
		return (1);
	}
#ifdef _KERNEL
	if (gio->cmd == DIOCGPART) {
		struct partinfo pi;
		pi.disklab = &ms->inram;
		pi.part = &ms->inram.d_partitions[bp->bio_to->index];
		bcopy(&pi, gio->data, sizeof pi);
		bp->bio_error = 0;
		g_io_deliver(bp);
		return (1);
	}
#endif
	return (0);
}

static void
g_bsd_dumpconf(struct sbuf *sb, char *indent, struct g_geom *gp, struct g_consumer *cp __unused, struct g_provider *pp)
{
#if 0
	struct g_mbr_softc *ms;
	struct g_slicer *gsp;

	gsp = gp->softc;
	ms = gsp->softc;
	if (pp != NULL) {
		sbuf_printf(sb, "%s<type>%d</type>\n",
		    indent, ms->type[pp->index]);
	}
#endif
	g_slice_dumpconf(sb, indent, gp, cp, pp);
}

static struct g_geom *
g_bsd_taste(struct g_class *mp, struct g_provider *pp, struct thread *tp, int flags)
{
	struct g_geom *gp;
	struct g_consumer *cp;
	struct g_provider *pp2;
	int error, i, j, npart;
	struct g_bsd_softc *ms;
	struct disklabel *dl;
	u_int secsize;
	u_int fwsectors, fwheads;
	off_t mediasize;
	struct partition *ppp, *ppr;

	g_trace(G_T_TOPOLOGY, "bsd_taste(%s,%s)", mp->name, pp->name);
	g_topology_assert();
	if (flags == G_TF_NORMAL &&
	    !strcmp(pp->geom->class->name, BSD_CLASS_NAME))
		return (NULL);
	gp = g_slice_new(mp, 8, pp, &cp, &ms, sizeof *ms, g_bsd_start);
	if (gp == NULL)
		return (NULL);
	g_topology_unlock();
	gp->dumpconf = g_bsd_dumpconf;
	npart = 0;
	while (1) {	/* a trick to allow us to use break */
		j = sizeof i;
		error = g_io_getattr("MBR::type", cp, &j, &i, tp);
		if (!error && i != 165 && flags == G_TF_NORMAL)
			break;
		j = sizeof secsize;
		error = g_io_getattr("GEOM::sectorsize", cp, &j, &secsize, tp);
		if (error) {
			secsize = 512;
			printf("g_bsd_taste: error %d Sectors are %d bytes\n",
			    error, secsize);
		}
		j = sizeof mediasize;
		error = g_io_getattr("GEOM::mediasize", cp, &j, &mediasize, tp);
		if (error) {
			mediasize = 0;
			printf("g_error %d Mediasize is %lld bytes\n",
			    error, mediasize);
		}
		error = g_bsd_i386(cp, secsize, &ms->ondisk);
		if (error)
			error = g_bsd_alpha(cp, secsize, &ms->ondisk);
		if (error)
			break;
		dl = &ms->ondisk;
		if (bootverbose)
			g_hexdump(dl, sizeof(*dl));
		if (dl->d_secsize < secsize)
			break;
		if (dl->d_secsize > secsize)
			secsize = dl->d_secsize;
		ppr = &dl->d_partitions[2];
		for (i = 0; i < 8; i++) {
			ppp = &dl->d_partitions[i];
			if (ppp->p_size == 0)
				continue;
			npart++;
			pp2 = g_slice_addslice(gp, i,
			    ((off_t)(ppp->p_offset - ppr->p_offset)) << 9ULL,
			    ((off_t)ppp->p_size) << 9ULL,
			    "%s%c", pp->name, 'a' + i);
			g_error_provider(pp2, 0);
		}
		ondisk2inram(ms);
		break;
	}
	if (npart == 0 && (
	    (flags == G_TF_INSIST && mediasize != 0) ||
	    (flags == G_TF_TRANSPARENT))) {
		dl = &ms->ondisk;
		bzero(dl, sizeof *dl);
		dl->d_magic = DISKMAGIC;
		dl->d_magic2 = DISKMAGIC;
		ppp = &dl->d_partitions[RAW_PART];
		ppp->p_offset = 0;
		ppp->p_size = mediasize / secsize;
		dl->d_npartitions = MAXPARTITIONS;
		dl->d_interleave = 1;
		dl->d_secsize = secsize;
		dl->d_rpm = 3600;
		j = sizeof fwsectors;
		error = g_io_getattr("GEOM::fwsectors", cp, &j, &fwsectors, tp);
		if (error)
			dl->d_nsectors = 32;
		else
			dl->d_nsectors = fwsectors;
		error = g_io_getattr("GEOM::fwheads", cp, &j, &fwheads, tp);
		if (error)
			dl->d_ntracks = 64;
		else
			dl->d_ntracks = fwheads;
		dl->d_secpercyl = dl->d_nsectors * dl->d_ntracks;
		dl->d_ncylinders = ppp->p_size / dl->d_secpercyl;
		dl->d_secperunit = ppp->p_size;
		dl->d_checksum = 0;
		dl->d_checksum = dkcksum(dl);
		ms->inram = ms->ondisk;
		pp2 = g_slice_addslice(gp, RAW_PART,
		    0, mediasize, "%s%c", pp->name, 'a' + RAW_PART);
		g_error_provider(pp2, 0);
		npart = 1;
	}
	g_topology_lock();
	error = g_access_rel(cp, -1, 0, 0);
	if (npart > 0)
		return (gp);
	g_std_spoiled(cp);
	return (NULL);
}

static struct g_class g_bsd_class	= {
	BSD_CLASS_NAME,
	g_bsd_taste,
	g_slice_access,
	g_slice_orphan,
	NULL,
	G_CLASS_INITSTUFF
};

DECLARE_GEOM_CLASS(g_bsd_class, g_bsd);
