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
 *
 * This is the method for dealing with BSD disklabels.  It has been
 * extensively (by my standards at least) commented, in the vain hope that
 * it will server as the source in future copy&paste operations.
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
#include <sys/stdint.h>
#include <sys/errno.h>
#include <sys/disklabel.h>
#include <geom/geom.h>
#include <geom/geom_slice.h>

#define	BSD_CLASS_NAME "BSD"

#define ALPHA_LABEL_OFFSET	64

/*
 * Our private data about one instance.  All the rest is handled by the
 * slice code and stored in its softc, so this is just the stuff
 * specific to BSD disklabels.
 */
struct g_bsd_softc {
	off_t	labeloffset;
	off_t	mbroffset;
	off_t	rawoffset;
	struct disklabel ondisk;
	struct disklabel inram;
};

/*
 * The next 4 functions isolate us from how the compiler lays out and pads
 * "struct disklabel".  We treat what we read from disk as a bytestream and
 * explicitly convert it into a struct disklabel.  This makes us compiler-
 * endianness- and wordsize- agnostic.
 * For now we only have little-endian formats to deal with.
 */

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
	int i;

	d->d_magic = g_dec_le4(ptr + 0);
	d->d_type = g_dec_le2(ptr + 4);
	d->d_subtype = g_dec_le2(ptr + 6);
	bcopy(ptr + 8, d->d_typename, 16);
	bcopy(ptr + 24, d->d_packname, 16);
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
	for (i = 0; i < MAXPARTITIONS; i++)
		g_bsd_ledec_partition(ptr + 148 + 16 * i, &d->d_partitions[i]);
}

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
	int i;

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
	for (i = 0; i < MAXPARTITIONS; i++)
		g_bsd_leenc_partition(ptr + 148 + 16 * i, &d->d_partitions[i]);
}

static int
g_bsd_ondisk_size(void)
{
	return (148 + 16 * MAXPARTITIONS);
}

/*
 * For reasons which were valid and just in their days, FreeBSD/i386 uses
 * absolute disk-addresses in disklabels.  The way it works is that the
 * p_offset field of all partitions have the first sector number of the
 * disk slice added to them.  This was hidden kernel-magic, userland did
 * not see these offsets.  These two functions subtract and add them
 * while converting from the "ondisk" to the "inram" labels and vice
 * versa.
 */
static void
ondisk2inram(struct g_bsd_softc *sc)
{
	struct partition *ppp;
	struct disklabel *dl;
	int i;

	sc->inram = sc->ondisk;
	dl = &sc->inram;

	/* Basic sanity-check needed to avoid mistakes. */
	if (dl->d_magic != DISKMAGIC || dl->d_magic2 != DISKMAGIC)
		return;
	if (dl->d_npartitions > MAXPARTITIONS)
		return;

	sc->rawoffset = dl->d_partitions[RAW_PART].p_offset;
	for (i = 0; i < dl->d_npartitions; i++) {
		ppp = &dl->d_partitions[i];
		if (ppp->p_size != 0 && ppp->p_offset < sc->rawoffset)
			sc->rawoffset = 0;
	}
	if (sc->rawoffset > 0) {
		for (i = 0; i < dl->d_npartitions; i++) {
			ppp = &dl->d_partitions[i];
			if (ppp->p_offset != 0)
				ppp->p_offset -= sc->rawoffset;
		}
	}
	dl->d_checksum = 0;
	dl->d_checksum = dkcksum(&sc->inram);
}

static void
inram2ondisk(struct g_bsd_softc *sc)
{
	struct partition *ppp;
	int i;

	sc->ondisk = sc->inram;
	if (sc->mbroffset != 0)
		sc->rawoffset = sc->mbroffset / sc->inram.d_secsize; 
	if (sc->rawoffset != 0) {
		for (i = 0; i < sc->inram.d_npartitions; i++) {
			ppp = &sc->ondisk.d_partitions[i];
			if (ppp->p_size > 0) 
				ppp->p_offset += sc->rawoffset;
			else
				ppp->p_offset = 0;
		}
	}
	sc->ondisk.d_checksum = 0;
	sc->ondisk.d_checksum = dkcksum(&sc->ondisk);
}

/*
 * Check that this looks like a valid disklabel, but be prepared
 * to get any kind of junk.  The checksum must be checked only
 * after this function returns success to prevent a bogus d_npartitions
 * value from tripping us up.
 */
