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
 * it will serve as the source in future copy&paste operations.
 */

#include <sys/param.h>
#include <sys/endian.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/bio.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/md5.h>
#include <sys/errno.h>
#include <sys/disklabel.h>
#include <geom/geom.h>
#include <geom/geom_slice.h>

#define	BSD_CLASS_NAME "BSD"

#define ALPHA_LABEL_OFFSET	64

#define LABELSIZE (148 + 16 * MAXPARTITIONS)

static void g_bsd_hotwrite(void *arg, int flag);
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
	u_char	label[LABELSIZE];
	u_char	labelsum[16];
};

/*
 * Modify our slicer to match proposed disklabel, if possible.
 * This is where we make sure we don't do something stupid.
 */
static int
g_bsd_modify(struct g_geom *gp, u_char *label)
{
	int i, error;
	struct partition *ppp;
	struct g_slicer *gsp;
	struct g_consumer *cp;
	struct g_bsd_softc *ms;
	u_int secsize, u;
	off_t rawoffset, o;
	struct disklabel dl;
	MD5_CTX md5sum;

	g_topology_assert();
	gsp = gp->softc;
	ms = gsp->softc;

	error = bsd_disklabel_le_dec(label, &dl, MAXPARTITIONS);
	if (error) {
		return (error);
	}

	/* Get dimensions of our device. */
	cp = LIST_FIRST(&gp->consumer);
	secsize = cp->provider->sectorsize;

	/* ... or a smaller sector size. */
	if (dl.d_secsize < secsize) {
		return (EINVAL);
	}

	/* ... or a non-multiple sector size. */
	if (dl.d_secsize % secsize != 0) {
		return (EINVAL);
	}

	/* Historical braindamage... */
	rawoffset = (off_t)dl.d_partitions[RAW_PART].p_offset * dl.d_secsize;

	for (i = 0; i < dl.d_npartitions; i++) {
		ppp = &dl.d_partitions[i];
		if (ppp->p_size == 0)
			continue;
	        o = (off_t)ppp->p_offset * dl.d_secsize;

		if (o < rawoffset)
			rawoffset = 0;
	}
	
	if (rawoffset != 0 && (off_t)rawoffset != ms->mbroffset)
		printf("WARNING: Expected rawoffset %jd, found %jd\n",
		    (intmax_t)ms->mbroffset/dl.d_secsize,
		    (intmax_t)rawoffset/dl.d_secsize);

	/* Don't munge open partitions. */
	for (i = 0; i < dl.d_npartitions; i++) {
		ppp = &dl.d_partitions[i];

	        o = (off_t)ppp->p_offset * dl.d_secsize;
		if (o == 0)
			o = rawoffset;
		error = g_slice_config(gp, i, G_SLICE_CONFIG_CHECK,
		    o - rawoffset,
		    (off_t)ppp->p_size * dl.d_secsize,
		     dl.d_secsize,
		    "%s%c", gp->name, 'a' + i);
		if (error)
			return (error);
	}

	/* Look good, go for it... */
	for (u = 0; u < gsp->nslice; u++) {
		ppp = &dl.d_partitions[u];
	        o = (off_t)ppp->p_offset * dl.d_secsize;
		if (o == 0)
			o = rawoffset;
		g_slice_config(gp, u, G_SLICE_CONFIG_SET,
		    o - rawoffset,
		    (off_t)ppp->p_size * dl.d_secsize,
		     dl.d_secsize,
		    "%s%c", gp->name, 'a' + u);
	}

	/* Update our softc */
	ms->ondisk = dl;
	if (label != ms->label)
		bcopy(label, ms->label, LABELSIZE);
	ms->rawoffset = rawoffset;

	/*
	 * In order to avoid recursively attaching to the same
	 * on-disk label (it's usually visible through the 'c'
	 * partition) we calculate an MD5 and ask if other BSD's
	 * below us love that label.  If they do, we don't.
	 */
	MD5Init(&md5sum);
	MD5Update(&md5sum, ms->label, sizeof(ms->label));
	MD5Final(ms->labelsum, &md5sum);

	return (0);
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
	error = bsd_disklabel_le_dec(buf + secoff, dl, MAXPARTITIONS);
	if (!error)
		bcopy(buf + secoff, ms->label, LABELSIZE);

	/* Remember to free the buffer g_read_data() gave us. */
	g_free(buf);

	ms->labeloffset = offset;
	return (error);
}

