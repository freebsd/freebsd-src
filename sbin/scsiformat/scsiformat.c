/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)scsiformat.c	5.5 (Berkeley) 4/2/94
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1992, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)scsiformat.c	5.5 (Berkeley) 4/2/94";
#endif /* not lint */

#include <sys/param.h>
#include <sys/ioctl.h>

#include <dev/scsi/scsi.h>
#include <dev/scsi/disk.h>
#include <dev/scsi/disktape.h>
#include <dev/scsi/scsi_ioctl.h>

#define COMPAT_HPSCSI

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int fd;
char *device;

void	scsi_str __P((char *, char *, int));
void	do_command __P((int, struct scsi_cdb *, void *, int));
void	do_format __P((void));
void	print_capacity __P((void));
void	print_inquiry __P((void));
void	prflags __P((int, const char *));
u_char *print_mode_page __P((u_char *));
void	print_mode_sense __P((void));
void	usage __P((void));

#define	N2(c, d)	(((c) << 8) | (d))
#define	N3(b, c, d)	(((b) << 16) | N2(c, d))
#define	N4(a, b, c, d)	(((a) << 24) | N3(b, c, d))

int	sense_pctl;

int
main(argc, argv)
	int argc;
	char *argv[];
{
	extern char *optarg;
	int ch, readonly;

	readonly = 0;
	sense_pctl = SCSI_MSENSE_PCTL_CUR;
	while ((ch = getopt(argc, argv, "rp:")) != EOF) {
		switch(ch) {
		case 'r':
			readonly = 1;
			break;
		case 'p':		/* mode sense page control */
			switch (*optarg) {
			case 'c':
				sense_pctl = SCSI_MSENSE_PCTL_CUR;
				break;
			case 'd':
				sense_pctl = SCSI_MSENSE_PCTL_DFLT;
				break;
			case 's':
				sense_pctl = SCSI_MSENSE_PCTL_SAVED;
				break;
			case 'v':
				(void)printf(
	"*** note: for variable parameters, 1-bit means ``can write here''\n");
				sense_pctl = SCSI_MSENSE_PCTL_VAR;
				break;
			}
			/* FALLTHROUGH */
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage();

	device = *argv;
	fd = open(device, readonly ? O_RDONLY : O_RDWR, 0);
	if (fd < 0) {
		(void)fprintf(stderr,
		    "scsiformat: %s: %s\n", device, strerror(errno));
		exit(1);
	}
	print_inquiry();
	print_capacity();
	print_mode_sense();

	if (!readonly)
		do_format();
	exit(0);
}

/*
 * Copy a counted string, trimming trailing blanks, and turning the
 * result into a C-style string.
 */
void
scsi_str(src, dst, len)
	register char *src, *dst;
	register int len;
{

	while (src[len - 1] == ' ') {
		if (--len == 0) {
			*dst = 0;
			return;
		}
	}
	bcopy(src, dst, len);
	dst[len] = 0;
}

void
print_inquiry()
{
	register struct scsi_inq_ansi *si;
	int ver;
	struct scsi_inquiry inqbuf;
	char vendor[10], product[17], rev[5];
	static struct scsi_cdb inq = {
		CMD_INQUIRY, 0, 0, 0, sizeof(inqbuf), 0
	};

	do_command(fd, &inq, &inqbuf, sizeof(inqbuf));
	(void)printf("%s: ", device);

	ver = (inqbuf.si_version >> VER_ANSI_SHIFT) & VER_ANSI_MASK;
	if (ver != 1 && ver != 2) {
		(void)printf("type 0x%x, qual 0x%x, ver 0x%x (ansi %d)\n",
		    inqbuf.si_type, inqbuf.si_qual, inqbuf.si_version, ver);
		return;
	}
	si = (struct scsi_inq_ansi *)&inqbuf;
	switch (si->si_type & TYPE_TYPE_MASK) {

	case TYPE_DAD:
		(void)printf("(disk)");
		break;

	case TYPE_WORM:
		(void)printf("(WORM)");
		break;

	case TYPE_ROM:
		(void)printf("(CD-ROM)");
		break;

	case TYPE_MO:
		(void)printf("(MO-DISK)");
		break;

	case TYPE_JUKEBOX:
		(void)printf("(jukebox)");
		break;

	default:
		(void)printf("(??)");
		break;
	}
	scsi_str(si->si_vendor, vendor, sizeof(si->si_vendor));
	scsi_str(si->si_product, product, sizeof(si->si_product));
	scsi_str(si->si_rev, rev, sizeof(si->si_rev));
	(void)printf(" %s %s rev %s:", vendor, product, rev);
}

void
print_capacity()
{
	struct scsi_rc rc;		/* for READ CAPACITY */
	static struct scsi_cdb cap = { CMD_READ_CAPACITY };

	do_command(fd, &cap, &rc, sizeof(rc));
	(void)printf(" %d blocks of %d bytes each\n",
	    N4(rc.rc_lbah, rc.rc_lbahm, rc.rc_lbalm, rc.rc_lbal) + 1,
	    N4(rc.rc_blh, rc.rc_blhm, rc.rc_bllm, rc.rc_bll));
}

void
print_mode_sense()
{
	register u_char *cp, *ep;
	register struct scsi_ms_bd *bd;
	register int n, i, l, len, bdlen;
#ifdef TEN_BYTE_SENSE
	struct {
		struct	scsi_ms10 ms;
		u_char	p[1023 - sizeof(struct scsi_ms10)];
	} msbuf;
	static struct scsi_cdb modesense = {
		CMD_MODE_SENSE10, SCSI_MSENSE_DBD, 0, 0, 0, 0, 0,
		sizeof(msbuf) >> 8, sizeof (msbuf), 0
	};

	CDB10(&modesense)->cdb_lbam = sense_pctl | SCSI_MS_PC_ALL;
	do_command(fd, &modesense, &msbuf, sizeof(msbuf));
	len = N2(msbuf.ms.ms_lenh, msbuf.ms.ms_lenl);
	bdlen = N2(msbuf.ms.ms_bdlh, msbuf.ms.ms_bdll);
#else
	struct {
		struct	scsi_ms6 ms;
		u_char	p[255 - sizeof(struct scsi_ms6)];
	} msbuf;
	static struct scsi_cdb modesense = {
		CMD_MODE_SENSE6, 0, 0, 0, sizeof(msbuf), 0
	};

	CDB6(&modesense)->cdb_lbam = sense_pctl | SCSI_MS_PC_ALL;
	do_command(fd, &modesense, &msbuf, sizeof(msbuf));
	len = msbuf.ms.ms_len;
	bdlen = msbuf.ms.ms_bdl;
#endif
	(void)printf("\n%d bytes of mode sense data. ", len);
	(void)printf("medium type 0x%x, %swrite protected\n",
	    msbuf.ms.ms_mt, msbuf.ms.ms_dsp & SCSI_MS_DSP_WP ? "" : "not ");
	if ((n = bdlen) != 0) {
		bd = (struct scsi_ms_bd *)msbuf.p;
		for (n /= sizeof(*bd); --n >= 0; bd++) {
			(void)printf("\tdensity code 0x%x, ", bd->bd_dc);
			i = N3(bd->bd_nbh, bd->bd_nbm, bd->bd_nbl);
			l = N3(bd->bd_blh, bd->bd_blm, bd->bd_bll);
			if (i)
				(void)printf("%d blocks of length %d\n", i, l);
			else
				(void)printf("all blocks of length %d\n", l);
		}
	}
	/*
	 * Sense header lengths includes the sense header, while mode page
	 * lengths do not ... let's hear it for consistency!
	 */
	cp = msbuf.p + bdlen;
	ep = msbuf.p + len - sizeof(msbuf.ms);
	while (cp < ep)
		cp = print_mode_page(cp);
}

void
prflags(v, cp)
	int v;
	register const char *cp;
{
	register const char *np;
	char f, sep;

	for (sep = '<'; (f = *cp++) != 0; cp = np) {
		for (np = cp; *np >= ' ';)
			np++;
		if ((v & (1 << (f - 1))) == 0)
			continue;
		printf("%c%.*s", sep, np - cp, cp);
		sep = ',';
	}
	if (sep != '<')
		putchar('>');
}

static char *
cache_policy(x)
	int x;
{
	static char rsvd[30];

	switch (x) {

	case SCSI_CACHE_DEFAULT:
		return ("default");

	case SCSI_CACHE_KEEPPF:
		return ("toss cmd data, save prefetch");

	case SCSI_CACHE_KEEPCMD:
		return ("toss prefetch data, save cmd");

	default:
		(void)sprintf(rsvd, "reserved %d", x);
		return (rsvd);
	}
	/* NOTREACHED */
}

u_char *
print_mode_page(cp)
	u_char *cp;
{
	register struct scsi_ms_page_hdr *mp;
	int len, code, i;
	u_char *tp;
	const char *s;

	mp = (struct scsi_ms_page_hdr *)cp;
	code = mp->mp_psc & SCSI_MS_PC_MASK;
	len = mp->mp_len;
	(void)printf("\npage type %d%s (%d bytes): ",
	    code, mp->mp_psc & SCSI_MS_MP_SAVEABLE ? " (saveable)" : "", len);
	switch (code) {

	case SCSI_MS_PC_RWERRREC:
#define	rw ((struct scsi_page_rwerrrec *)(mp + 1))
		(void)printf("Read/Write Error Recovery parameters.\n");
		(void)printf("\tflags = 0x%x", rw->rw_flags);
		prflags(rw->rw_flags,
		    "\10AWRE\7ARRE\6TB\5RC\4EER\3PER\2DTE\1DCR");
		(void)printf(",\n\t%d read retries, %d correction span bits,\n",
		    rw->rw_read_retry, rw->rw_corr_span);
		(void)printf("\t%d head offsets, %d data strobe offsets%s\n",
		    rw->rw_hd_off, rw->rw_ds_off, len > 6 ? "," : ".");
		if (len <= 6)
			break;
		(void)printf("\t%d write retries, ", rw->rw_write_retry);
		i = N2(rw->rw_rtlh, rw->rw_rtll);
		if (i != 0xffff)
			(void)printf("%d", i);
		else
			(void)printf("no");
		(void)printf(" recovery time limit.\n");
		break;
#undef rw

	case SCSI_MS_PC_DR:
#define	dr ((struct scsi_page_dr *)(mp + 1))
		(void)printf("Disconnect/Reconnect control.\n");
		(void)printf("\tbuffer full ratio %d, buffer empty ratio %d,\n",
		    dr->dr_full, dr->dr_empty);
		(void)printf("\ttime limits: %d bus inactivity, ",
		    N2(dr->dr_inacth, dr->dr_inactl));
		(void)printf("%d disconnect, %d connect.\n",
		    N2(dr->dr_disconh, dr->dr_disconl),
		    N2(dr->dr_conh, dr->dr_conl));
		(void)printf("\tmaximum burst size %d,\n",
		    N2(dr->dr_bursth, dr->dr_burstl));
		switch (dr->dr_dtdc & SCSI_DR_DTDC_MASK) {
		case SCSI_DR_DTDC_NONE:
			s = "never";
			break;
		case SCSI_DR_DTDC_NOTDATA:
			s = "during data transfer";
			break;
		case SCSI_DR_DTDC_RSVD:
			s = "???";
			break;
		case SCSI_DR_DTDC_NOTD2:
			s = "during and after data transfer";
			break;
		}
		(void)printf("\tsuppress disconnect %s.\n", s);
		break;
#undef dr

	case SCSI_MS_PC_FMT:
#define	fmt ((struct scsi_page_fmt *)(mp + 1))
		(void)printf("Format parameters.\n");
		(void)printf("\t%d tracks/zone, %d alt.sect./zone, ",
		    N2(fmt->fmt_tpzh, fmt->fmt_tpzl),
		    N2(fmt->fmt_aspzh, fmt->fmt_aspzl));
		(void)printf("%d alt.tracks/zone,\n\t%d alt.tracks/vol., ",
		    N2(fmt->fmt_atpzh, fmt->fmt_atpzl),
		    N2(fmt->fmt_atpvh, fmt->fmt_atpvl));
		(void)printf("%d sectors/track, %d bytes/phys.sector,\n",
		    N2(fmt->fmt_spth, fmt->fmt_sptl),
		    N2(fmt->fmt_dbppsh, fmt->fmt_dbppsl));
		(void)printf("\tinterleave %d, track skew %d, cyl.skew %d,\n",
		    N2(fmt->fmt_ilh, fmt->fmt_ill),
		    N2(fmt->fmt_tsfh, fmt->fmt_tsfl),
		    N2(fmt->fmt_csfh, fmt->fmt_csfl));
		(void)printf("\tdrive flags 0x%x", fmt->fmt_flags);
		prflags(fmt->fmt_flags, "\10SSEC\7HSEC\6RMB\5SURF");
		(void)printf(".\n");
		break;
#undef fmt

	case SCSI_MS_PC_RDGEOM:
#define rd ((struct scsi_page_rdgeom *)(mp + 1))
		(void)printf("Disk Geometry parameters.\n");
		(void)printf("\t%d cylinders, %d heads,\n",
		    N3(rd->rd_ncylh, rd->rd_ncylm, rd->rd_ncyll),
		    rd->rd_nheads);
		(void)printf("\tstart write precompensation at cyl %d,\n",
		    N3(rd->rd_wpcylh, rd->rd_wpcylm, rd->rd_wpcyll));
		(void)printf("\tstart reduced write current at cyl %d,\n",
		    N3(rd->rd_rwcylh, rd->rd_rwcylm, rd->rd_rwcyll));
		(void)printf("\tseek step rate %f us, landing zone cyl %d,\n",
		    N2(rd->rd_steph, rd->rd_stepl) * 0.1,
		    N3(rd->rd_lcylh, rd->rd_lcylm, rd->rd_lcyll));
		switch (rd->rd_rpl & SCSI_RD_RPL_MASK) {
		case SCSI_RD_RPL_NONE:
			s = "disabled or unsupported";
			break;
		case SCSI_RD_RPL_SLAVE:
			s = "slave";
			break;
		case SCSI_RD_RPL_MASTER:
			s = "master";
			break;
		case SCSI_RD_RPL_MCONTROL:
			s = "master control";
			break;
		}
		(void)printf("\trotational synch %s, offset %d/256%s\n",
		    s, rd->rd_roff, len > 18 ? "," : ".");
		if (len > 18)
			(void)printf("\trotation %d rpm.\n",
			    N2(rd->rd_rpmh, rd->rd_rpml));
		break;
#undef rd

	case SCSI_MS_PC_VERRREC:
#define	v ((struct scsi_page_verrrec *)(mp + 1))
		(void)printf("Verify Error Recovery parameters.\n");
		(void)printf("\tflags = 0x%x", v->v_flags);
		prflags(v->v_flags, "\4EER\3PER\2DTE\1DCR");
		(void)printf(",\n\t%d verify retries, %d %s span bits,\n\t",
		    v->v_verify_retry, v->v_corr_span, "correction");
		(void)printf("%d recovery time limit.\n",
		    N2(v->v_rtlh, v->v_rtll));
		break;
#undef v

	case SCSI_MS_PC_CACHE:
#define cache ((struct scsi_page_cache *)(mp + 1))
		(void)printf("Caching Page.\n");
		(void)printf("\tflags = 0x%x", cache->cache_flags);
		prflags(cache->cache_flags, "\3WCE\2MF\1RCD");
		(void)printf(
		    ",\n\tread retention = %s, write retention = %s,\n",
		    cache_policy(SCSI_CACHE_RDPOLICY(cache->cache_reten)),
		    cache_policy(SCSI_CACHE_WRPOLICY(cache->cache_reten)));
		(void)printf("\tdisable prefetch transfer length = %d,\n",
		    N2(cache->cache_dptlh, cache->cache_dptll));
		(void)printf("\tmin prefetch = %d, max prefetch = %d, ",
		    N2(cache->cache_minpfh, cache->cache_minpfl),
		    N2(cache->cache_maxpfh, cache->cache_maxpfl));
		(void)printf("max prefetch ceiling = %d.\n",
		    N2(cache->cache_mpch, cache->cache_mpcl));
		break;
#undef cache

	case SCSI_MS_PC_CTLMODE:
#define	cm ((struct scsi_page_ctlmode *)(mp + 1))
		(void)printf("Control Mode Page.\n");
		(void)printf("\t%s report log-activity error conditions,\n",
		    cm->cm_rlec & SCSI_CM_RLEC ? "do" : "do not");
		(void)printf("\tqueue algorithm modifier = %d, flags = 0x%x",
		    SCSI_CM_QMOD(cm->cm_qctl),
		    cm->cm_qctl & (SCSI_CM_QERR|SCSI_CM_DQUE));
		prflags(cm->cm_qctl, "\2QERR\1DQUE");
		(void)printf(",\n\tECA/AEN flags = 0x%x", cm->cm_ecaaen);
		prflags(cm->cm_ecaaen, "\10ECA\3RAENP\2UUAENP\1EAENP");
		(void)printf(", AEN holdoff period = %d ms.\n",
		    N2(cm->cm_aenholdh, cm->cm_aenholdl));
		break;
#undef cm

	/*
	 * Vendor Unique, but what the heck.
	 */
	case SCSI_MS_PC_CDCCACHECTL:
#define	ccm ((struct scsi_page_CDCcachectlmode *)(mp + 1))
		(void)printf("CDC-specific Cache Control Mode Page.\n");
		(void)printf("\tflags = 0x%x", ccm->ccm_flags);
		prflags(ccm->ccm_flags, "\7WIE\5ENABLE");
		(void)printf(", table size = %d, prefetch threshold = %d\n",
		    SCSI_CDC_CCM_TBLSZ(ccm->ccm_flags),
		    ccm->ccm_pfthresh);
		(void)printf("\tmaximum %s = %d, maximum %s = %d,\n",
		    "threshold", ccm->ccm_maxthresh,
		    "prefetch multiplier", ccm->ccm_maxpfmult);
		(void)printf("\tminimum %s = %d, minimum %s = %d.\n",
		    "threshold", ccm->ccm_minthresh,
		    "prefetch multiplier", ccm->ccm_minpfmult);
		break;
#undef ccm

	default:
		(void)printf("Unknown page type.");
		for (tp = cp + sizeof(*mp), i = 0; i < len; ++i) {
			if ((i & 7) == 0)
				(void)printf("\n\t%2d: ", i);
			(void)printf(" %02x", *tp++);
		}
		(void)printf(".\n");
		break;
	}
	return (cp + sizeof(*mp) + len);
}

void
pr_sense(fd)
	int fd;
{
	static struct scsi_fmt_sense s;
	register struct scsi_sense *sn;

	if (ioctl(fd, SDIOCSENSE, &s) < 0)
		(void)fprintf(stderr,
		    "scsiformat: SDIOCSENSE: %s\n", strerror(errno));

	(void)printf("scsi status 0x%x", s.status);
	if (s.status & STS_CHECKCOND) {
		sn = (struct scsi_sense *)s.sense;

		(void)printf(" sense class %d, code %d",
		    SENSE_ECLASS(sn), SENSE_ECODE(sn));
		if (SENSE_ISXSENSE(sn)) {
			(void)printf(", key %d", XSENSE_KEY(sn));
			if (XSENSE_IVALID(sn))
				(void)printf(", blk %d", XSENSE_INFO(sn));
		}
	}
	(void)printf("\n");
}

void
do_format()
{
	struct {
		struct scsi_ms6 ms;		/* mode select header */
		struct scsi_ms_bd bd;		/* block descriptor */
		struct scsi_ms_page_hdr mp;	/* ctl mode page hdr */
		struct scsi_page_ctlmode cm;	/* ctl mode page */
		u_char pad[4];			/* ??? */
	} msel;
	u_char fmtbuf[128];
	static struct scsi_cdb modeselect = {
		CMD_MODE_SELECT6,
		SCSI_MSEL_SCSI2_DATA | SCSI_MSEL_SAVEPAGES, 0, 0,
		sizeof(msel), 0
	};
	static struct scsi_cdb format = { CMD_FORMAT_UNIT };

	/* want mostly 0s; set them all zero here */
	bzero(&msel, sizeof(msel));

	/* one block descriptor */
	msel.ms.ms_bdl = sizeof(struct scsi_ms_bd);

	/* block length = 512 bytes */
	msel.bd.bd_blm = 512 / 256;
	msel.bd.bd_bll = 512 % 256;

	/*
	 * In the following, the mystery pad region is copied from
	 * the original driver.  I have no idea what it is for.
	 * (Anyone got SCSI-2 documents?)
	 */

	/* mode page parameters: report log-activity exception conditions */
	msel.mp.mp_psc = SCSI_MS_PC_CTLMODE;
	msel.mp.mp_len = sizeof(msel.cm) + sizeof(msel.pad);
	msel.cm.cm_rlec = SCSI_CM_RLEC;

	do_command(fd, &modeselect, &msel, sizeof(msel));

	bzero(fmtbuf, sizeof(fmtbuf));
	do_command(fd, &format, fmtbuf, sizeof(fmtbuf));
}

void
do_command(fd, cdb, buf, len)
	int fd;
	struct scsi_cdb *cdb;
	void *buf;
	int len;
{
	static int on = 1, off = 0;
	int user, ret;

	bzero(buf, len);
	if (ioctl(fd, SDIOCSFORMAT, &on) < 0) {
		(void)fprintf(stderr,
		    "scsiformat: SDIOCSFORMAT (on): %s\n", strerror(errno));
		if (ioctl(fd, SDIOCGFORMAT, &user) == 0 && user != 0)
			(void)fprintf(stderr, "scsiformat: pid %d has it\n",
			    user);
		return;
	}
	ret = ioctl(fd, SDIOCSCSICOMMAND, cdb);
#ifdef COMPAT_HPSCSI
	if (ret < 0) {
		static const char scsicmdlen[8] = { 6, 10, 0, 0, 0, 12, 0, 0 };
#define	SCSICMDLEN(cmd)	scsicmdlen[(cmd) >> 5]
		struct scsi_fmt_cdb {
			int	len;
			u_char	cdb[28];
		} sc;
#define OSDIOCSCSICOMMAND _IOW('S', 0x3, struct scsi_fmt_cdb)

		sc.len = SCSICMDLEN(cdb->cdb_bytes[0]);
		bcopy(cdb->cdb_bytes, sc.cdb, sc.len);
		ret = ioctl(fd, OSDIOCSCSICOMMAND, &sc);
	}
#endif
	if (ret < 0)
		(void)fprintf(stderr,
		    "scsiformat: SDIOCSCSICOMMAND: %s\n", strerror(errno));
	else if (read(fd, buf, len) < 0) {
		(void)fprintf(stderr,
		    "scsiformat: read: %s\n", strerror(errno));
		pr_sense(fd);
	}

	if (ioctl(fd, SDIOCSFORMAT, &off) < 0)
		(void)fprintf(stderr,
		    "scsiformat: SDIOCSFORMAT (off): %s\n", strerror(errno));
}

void
usage()
{
	(void)fprintf(stderr, "usage: scsiformat [-r] [-p c|d|s|v] device\n");
	exit(1);
}