static int
g_bsd_checklabel(struct disklabel *dl)
{
	struct partition *ppp;
	int i;

	if (dl->d_magic != DISKMAGIC || dl->d_magic2 != DISKMAGIC)
		return (EINVAL);
	/*
	 * If the label specifies more partitions than we can handle
	 * we have to reject it:  If people updated the label they would
	 * trash it, and that would break the checksum.
	 */
	if (dl->d_npartitions > MAXPARTITIONS)
		return (EINVAL);

	for (i = 0; i < dl->d_npartitions; i++) {
		ppp = &dl->d_partitions[i];
		/* Cannot extend past unit. */
		if (ppp->p_size != 0 &&
		     ppp->p_offset + ppp->p_size > dl->d_secperunit) {
			return (EINVAL);
		}
	}
	return (0);
}

/*
 * Modify our slicer to match proposed disklabel, if possible.
 * First carry out all the simple checks, then lock topology
 * and check that no open providers are affected negatively
 * then carry out all the changes.
 *
 * NB: Returns with topology held only if successful return.
 */
static int
g_bsd_modify(struct g_geom *gp, struct disklabel *dl)
{
	int i, error;
	struct partition *ppp;
	struct g_slicer *gsp;
	struct g_consumer *cp;
	u_int secsize, u;
	off_t mediasize;

	/* Basic check that this is indeed a disklabel. */
	error = g_bsd_checklabel(dl);
	if (error)
		return (error);

	/* Make sure the checksum is OK. */
	if (dkcksum(dl) != 0)
		return (EINVAL);

	/* Get dimensions of our device. */
	cp = LIST_FIRST(&gp->consumer);
	secsize = cp->provider->sectorsize;
	mediasize = cp->provider->mediasize;

#ifdef nolonger
	/*
	 * The raw-partition must start at zero.  We do not check that the
	 * size == mediasize because this is overly restrictive.  We have
	 * already tested in g_bsd_checklabel() that it is not longer.
	 * XXX: RAW_PART is archaic anyway, and we should drop it.
	 */
	if (dl->d_partitions[RAW_PART].p_offset != 0)
		return (EINVAL);
#endif

#ifdef notyet
	/*
	 * Indications are that the d_secperunit is not correctly
	 * initialized in many cases, and since we don't need it
	 * for anything, we dont strictly need this test.
	 * Preemptive action to avoid confusing people in disklabel(8)
	 * may be in order.
	 */
	/* The label cannot claim a larger size than the media. */
	if ((off_t)dl->d_secperunit * dl->d_secsize > mediasize)
		return (EINVAL);
#endif


	/* ... or a smaller sector size. */
	if (dl->d_secsize < secsize)
		return (EINVAL);

	/* ... or a non-multiple sector size. */
	if (dl->d_secsize % secsize != 0)
		return (EINVAL);

	g_topology_lock();

	/* Don't munge open partitions. */
	gsp = gp->softc;
	for (i = 0; i < dl->d_npartitions; i++) {
		ppp = &dl->d_partitions[i];

		error = g_slice_config(gp, i, G_SLICE_CONFIG_CHECK,
		    (off_t)ppp->p_offset * dl->d_secsize,
		    (off_t)ppp->p_size * dl->d_secsize,
		     dl->d_secsize,
		    "%s%c", gp->name, 'a' + i);
		if (error) {
			g_topology_unlock();
			return (error);
		}
	}

	/* Look good, go for it... */
	for (u = 0; u < gsp->nslice; u++) {
		ppp = &dl->d_partitions[u];
		g_slice_config(gp, u, G_SLICE_CONFIG_SET,
		    (off_t)ppp->p_offset * dl->d_secsize,
		    (off_t)ppp->p_size * dl->d_secsize,
		     dl->d_secsize,
		    "%s%c", gp->name, 'a' + u);
	}
	return (0);
}

/*
 * Calculate a disklabel checksum for a little-endian byte-stream.
 * We need access to the decoded disklabel because the checksum only
 * covers the partition data for the first d_npartitions.
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

/*
 * This is an internal helper function, called multiple times from the taste
 * function to try to locate a disklabel on the disk.  More civilized formats
 * will not need this, as there is only one possible place on disk to look
 * for the magic spot.
 */

