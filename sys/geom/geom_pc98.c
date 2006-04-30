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
#include <sys/endian.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/fcntl.h>
#include <sys/malloc.h>
#include <sys/bio.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <sys/diskpc98.h>
#include <geom/geom.h>
#include <geom/geom_slice.h>

#define PC98_CLASS_NAME "PC98"

struct g_pc98_softc {
	u_int fwsectors, fwheads, sectorsize;
	int type[NDOSPART];
	u_char sec[8192];
};

static void
g_pc98_print(int i, struct pc98_partition *dp)
{
	char sname[17];

	strncpy(sname, dp->dp_name, 16);
	sname[16] = '\0';

	hexdump(dp, sizeof(dp[0]), NULL, 0);
	printf("[%d] mid:%d(0x%x) sid:%d(0x%x)",
	       i, dp->dp_mid, dp->dp_mid, dp->dp_sid, dp->dp_sid);
	printf(" s:%d/%d/%d", dp->dp_scyl, dp->dp_shd, dp->dp_ssect);
	printf(" e:%d/%d/%d", dp->dp_ecyl, dp->dp_ehd, dp->dp_esect);
	printf(" sname:%s\n", sname);
}

/*
 * XXX: Add gctl_req arg and give good error msgs.
 * XXX: Check that length argument does not bring boot code inside any slice.
 */
static int
g_pc98_modify(struct g_geom *gp, struct g_pc98_softc *ms, u_char *sec, int len __unused)
{
	int i, error;
	off_t s[NDOSPART], l[NDOSPART];
	struct pc98_partition dp[NDOSPART];

	g_topology_assert();
	
	if (sec[0x1fe] != 0x55 || sec[0x1ff] != 0xaa)
		return (EBUSY);

#if 0
	/*
	 * By convetion, it seems that the ipl program has a jump at location
	 * 0 to the real start of the boot loader.  By convetion, it appears
	 * that after this jump, there's a string, terminated by at last one,
	 * if not more, zeros, followed by the target of the jump.  FreeBSD's
	 * pc98 boot0 uses 'IPL1' followed by 3 zeros here, likely for
	 * compatibility with some older boot loader.  Linux98's boot loader
	 * appears to use 'Linux 98' followed by only two.  GRUB/98 appears to
	 * use 'GRUB/98 ' followed by none.  These last two appear to be
	 * ported from the ia32 versions, but appear to show similar
	 * convention.  Grub/98 has an additional NOP after the jmp, which
	 * isn't present in others.
	 *
	 * The following test was inspired by looking only at partitions
	 * with FreeBSD's boot0 (or one that it is compatible with).  As
	 * such, if failed when other IPL programs were used.
	 */
	if (sec[4] != 'I' || sec[5] != 'P' || sec[6] != 'L' || sec[7] != '1')
		return (EBUSY);
#endif

	for (i = 0; i < NDOSPART; i++)
		pc98_partition_dec(
			sec + 512 + i * sizeof(struct pc98_partition), &dp[i]);

	for (i = 0; i < NDOSPART; i++) {
		/* If start and end are identical it's bogus */
		if (dp[i].dp_ssect == dp[i].dp_esect &&
		    dp[i].dp_shd == dp[i].dp_ehd &&
		    dp[i].dp_scyl == dp[i].dp_ecyl)
			s[i] = l[i] = 0;
		else if (dp[i].dp_ecyl == 0)
			s[i] = l[i] = 0;
		else {
			s[i] = (off_t)dp[i].dp_scyl *
				ms->fwsectors * ms->fwheads * ms->sectorsize;
			l[i] = (off_t)(dp[i].dp_ecyl - dp[i].dp_scyl + 1) *
				ms->fwsectors * ms->fwheads * ms->sectorsize;
		}
		if (bootverbose) {
			printf("PC98 Slice %d on %s:\n", i + 1, gp->name);
			g_pc98_print(i, dp + i);
		}
		if (s[i] < 0 || l[i] < 0)
			error = EBUSY;
		else
			error = g_slice_config(gp, i, G_SLICE_CONFIG_CHECK,
				       s[i], l[i], ms->sectorsize,
				       "%ss%d", gp->name, i + 1);
		if (error)
			return (error);
	}

	for (i = 0; i < NDOSPART; i++) {
		ms->type[i] = (dp[i].dp_sid << 8) | dp[i].dp_mid;
		g_slice_config(gp, i, G_SLICE_CONFIG_SET, s[i], l[i],
			       ms->sectorsize, "%ss%d", gp->name, i + 1);
	}

	bcopy(sec, ms->sec, sizeof (ms->sec));

	return (0);
}

static int
g_pc98_ioctl(struct g_provider *pp, u_long cmd, void *data, int fflag, struct thread *td)
{
	struct g_geom *gp;
	struct g_pc98_softc *ms;
	struct g_slicer *gsp;
	struct g_consumer *cp;
	int error, opened;

	gp = pp->geom;
	gsp = gp->softc;
	ms = gsp->softc;

	opened = 0;
	error = 0;
	switch(cmd) {
	case DIOCSPC98: {
		if (!(fflag & FWRITE))
			return (EPERM);
		DROP_GIANT();
		g_topology_lock();
		cp = LIST_FIRST(&gp->consumer);
		if (cp->acw == 0) {
			error = g_access(cp, 0, 1, 0);
			if (error == 0)
				opened = 1;
		}
		if (!error)
			error = g_pc98_modify(gp, ms, data, 8192);
		if (!error)
			error = g_write_data(cp, 0, data, 8192);
		if (opened)
			g_access(cp, 0, -1 , 0);
		g_topology_unlock();
		PICKUP_GIANT();
		return(error);
	}
	default:
		return (ENOIOCTL);
	}
}