/*
 * This function writes the current label to disk, possibly updating
 * the alpha SRM checksum.
 */

static int
g_bsd_writelabel(struct g_geom *gp, u_char *bootcode)
{
	off_t secoff;
	u_int secsize;
	struct g_consumer *cp;
	struct g_slicer *gsp;
	struct g_bsd_softc *ms;
	u_char *buf;
	uint64_t sum;
	int error, i;

	gsp = gp->softc;
	ms = gsp->softc;
	cp = LIST_FIRST(&gp->consumer);
	/* Get sector size, we need it to read data. */
	secsize = cp->provider->sectorsize;
	secoff = ms->labeloffset % secsize;
	if (bootcode == NULL) {
		buf = g_read_data(cp, ms->labeloffset - secoff, secsize, &error);
		if (buf == NULL || error != 0)
			return (error);
		bcopy(ms->label, buf + secoff, sizeof(ms->label));
	} else {
		buf = bootcode;
		bcopy(ms->label, buf + ms->labeloffset, sizeof(ms->label));
	}
	if (ms->labeloffset == ALPHA_LABEL_OFFSET) {
		sum = 0;
		for (i = 0; i < 63; i++)
			sum += le64dec(buf + i * 8);
		le64enc(buf + 504, sum);
	}
	if (bootcode == NULL) {
		error = g_write_data(cp, ms->labeloffset - secoff, buf, secsize);
		g_free(buf);
	} else {
		error = g_write_data(cp, 0, bootcode, BBSIZE);
	}
	return(error);
}


/*
 * Implement certain ioctls to modify disklabels with.  This function
 * is called by the event handler thread with topology locked as result
 * of the g_post_event() in g_bsd_start().  It is not necessary to keep
 * topology locked all the time but make sure to return with topology
 * locked as well.
 */

static void
g_bsd_ioctl(void *arg, int flag)
{
	struct bio *bp;
	struct g_geom *gp;
	struct g_ioctl *gio;
	u_char *label;
	int error;

	g_topology_assert();
	bp = arg;
	if (flag == EV_CANCEL) {
		g_io_deliver(bp, ENXIO);
		return;
	}

	gp = bp->bio_to->geom;
	gio = (struct g_ioctl *)bp->bio_data;

	label = g_malloc(LABELSIZE, M_WAITOK);

	/* The disklabel to set is the ioctl argument. */
	bsd_disklabel_le_enc(label, gio->data);

	/* Validate and modify our slice instance to match. */
	error = g_bsd_modify(gp, label);	/* Picks up topology lock on success. */
	g_free(label);
	if (error || gio->cmd == DIOCSDINFO) {
		g_io_deliver(bp, error);
		return;
	}
	
	KASSERT(gio->cmd == DIOCWDINFO, ("Unknown ioctl in g_bsd_ioctl"));
	g_io_deliver(bp, g_bsd_writelabel(gp, NULL));
}

/*
 * Rewrite the bootblock, which is BBSIZE bytes from the start of the disk.
 * We punch down the disklabel where we expect it to be before writing.
 */