static int
g_bsd_try(struct g_geom *gp, struct g_slicer *gsp, struct g_consumer *cp, int secsize, struct g_bsd_softc *ms, off_t offset)
{
	int error;
	u_char *buf;
	struct disklabel *dl;
	off_t secoff;

	/*
	 * We need to read entire aligned sectors, and we assume that the
	 * disklabel does not span sectors, so one sector is enough.
	 */
	error = 0;
	secoff = offset % secsize;
	buf = g_read_data(cp, offset - secoff, secsize, &error);
	if (buf == NULL || error != 0)
		return (ENOENT);

	/* Decode into our native format. */
	dl = &ms->ondisk;
	g_bsd_ledec_disklabel(buf + secoff, dl);

	ondisk2inram(ms);

	dl = &ms->inram;
	/* Does it look like a label at all? */
	if (g_bsd_checklabel(dl))
		error = ENOENT;
	/* ... and does the raw data have a good checksum? */
	if (error == 0 && g_bsd_lesum(dl, buf + secoff) != 0)
		error = ENOENT;

	/* Remember to free the buffer g_read_data() gave us. */
	g_free(buf);

	/* If we had a label, record it properly. */
	if (error == 0) {
		gsp->frontstuff = 16 * secsize;	/* XXX */
		ms->labeloffset = offset;
		g_topology_lock();
		g_slice_conf_hot(gp, 0, offset, g_bsd_ondisk_size());
		g_topology_unlock();
	}
	return (error);
}

/*
 * Implement certain ioctls to modify disklabels with.  This function
 * is called by the event handler thread with topology locked as result
 * of the g_call_me() in g_bsd_start().  It is not necessary to keep
 * topology locked all the time but make sure to return with topology
 * locked as well.
 */

static void
g_bsd_ioctl(void *arg)
{
	struct bio *bp;
	struct g_geom *gp;
	struct g_slicer *gsp;
	struct g_bsd_softc *ms;
	struct disklabel *dl;
	struct g_ioctl *gio;
	struct g_consumer *cp;
	u_char *buf;
	off_t secoff;
	u_int secsize;
	int error, i;
	uint64_t sum;

	/* We don't need topology for now. */
	g_topology_unlock();

	/* Get hold of the interesting bits from the bio. */
	bp = arg;
	gp = bp->bio_to->geom;
	gsp = gp->softc;
	ms = gsp->softc;
	gio = (struct g_ioctl *)bp->bio_data;

	/* The disklabel to set is the ioctl argument. */
	dl = gio->data;

	/* Validate and modify our slice instance to match. */
	error = g_bsd_modify(gp, dl);	/* Picks up topology lock on success. */
	if (error) {
		g_topology_lock();
		g_io_deliver(bp, error);
		return;
	}
	/* Update our copy of the disklabel. */
	ms->inram = *dl;
	inram2ondisk(ms);

	if (gio->cmd == DIOCSDINFO) {
		g_io_deliver(bp, 0);
		return;
	}
	KASSERT(gio->cmd == DIOCWDINFO, ("Unknown ioctl in g_bsd_ioctl"));
	cp = LIST_FIRST(&gp->consumer);
	/* Get sector size, we need it to read data. */
	secsize = cp->provider->sectorsize;
	secoff = ms->labeloffset % secsize;
	buf = g_read_data(cp, ms->labeloffset - secoff, secsize, &error);
	if (buf == NULL || error != 0) {
		g_io_deliver(bp, error);
		return;
	}
	dl = &ms->ondisk;
	g_bsd_leenc_disklabel(buf + secoff, dl);
	if (ms->labeloffset == ALPHA_LABEL_OFFSET) {
		sum = 0;
		for (i = 0; i < 63; i++)
			sum += g_dec_le8(buf + i * 8);
		g_enc_le8(buf + 504, sum);
	}
	error = g_write_data(cp, ms->labeloffset - secoff, buf, secsize);
	g_free(buf);
	g_io_deliver(bp, error);
}

/*
 * If the user tries to overwrite our disklabel through an open partition
 * or via a magicwrite config call, we end up here and try to prevent
 * footshooting as best we can.
 */
static void
g_bsd_hotwrite(void *arg)
{
	struct bio *bp;
	struct g_geom *gp;
	struct g_slicer *gsp;
	struct g_slice *gsl;
	struct g_bsd_softc *ms;
	struct g_bsd_softc fake;
	u_char *p;
	int error;
	
	bp = arg;
	gp = bp->bio_to->geom;
	gsp = gp->softc;
	ms = gsp->softc;
	gsl = &gsp->slices[bp->bio_to->index];
	p = (u_char*)bp->bio_data + ms->labeloffset 
	    - (bp->bio_offset + gsl->offset);
	g_bsd_ledec_disklabel(p, &fake.ondisk);
	
	ondisk2inram(&fake);
	if (g_bsd_checklabel(&fake.inram)) {
		g_io_deliver(bp, EPERM);
		return;
	}
	if (g_bsd_lesum(&fake.ondisk, p) != 0) {
		g_io_deliver(bp, EPERM);
		return;
	}
	g_topology_unlock();
	error = g_bsd_modify(gp, &fake.inram);	/* May pick up topology. */
	if (error) {
		g_io_deliver(bp, EPERM);
		g_topology_lock();
		return;
	}
	/* Update our copy of the disklabel. */
	ms->inram = fake.inram;
	inram2ondisk(ms);
	g_bsd_leenc_disklabel(p, &ms->ondisk);
	g_slice_finish_hot(bp);
}