static int
g_pc98_start(struct bio *bp)
{
	struct g_provider *pp;
	struct g_geom *gp;
	struct g_pc98_softc *mp;
	struct g_slicer *gsp;
	int idx;

	pp = bp->bio_to;
	idx = pp->index;
	gp = pp->geom;
	gsp = gp->softc;
	mp = gsp->softc;
	if (bp->bio_cmd == BIO_GETATTR) {
		if (g_handleattr_int(bp, "PC98::type", mp->type[idx]))
			return (1);
		if (g_handleattr_off_t(bp, "PC98::offset",
				       gsp->slices[idx].offset))
			return (1);
	}

	return (0);
}

static void
g_pc98_dumpconf(struct sbuf *sb, const char *indent, struct g_geom *gp,
		struct g_consumer *cp __unused, struct g_provider *pp)
{
	struct g_pc98_softc *mp;
	struct g_slicer *gsp;
	struct pc98_partition dp;
	char sname[17];

	gsp = gp->softc;
	mp = gsp->softc;
	g_slice_dumpconf(sb, indent, gp, cp, pp);
	if (pp != NULL) {
		pc98_partition_dec(
			mp->sec + 512 +
			pp->index * sizeof(struct pc98_partition), &dp);
		strncpy(sname, dp.dp_name, 16);
		sname[16] = '\0';
		if (indent == NULL) {
			sbuf_printf(sb, " ty %d", mp->type[pp->index]);
			sbuf_printf(sb, " sn %s", sname);
		} else {
			sbuf_printf(sb, "%s<type>%d</type>\n", indent,
				    mp->type[pp->index]);
			sbuf_printf(sb, "%s<sname>%s</sname>\n", indent,
				    sname);
		}
	}
}

static struct g_geom *
g_pc98_taste(struct g_class *mp, struct g_provider *pp, int flags)
{
	struct g_geom *gp;
	struct g_consumer *cp;
	int error;
	struct g_pc98_softc *ms;
	u_int fwsectors, fwheads, sectorsize;
	u_char *buf;

	g_trace(G_T_TOPOLOGY, "g_pc98_taste(%s,%s)", mp->name, pp->name);
	g_topology_assert();
	if (flags == G_TF_NORMAL &&
	    !strcmp(pp->geom->class->name, PC98_CLASS_NAME))
		return (NULL);
	gp = g_slice_new(mp, NDOSPART, pp, &cp, &ms, sizeof *ms, g_pc98_start);
	if (gp == NULL)
		return (NULL);
	g_topology_unlock();
	do {
		if (gp->rank != 2 && flags == G_TF_NORMAL)
			break;
		error = g_getattr("GEOM::fwsectors", cp, &fwsectors);
		if (error || fwsectors == 0) {
			fwsectors = 17;
			if (bootverbose)
				printf("g_pc98_taste: guessing %d sectors\n",
				    fwsectors);
		}
		error = g_getattr("GEOM::fwheads", cp, &fwheads);
		if (error || fwheads == 0) {
			fwheads = 8;
			if (bootverbose)
				printf("g_pc98_taste: guessing %d heads\n",
				    fwheads);
		}
		sectorsize = cp->provider->sectorsize;
		if (sectorsize % 512 != 0)
			break;
		buf = g_read_data(cp, 0, 8192, NULL);
		if (buf == NULL)
			break;
		ms->fwsectors = fwsectors;
		ms->fwheads = fwheads;
		ms->sectorsize = sectorsize;
		g_topology_lock();
		g_pc98_modify(gp, ms, buf, 8192);
		g_topology_unlock();
		g_free(buf);
		break;
	} while (0);
	g_topology_lock();
	g_access(cp, -1, 0, 0);
	if (LIST_EMPTY(&gp->provider)) {
		g_slice_spoiled(cp);
		return (NULL);
	}
	return (gp);
}

static void
g_pc98_config(struct gctl_req *req, struct g_class *mp, const char *verb)
{
	struct g_geom *gp;
	struct g_consumer *cp;
	struct g_pc98_softc *ms;
	struct g_slicer *gsp;
	int opened = 0, error = 0;
	void *data;
	int len;

	g_topology_assert();
	gp = gctl_get_geom(req, mp, "geom");
	if (gp == NULL)
		return;
	if (strcmp(verb, "write PC98")) {
		gctl_error(req, "Unknown verb");
		return;
	}
	gsp = gp->softc;
	ms = gsp->softc;
	data = gctl_get_param(req, "data", &len);
	if (data == NULL)
		return;
	if (len < 8192 || (len % 512)) {
		gctl_error(req, "Wrong request length");
		return;
	}
	cp = LIST_FIRST(&gp->consumer);
	if (cp->acw == 0) {
		error = g_access(cp, 0, 1, 0);
		if (error == 0)
			opened = 1;
	}
	if (!error)
		error = g_pc98_modify(gp, ms, data, len);
	if (error)
		gctl_error(req, "conflict with open slices");
	if (!error)
		error = g_write_data(cp, 0, data, len);
	if (error)
		gctl_error(req, "sector zero write failed");
	if (opened)
		g_access(cp, 0, -1 , 0);
	return;
}

static struct g_class g_pc98_class = {
	.name = PC98_CLASS_NAME,
	.version = G_VERSION,
	.taste = g_pc98_taste,
	.dumpconf = g_pc98_dumpconf,
	.ctlreq = g_pc98_config,
	.ioctl = g_pc98_ioctl,
};

DECLARE_GEOM_CLASS(g_pc98_class, g_pc98);