static int
g_bsd_diocbsdbb(dev_t dev, u_long cmd __unused, caddr_t data, int fflag __unused, struct thread *td __unused)
{
	struct g_geom *gp;
	struct g_slicer *gsp;
	struct g_bsd_softc *ms;
	struct g_consumer *cp;
	u_char *buf;
	void *p;
	int error, i;
	uint64_t sum;

	/* Get hold of the interesting bits from the bio. */
	gp = (void *)dev;
	gsp = gp->softc;
	ms = gsp->softc;

	/* The disklabel to set is the ioctl argument. */
	buf = g_malloc(BBSIZE, M_WAITOK);
	p = *(void **)data;
	error = copyin(p, buf, BBSIZE);
	if (!error) {
		DROP_GIANT();
		g_topology_lock();
		/* Validate and modify our slice instance to match. */
		error = g_bsd_modify(gp, buf + ms->labeloffset);
		if (!error) {
			cp = LIST_FIRST(&gp->consumer);
			if (ms->labeloffset == ALPHA_LABEL_OFFSET) {
				sum = 0;
				for (i = 0; i < 63; i++)
					sum += le64dec(buf + i * 8);
				le64enc(buf + 504, sum);
			}
			error = g_write_data(cp, 0, buf, BBSIZE);
		}
		g_topology_unlock();
		PICKUP_GIANT();
	}
	g_free(buf);
	return (error);
}

/*
 * If the user tries to overwrite our disklabel through an open partition
 * or via a magicwrite config call, we end up here and try to prevent
 * footshooting as best we can.
 */
static void
g_bsd_hotwrite(void *arg, int flag)
{
	struct bio *bp;
	struct g_geom *gp;
	struct g_slicer *gsp;
	struct g_slice *gsl;
	struct g_bsd_softc *ms;
	u_char *p;
	int error;
	
	g_topology_assert();
	/*
	 * We should never get canceled, because that would amount to a removal
	 * of the geom while there was outstanding I/O requests.
	 */
	KASSERT(flag != EV_CANCEL, ("g_bsd_hotwrite cancelled"));
	bp = arg;
	gp = bp->bio_to->geom;
	gsp = gp->softc;
	ms = gsp->softc;
	gsl = &gsp->slices[bp->bio_to->index];
	p = (u_char*)bp->bio_data + ms->labeloffset 
	    - (bp->bio_offset + gsl->offset);
	error = g_bsd_modify(gp, p);
	if (error) {
		g_io_deliver(bp, EPERM);
		return;
	}
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
	case BIO_GETATTR:
		if (g_handleattr(bp, "BSD::labelsum", ms->labelsum,
		    sizeof(ms->labelsum)))
			return (1);
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
		bsd_disklabel_le_dec(ms->label, gio->data, MAXPARTITIONS);
		g_io_deliver(bp, 0);
		return (1);
	case DIOCBSDBB:
		gio->func = g_bsd_diocbsdbb;
		gio->dev = (void *)gp;
		g_io_deliver(bp, EDIRIOCTL);
		return (1);
	case DIOCSDINFO:
	case DIOCWDINFO:
		/*
		 * These we cannot do without the topology lock and some
		 * some I/O requests.  Ask the event-handler to schedule
		 * us in a less restricted environment.
		 */
		error = g_post_event(g_bsd_ioctl, bp, M_NOWAIT, gp, NULL);
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
	} else if (pp != NULL) {
		if (indent == NULL)
			sbuf_printf(sb, " ty %d",
			    ms->ondisk.d_partitions[pp->index].p_fstype);
		else
			sbuf_printf(sb, "%s<type>%d</type>\n", indent,
			    ms->ondisk.d_partitions[pp->index].p_fstype);
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
	u_int secsize;
	struct g_slicer *gsp;
	u_char hash[16];
	MD5_CTX md5sum;

	g_trace(G_T_TOPOLOGY, "bsd_taste(%s,%s)", mp->name, pp->name);
	g_topology_assert();

	/* We don't implement transparent inserts. */
	if (flags == G_TF_TRANSPARENT)
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
		 * attach to any other type.
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
		 * In order to avoid recursively attaching to the same
		 * on-disk label (it's usually visible through the 'c'
		 * partition) we calculate an MD5 and ask if other BSD's
		 * below us love that label.  If they do, we don't.
		 */
		MD5Init(&md5sum);
		MD5Update(&md5sum, ms->label, sizeof(ms->label));
		MD5Final(ms->labelsum, &md5sum);

		error = g_getattr("BSD::labelsum", cp, &hash);
		if (!error && !bcmp(ms->labelsum, hash, sizeof(hash)))
			break;

		/*
		 * Process the found disklabel, and modify our "slice"
		 * instance to match it, if possible.
		 */
		error = g_bsd_modify(gp, ms->label);
	} while (0);

	/* Success or failure, we can close our provider now. */
	error = g_access_rel(cp, -1, 0, 0);

	/* If we have configured any providers, return the new geom. */
	if (gsp->nprovider > 0) {
		g_slice_conf_hot(gp, 0, ms->labeloffset, LABELSIZE,
		    G_SLICE_HOT_ALLOW, G_SLICE_HOT_DENY, G_SLICE_HOT_CALL);
		gsp->hot = g_bsd_hotwrite;
		return (gp);
	}
	/*
	 * ...else push the "self-destruct" button, by spoiling our own
	 * consumer.  This triggers a call to g_slice_spoiled which will
	 * dismantle what was setup.
	 */
	g_slice_spoiled(cp);
	return (NULL);
}