/*-
 * This start routine is only called for non-trivial requests, all the
 * trivial ones are handled autonomously by the slice code.
 * For requests we handle here, we must call the g_io_deliver() on the
 * bio, and return non-zero to indicate to the slice code that we did so.
 * This code executes in the "DOWN" I/O path, this means:
 *    * No sleeping.
 *    * Don't grab the topology lock.
 *    * Don't call biowait, g_getattr(), g_setattr() or g_read_data()
 */

static int
g_bsd_start(struct bio *bp)
{
	struct g_geom *gp;
	struct g_bsd_softc *ms;
	struct g_slicer *gsp;
	struct g_ioctl *gio;
	int error;

	gp = bp->bio_to->geom;
	gsp = gp->softc;
	ms = gsp->softc;
	switch(bp->bio_cmd) {
	case BIO_READ:
		/* We allow reading of our hot spots */
		return (0);
	case BIO_DELETE:
		/* We do not allow deleting our hot spots */
		return (EPERM);
	case BIO_WRITE:
		g_call_me(g_bsd_hotwrite, bp);
		return (EJUSTRETURN);
	case BIO_GETATTR:
	case BIO_SETATTR:
		break;
	default:
		KASSERT(0 == 1, ("Unknown bio_cmd in g_bsd_start (%d)",
		    bp->bio_cmd));
	}

	/* We only handle ioctl(2) requests of the right format. */
	if (strcmp(bp->bio_attribute, "GEOM::ioctl"))
		return (0);
	else if (bp->bio_length != sizeof(*gio))
		return (0);

	/* Get hold of the ioctl parameters. */
	gio = (struct g_ioctl *)bp->bio_data;

	switch (gio->cmd) {
	case DIOCGDINFO:
		/* Return a copy of the disklabel to userland. */
		bcopy(&ms->inram, gio->data, sizeof(ms->inram));
		g_io_deliver(bp, 0);
		return (1);
	case DIOCSDINFO:
	case DIOCWDINFO:
		/*
		 * These we cannot do without the topology lock and some
		 * some I/O requests.  Ask the event-handler to schedule
		 * us in a less restricted environment.
		 */
		error = g_call_me(g_bsd_ioctl, bp);
		if (error)
			g_io_deliver(bp, error);
		/*
		 * We must return non-zero to indicate that we will deal
		 * with this bio, even though we have not done so yet.
		 */
		return (1);
	default:
		return (0);
	}
}

/*
 * Dump configuration information in XML format.
 * Notice that the function is called once for the geom and once for each
 * consumer and provider.  We let g_slice_dumpconf() do most of the work.
 */
static void
g_bsd_dumpconf(struct sbuf *sb, const char *indent, struct g_geom *gp, struct g_consumer *cp, struct g_provider *pp)
{
	struct g_bsd_softc *ms;
	struct g_slicer *gsp;

	gsp = gp->softc;
	ms = gsp->softc;
	g_slice_dumpconf(sb, indent, gp, cp, pp);
	if (indent != NULL && pp == NULL && cp == NULL) {
		sbuf_printf(sb, "%s<labeloffset>%jd</labeloffset>\n",
		    indent, (intmax_t)ms->labeloffset);
		sbuf_printf(sb, "%s<rawoffset>%jd</rawoffset>\n",
		    indent, (intmax_t)ms->rawoffset);
		sbuf_printf(sb, "%s<mbroffset>%jd</mbroffset>\n",
		    indent, (intmax_t)ms->mbroffset);
	}
}

/*
 * The taste function is called from the event-handler, with the topology
 * lock already held and a provider to examine.  The flags are unused.
 *
 * If flags == G_TF_NORMAL, the idea is to take a bite of the provider and
 * if we find valid, consistent magic on it, build a geom on it.
 * any magic bits which indicate that we should automatically put a BSD
 * geom on it.
 *
 * There may be cases where the operator would like to put a BSD-geom on
 * providers which do not meet all of the requirements.  This can be done
 * by instead passing the G_TF_INSIST flag, which will override these
 * checks.
 *
 * The final flags value is G_TF_TRANSPARENT, which instructs the method
 * to put a geom on top of the provider and configure it to be as transparent
 * as possible.  This is not really relevant to the BSD method and therefore
 * not implemented here.
 */

static struct g_geom *
g_bsd_taste(struct g_class *mp, struct g_provider *pp, int flags)
{
	struct g_geom *gp;
	struct g_consumer *cp;
	int error, i;
	struct g_bsd_softc *ms;
	struct disklabel *dl;
	u_int secsize;
	struct g_slicer *gsp;

	g_trace(G_T_TOPOLOGY, "bsd_taste(%s,%s)", mp->name, pp->name);
	g_topology_assert();

	/* We don't implement transparent inserts. */
	if (flags == G_TF_TRANSPARENT)
		return (NULL);

	/*
	 * The BSD-method will not automatically configure itself recursively
	 * Note that it is legal to examine the class-name of our provider,
	 * nothing else should ever be examined inside the provider.
	 */
	if (flags == G_TF_NORMAL &&
	    !strcmp(pp->geom->class->name, BSD_CLASS_NAME))
		return (NULL);

	/*
	 * BSD labels are a subclass of the general "slicing" topology so
	 * a lot of the work can be done by the common "slice" code.
	 * Create a geom with space for MAXPARTITIONS providers, one consumer
	 * and a softc structure for us.  Specify the provider to attach
	 * the consumer to and our "start" routine for special requests.
	 * The provider is opened with mode (1,0,0) so we can do reads
	 * from it.
	 */
	gp = g_slice_new(mp, MAXPARTITIONS, pp, &cp, &ms,
	     sizeof(*ms), g_bsd_start);
	if (gp == NULL)
		return (NULL);

	/*
	 * Now that we have attached to and opened our provider, we do
	 * not need the topology lock until we change the topology again
	 * next time.
	 */
	g_topology_unlock();

	/*
	 * Fill in the optional details, in our case we have a dumpconf
	 * routine which the "slice" code should call at the right time
	 */
	gp->dumpconf = g_bsd_dumpconf;

	/* Get the geom_slicer softc from the geom. */
	gsp = gp->softc;

	/*
	 * The do...while loop here allows us to have multiple escapes
	 * using a simple "break".  This improves code clarity without
	 * ending up in deep nesting and without using goto or come from.
	 */
	do {
		/*
		 * If the provider is an MBR we will only auto attach
		 * to type 165 slices in the G_TF_NORMAL case.  We will
		 * attach to any other type (BSD was handles above)
		 */
		error = g_getattr("MBR::type", cp, &i);
		if (!error) {
			if (i != 165 && flags == G_TF_NORMAL)
				break;
			error = g_getattr("MBR::offset", cp, &ms->mbroffset);
			if (error)
				break;
		}

		/* Same thing if we are inside a PC98 */
		error = g_getattr("PC98::type", cp, &i);
		if (!error) {
			if (i != 0xc494 && flags == G_TF_NORMAL)
				break;
			error = g_getattr("PC98::offset", cp, &ms->mbroffset);
			if (error)
				break;
		}

		/* Get sector size, we need it to read data. */
		secsize = cp->provider->sectorsize;
		if (secsize < 512)
			break;

		/* First look for a label at the start of the second sector. */
		error = g_bsd_try(gp, gsp, cp, secsize, ms, secsize);

		/* Next, look for alpha labels */
		if (error)
			error = g_bsd_try(gp, gsp, cp, secsize, ms,
			    ALPHA_LABEL_OFFSET);

		/* If we didn't find a label, punt. */
		if (error)
			break;

		/*
		 * Process the found disklabel, and modify our "slice"
		 * instance to match it, if possible.
		 */
		dl = &ms->inram;
		error = g_bsd_modify(gp, dl);	/* Picks up topology lock. */
		if (!error)
			g_topology_unlock();
		break;
	} while (0);

	/* Success of failure, we can close our provider now. */
	g_topology_lock();
	error = g_access_rel(cp, -1, 0, 0);

	/* If we have configured any providers, return the new geom. */
	if (gsp->nprovider > 0)
		return (gp);
	/*
	 * ...else push the "self-destruct" button, by spoiling our own
	 * consumer.  This triggers a call to g_std_spoiled which will
	 * dismantle what was setup.
	 */
	g_std_spoiled(cp);
	return (NULL);
}

/* Finally, register with GEOM infrastructure. */
static struct g_class g_bsd_class = {
	BSD_CLASS_NAME,
	g_bsd_taste,
	NULL,
	G_CLASS_INITIALIZER
};

DECLARE_GEOM_CLASS(g_bsd_class, g_bsd);