struct h0h0 {
	struct g_geom *gp;
	struct g_bsd_softc *ms;
	u_char *label;
	int error;
};

static void
g_bsd_callconfig(void *arg, int flag)
{
	struct h0h0 *hp;

	hp = arg;
	hp->error = g_bsd_modify(hp->gp, hp->label);
	if (!hp->error)
		hp->error = g_bsd_writelabel(hp->gp, NULL);
}

/*
 * NB! curthread is user process which GCTL'ed.
 */
static int
g_bsd_config(struct gctl_req *req, struct g_geom *gp, const char *verb)
{
	u_char *label;
	int error;
	struct h0h0 h0h0;
	struct g_slicer *gsp;
	struct g_consumer *cp;
	struct g_bsd_softc *ms;

	g_topology_assert();
	cp = LIST_FIRST(&gp->consumer);
	gsp = gp->softc;
	ms = gsp->softc;
	if (!strcmp(verb, "read mbroffset")) {
		error = gctl_set_param(req, "mbroffset",
		    &ms->mbroffset, sizeof(ms->mbroffset));
		return (error);
	} else if (!strcmp(verb, "write label")) {
		label = gctl_get_paraml(req, "label", LABELSIZE);
		if (label == NULL)
			return (EINVAL);
		h0h0.gp = gp;
		h0h0.ms = gsp->softc;
		h0h0.label = label;
		h0h0.error = -1;
		/* XXX: Does this reference register with our selfdestruct code ? */
		error = g_access_rel(cp, 1, 1, 1);
		if (error) {
			g_free(label);
			return (error);
		}
		g_topology_unlock();
		g_waitfor_event(g_bsd_callconfig, &h0h0, M_WAITOK, gp, NULL);
		g_topology_lock();
		error = h0h0.error;
		g_access_rel(cp, -1, -1, -1);
		g_free(label);
	} else if (!strcmp(verb, "write bootcode")) {
		label = gctl_get_paraml(req, "bootcode", BBSIZE);
		if (label == NULL)
			return (EINVAL);
		/* XXX: Does this reference register with our selfdestruct code ? */
		error = g_access_rel(cp, 1, 1, 1);
		if (error) {
			g_free(label);
			return (error);
		}
		error = g_bsd_writelabel(gp, label);
		g_access_rel(cp, -1, -1, -1);
		g_free(label);
	} else {
		return (gctl_error(req, "Unknown verb parameter"));
	}

	return (error);
}

/* Finally, register with GEOM infrastructure. */
static struct g_class g_bsd_class = {
	.name = BSD_CLASS_NAME,
	.taste = g_bsd_taste,
	.config_geom = g_bsd_config,
};

DECLARE_GEOM_CLASS(g_bsd_class, g_bsd);
