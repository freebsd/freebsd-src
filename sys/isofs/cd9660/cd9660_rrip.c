/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley
 * by Pace Willisson (pace@blitz.com).  The Rock Ridge Extension
 * Support code is derived from software contributed to Berkeley
 * by Atsushi Murai (amurai@spec.co.jp).
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
 *	@(#)cd9660_rrip.c	8.2 (Berkeley) 1/23/94
 * $Id: cd9660_rrip.c,v 1.9 1995/12/03 17:14:36 bde Exp $
 */

#define CD9660 1  /* bogus dependency in sys/mount.h */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/buf.h>
#include <sys/file.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/kernel.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <sys/time.h>

#include <isofs/cd9660/iso.h>
#include <isofs/cd9660/cd9660_node.h>
#include <isofs/cd9660/cd9660_rrip.h>
#include <isofs/cd9660/iso_rrip.h>

typedef int	rrt_func_t __P((void *, ISO_RRIP_ANALYZE *ana));

typedef struct {
	char type[2];
	rrt_func_t *func;
	void (*func2) __P((struct iso_directory_record *isodir, ISO_RRIP_ANALYZE *ana));
	int result;
} RRIP_TABLE;

static int	cd9660_rrip_altname __P((ISO_RRIP_ALTNAME *p,
					 ISO_RRIP_ANALYZE *ana));
static int	cd9660_rrip_attr __P((ISO_RRIP_ATTR *p, ISO_RRIP_ANALYZE *ana));
static int	cd9660_rrip_cont __P((ISO_RRIP_CONT *p, ISO_RRIP_ANALYZE *ana));
static void	cd9660_rrip_defattr __P((struct iso_directory_record *isodir,
					 ISO_RRIP_ANALYZE *ana));
static void	cd9660_rrip_defname __P((struct iso_directory_record *isodir,
					 ISO_RRIP_ANALYZE *ana));
static void	cd9660_rrip_deftstamp __P((struct iso_directory_record *isodir,
					   ISO_RRIP_ANALYZE *ana));
static int	cd9660_rrip_device __P((ISO_RRIP_DEVICE *p,
					ISO_RRIP_ANALYZE *ana));
static int	cd9660_rrip_extref __P((ISO_RRIP_EXTREF *p,
					ISO_RRIP_ANALYZE *ana));
static int	cd9660_rrip_idflag __P((ISO_RRIP_IDFLAG *p,
					ISO_RRIP_ANALYZE *ana));
static int	cd9660_rrip_loop __P((struct iso_directory_record *isodir,
				      ISO_RRIP_ANALYZE *ana,
				      RRIP_TABLE *table));
static int	cd9660_rrip_pclink __P((ISO_RRIP_CLINK *p,
					ISO_RRIP_ANALYZE *ana));
static int	cd9660_rrip_reldir __P((ISO_RRIP_RELDIR *p,
					ISO_RRIP_ANALYZE *ana));
static int	cd9660_rrip_slink __P((ISO_RRIP_SLINK *p,
				       ISO_RRIP_ANALYZE *ana));
static int	cd9660_rrip_stop __P((ISO_SUSP_HEADER *p,
				      ISO_RRIP_ANALYZE *ana));
static int	cd9660_rrip_tstamp __P((ISO_RRIP_TSTAMP *p,
					ISO_RRIP_ANALYZE *ana));

/*
 * POSIX file attribute
 */
static int
cd9660_rrip_attr(p,ana)
	ISO_RRIP_ATTR *p;
	ISO_RRIP_ANALYZE *ana;
{
	ana->inop->inode.iso_mode = isonum_731(p->mode_l);
	ana->inop->inode.iso_uid = (uid_t)isonum_731(p->uid_l);
	ana->inop->inode.iso_gid = (gid_t)isonum_731(p->gid_l);
	ana->inop->inode.iso_links = isonum_731(p->links_l);
	ana->fields &= ~ISO_SUSP_ATTR;
	return ISO_SUSP_ATTR;
}

static void
cd9660_rrip_defattr(isodir,ana)
	struct iso_directory_record *isodir;
	ISO_RRIP_ANALYZE *ana;
{
	/* But this is a required field! */
	printf("RRIP without PX field?\n");
	cd9660_defattr(isodir,ana->inop,NULL,ISO_FTYPE_RRIP);
}

/*
 * Symbolic Links
 */
static int
cd9660_rrip_slink(p,ana)
	ISO_RRIP_SLINK	*p;
	ISO_RRIP_ANALYZE *ana;
{
	register ISO_RRIP_SLINK_COMPONENT *pcomp;
	register ISO_RRIP_SLINK_COMPONENT *pcompe;
	int len, wlen, cont;
	char *outbuf, *inbuf;

	pcomp = (ISO_RRIP_SLINK_COMPONENT *)p->component;
	pcompe = (ISO_RRIP_SLINK_COMPONENT *)((char *)p + isonum_711(p->h.length));
	len = *ana->outlen;
	outbuf = ana->outbuf;
	cont = ana->cont;

	/*
	 * Gathering a Symbolic name from each component with path
	 */
	for (;
	     pcomp < pcompe;
	     pcomp = (ISO_RRIP_SLINK_COMPONENT *)((char *)pcomp + ISO_RRIP_SLSIZ
						  + isonum_711(pcomp->clen))) {

		if (!cont) {
			if (len < ana->maxlen) {
				len++;
				*outbuf++ = '/';
			}
		}
		cont = 0;

		inbuf = "..";
		wlen = 0;

		switch (*pcomp->cflag) {

		case ISO_SUSP_CFLAG_CURRENT:
			/* Inserting Current */
			wlen = 1;
			break;

		case ISO_SUSP_CFLAG_PARENT:
			/* Inserting Parent */
			wlen = 2;
			break;

		case ISO_SUSP_CFLAG_ROOT:
			/* Inserting slash for ROOT */
			/* start over from beginning(?) */
			outbuf -= len;
			len = 0;
			break;

		case ISO_SUSP_CFLAG_VOLROOT:
			/* Inserting a mount point i.e. "/cdrom" */
			/* same as above */
			outbuf -= len;
			len = 0;
			inbuf = ana->imp->im_mountp->mnt_stat.f_mntonname;
			wlen = strlen(inbuf);
			break;

		case ISO_SUSP_CFLAG_HOST:
			/* Inserting hostname i.e. "kurt.tools.de" */
			inbuf = hostname;
			wlen = strlen(hostname);
			break;

		case ISO_SUSP_CFLAG_CONTINUE:
			cont = 1;
			/* fall thru */
		case 0:
			/* Inserting component */
			wlen = isonum_711(pcomp->clen);
			inbuf = pcomp->name;
			break;
		default:
			printf("RRIP with incorrect flags?");
			wlen = ana->maxlen + 1;
			break;
		}

		if (len + wlen > ana->maxlen) {
			/* indicate error to caller */
			ana->cont = 1;
			ana->fields = 0;
			ana->outbuf -= *ana->outlen;
			*ana->outlen = 0;
			return 0;
		}

		bcopy(inbuf,outbuf,wlen);
		outbuf += wlen;
		len += wlen;

	}
	ana->outbuf = outbuf;
	*ana->outlen = len;
	ana->cont = cont;

	if (!isonum_711(p->flags)) {
		ana->fields &= ~ISO_SUSP_SLINK;
		return ISO_SUSP_SLINK;
	}
	return 0;
}

/*
 * Alternate name
 */
static int
cd9660_rrip_altname(p,ana)
	ISO_RRIP_ALTNAME *p;
	ISO_RRIP_ANALYZE *ana;
{
	char *inbuf;
	int wlen;
	int cont;

	inbuf = "..";
	wlen = 0;
	cont = 0;

	switch (*p->flags) {
	case ISO_SUSP_CFLAG_CURRENT:
		/* Inserting Current */
		wlen = 1;
		break;

	case ISO_SUSP_CFLAG_PARENT:
		/* Inserting Parent */
		wlen = 2;
		break;

	case ISO_SUSP_CFLAG_HOST:
		/* Inserting hostname i.e. "kurt.tools.de" */
		inbuf = hostname;
		wlen = strlen(hostname);
		break;

	case ISO_SUSP_CFLAG_CONTINUE:
		cont = 1;
		/* fall thru */
	case 0:
		/* Inserting component */
		wlen = isonum_711(p->h.length) - 5;
		inbuf = (char *)p + 5;
		break;

	default:
		printf("RRIP with incorrect NM flags?\n");
		wlen = ana->maxlen + 1;
		break;
	}

	if ((*ana->outlen += wlen) > ana->maxlen) {
		/* treat as no name field */
		ana->fields &= ~ISO_SUSP_ALTNAME;
		ana->outbuf -= *ana->outlen - wlen;
		*ana->outlen = 0;
		return 0;
	}

	bcopy(inbuf,ana->outbuf,wlen);
	ana->outbuf += wlen;

	if (!cont) {
		ana->fields &= ~ISO_SUSP_ALTNAME;
		return ISO_SUSP_ALTNAME;
	}
	return 0;
}

static void
cd9660_rrip_defname(isodir,ana)
	struct iso_directory_record *isodir;
	ISO_RRIP_ANALYZE *ana;
{
	strcpy(ana->outbuf,"..");
	switch (*isodir->name) {
	default:
		isofntrans(isodir->name,isonum_711(isodir->name_len),
			   ana->outbuf,ana->outlen,
			   1,isonum_711(isodir->flags)&4);
		break;
	case 0:
		*ana->outlen = 1;
		break;
	case 1:
		*ana->outlen = 2;
		break;
	}
}

/*
 * Parent or Child Link
 */
static int
cd9660_rrip_pclink(p,ana)
	ISO_RRIP_CLINK	*p;
	ISO_RRIP_ANALYZE *ana;
{
	*ana->inump = isonum_733(p->dir_loc) << ana->imp->im_bshift;
	ana->fields &= ~(ISO_SUSP_CLINK|ISO_SUSP_PLINK);
	return *p->h.type == 'C' ? ISO_SUSP_CLINK : ISO_SUSP_PLINK;
}

/*
 * Relocated directory
 */
static int
cd9660_rrip_reldir(p,ana)
	ISO_RRIP_RELDIR	 *p;
	ISO_RRIP_ANALYZE *ana;
{
	/* special hack to make caller aware of RE field */
	*ana->outlen = 0;
	ana->fields = 0;
	return ISO_SUSP_RELDIR|ISO_SUSP_ALTNAME|ISO_SUSP_CLINK|ISO_SUSP_PLINK;
}

static int
cd9660_rrip_tstamp(p,ana)
	ISO_RRIP_TSTAMP *p;
	ISO_RRIP_ANALYZE *ana;
{
	unsigned char *ptime;

	ptime = p->time;

	/* Check a format of time stamp (7bytes/17bytes) */
	if (!(*p->flags&ISO_SUSP_TSTAMP_FORM17)) {
		if (*p->flags&ISO_SUSP_TSTAMP_CREAT)
			ptime += 7;

		if (*p->flags&ISO_SUSP_TSTAMP_MODIFY) {
			cd9660_tstamp_conv7(ptime,&ana->inop->inode.iso_mtime,
					    ISO_FTYPE_RRIP);
			ptime += 7;
		} else
			bzero(&ana->inop->inode.iso_mtime,sizeof(struct timespec));

		if (*p->flags&ISO_SUSP_TSTAMP_ACCESS) {
			cd9660_tstamp_conv7(ptime,&ana->inop->inode.iso_atime,
					    ISO_FTYPE_RRIP);
			ptime += 7;
		} else
			ana->inop->inode.iso_atime = ana->inop->inode.iso_mtime;

		if (*p->flags&ISO_SUSP_TSTAMP_ATTR)
			cd9660_tstamp_conv7(ptime,&ana->inop->inode.iso_ctime,
					    ISO_FTYPE_RRIP);
		else
			ana->inop->inode.iso_ctime = ana->inop->inode.iso_mtime;

	} else {
		if (*p->flags&ISO_SUSP_TSTAMP_CREAT)
			ptime += 17;

		if (*p->flags&ISO_SUSP_TSTAMP_MODIFY) {
			cd9660_tstamp_conv17(ptime,&ana->inop->inode.iso_mtime);
			ptime += 17;
		} else
			bzero(&ana->inop->inode.iso_mtime,sizeof(struct timespec));

		if (*p->flags&ISO_SUSP_TSTAMP_ACCESS) {
			cd9660_tstamp_conv17(ptime,&ana->inop->inode.iso_atime);
			ptime += 17;
		} else
			ana->inop->inode.iso_atime = ana->inop->inode.iso_mtime;

		if (*p->flags&ISO_SUSP_TSTAMP_ATTR)
			cd9660_tstamp_conv17(ptime,&ana->inop->inode.iso_ctime);
		else
			ana->inop->inode.iso_ctime = ana->inop->inode.iso_mtime;

	}
	ana->fields &= ~ISO_SUSP_TSTAMP;
	return ISO_SUSP_TSTAMP;
}

static void
cd9660_rrip_deftstamp(isodir,ana)
	struct iso_directory_record  *isodir;
	ISO_RRIP_ANALYZE *ana;
{
	cd9660_deftstamp(isodir,ana->inop,NULL,ISO_FTYPE_RRIP);
}

/*
 * POSIX device modes
 */
static int
cd9660_rrip_device(p,ana)
	ISO_RRIP_DEVICE *p;
	ISO_RRIP_ANALYZE *ana;
{
	unsigned high, low;

	high = isonum_733(p->dev_t_high_l);
	low  = isonum_733(p->dev_t_low_l);

	if ( high == 0 ) {
		ana->inop->inode.iso_rdev = makedev( major(low), minor(low) );
	} else {
		ana->inop->inode.iso_rdev = makedev( high, minor(low) );
	}
	ana->fields &= ~ISO_SUSP_DEVICE;
	return ISO_SUSP_DEVICE;
}

/*
 * Flag indicating
 */
static int
cd9660_rrip_idflag(p,ana)
	ISO_RRIP_IDFLAG *p;
	ISO_RRIP_ANALYZE *ana;
{
	ana->fields &= isonum_711(p->flags)|~0xff; /* don't touch high bits */
	/* special handling of RE field */
	if (ana->fields&ISO_SUSP_RELDIR)
		return cd9660_rrip_reldir(/* XXX */ (ISO_RRIP_RELDIR *)p,ana);

	return ISO_SUSP_IDFLAG;
}

/*
 * Continuation pointer
 */
static int
cd9660_rrip_cont(p,ana)
	ISO_RRIP_CONT *p;
	ISO_RRIP_ANALYZE *ana;
{
	ana->iso_ce_blk = isonum_733(p->location);
	ana->iso_ce_off = isonum_733(p->offset);
	ana->iso_ce_len = isonum_733(p->length);
	return ISO_SUSP_CONT;
}

/*
 * System Use end
 */
static int
cd9660_rrip_stop(p,ana)
	ISO_SUSP_HEADER *p;
	ISO_RRIP_ANALYZE *ana;
{
	/* stop analyzing */
	ana->fields = 0;
	return ISO_SUSP_STOP;
}

/*
 * Extension reference
 */
static int
cd9660_rrip_extref(p,ana)
	ISO_RRIP_EXTREF *p;
	ISO_RRIP_ANALYZE *ana;
{
	if (isonum_711(p->len_id) != 10
	    || bcmp((char *)p + 8,"RRIP_1991A",10)
	    || isonum_711(p->version) != 1)
		return 0;
	ana->fields &= ~ISO_SUSP_EXTREF;
	return ISO_SUSP_EXTREF;
}

static int
cd9660_rrip_loop(isodir,ana,table)
	struct iso_directory_record *isodir;
	ISO_RRIP_ANALYZE *ana;
	RRIP_TABLE *table;
{
	register RRIP_TABLE *ptable;
	register ISO_SUSP_HEADER *phead;
	register ISO_SUSP_HEADER *pend;
	struct buf *bp = NULL;
	char *pwhead;
	int result;

	/*
	 * Note: If name length is odd,
	 *	 it will be padding 1 byte after the name
	 */
	pwhead = isodir->name + isonum_711(isodir->name_len);
	if (!(isonum_711(isodir->name_len)&1))
		pwhead++;

	/* If it's not the '.' entry of the root dir obey SP field */
	if (*isodir->name != 0
	    || isonum_733(isodir->extent) != ana->imp->root_extent)
		pwhead += ana->imp->rr_skip;
	else
		pwhead += ana->imp->rr_skip0;

	phead = (ISO_SUSP_HEADER *)pwhead;
	pend = (ISO_SUSP_HEADER *)((char *)isodir + isonum_711(isodir->length));

	result = 0;
	while (1) {
		ana->iso_ce_len = 0;
		/*
		 * Note: "pend" should be more than one SUSP header
		 */
		while (pend >= phead + 1) {
			if (isonum_711(phead->version) == 1) {
				for (ptable = table; ptable->func; ptable++) {
					if (*phead->type == *ptable->type
					    && phead->type[1] == ptable->type[1]) {
						result |= ptable->func(phead,ana);
						break;
					}
				}
				if (!ana->fields)
					break;
			}
			/*
			 * move to next SUSP
			 * Hopefully this works with newer versions, too
			 */
			phead = (ISO_SUSP_HEADER *)((char *)phead + isonum_711(phead->length));
		}

		if ( ana->fields && ana->iso_ce_len ) {
			if (ana->iso_ce_blk >= ana->imp->volume_space_size
			    || ana->iso_ce_off + ana->iso_ce_len > ana->imp->logical_block_size
			    || bread(ana->imp->im_devvp,
				     iso_lblktodaddr(ana->imp, ana->iso_ce_blk),
				     ana->imp->logical_block_size,NOCRED,&bp))
				/* what to do now? */
				break;
			phead = (ISO_SUSP_HEADER *)(bp->b_un.b_addr + ana->iso_ce_off);
			pend = (ISO_SUSP_HEADER *) ((char *)phead + ana->iso_ce_len);
		} else
			break;
	}
	if (bp)
		brelse(bp);
	/*
	 * If we don't find the Basic SUSP stuffs, just set default value
	 *   ( attribute/time stamp )
	 */
	for (ptable = table; ptable->func2; ptable++)
		if (!(ptable->result&result))
			ptable->func2(isodir,ana);

	return result;
}

/*
 * XXX the casts are bogus but will do for now.
 */
#define	BC	(rrt_func_t *)
static RRIP_TABLE rrip_table_analyze[] = {
	{ "PX", BC cd9660_rrip_attr,	cd9660_rrip_defattr,	ISO_SUSP_ATTR },
	{ "TF", BC cd9660_rrip_tstamp,	cd9660_rrip_deftstamp,	ISO_SUSP_TSTAMP },
	{ "PN", BC cd9660_rrip_device,	0,			ISO_SUSP_DEVICE },
	{ "RR", BC cd9660_rrip_idflag,	0,			ISO_SUSP_IDFLAG },
	{ "CE", BC cd9660_rrip_cont,	0,			ISO_SUSP_CONT },
	{ "ST", BC cd9660_rrip_stop,	0,			ISO_SUSP_STOP },
	{ "",	0,			0,			0 }
};

int
cd9660_rrip_analyze(isodir,inop,imp)
	struct iso_directory_record *isodir;
	struct iso_node *inop;
	struct iso_mnt *imp;
{
	ISO_RRIP_ANALYZE analyze;

	analyze.inop = inop;
	analyze.imp = imp;
	analyze.fields = ISO_SUSP_ATTR|ISO_SUSP_TSTAMP|ISO_SUSP_DEVICE;

	return cd9660_rrip_loop(isodir,&analyze,rrip_table_analyze);
}

/*
 * Get Alternate Name from 'AL' record
 * If either no AL record or 0 length,
 *    it will be return the translated ISO9660 name,
 */
static RRIP_TABLE rrip_table_getname[] = {
	{ "NM", BC cd9660_rrip_altname,	cd9660_rrip_defname,	ISO_SUSP_ALTNAME },
	{ "CL", BC cd9660_rrip_pclink,	0,			ISO_SUSP_CLINK|ISO_SUSP_PLINK },
	{ "PL", BC cd9660_rrip_pclink,	0,			ISO_SUSP_CLINK|ISO_SUSP_PLINK },
	{ "RE", BC cd9660_rrip_reldir,	0,			ISO_SUSP_RELDIR },
	{ "RR", BC cd9660_rrip_idflag,	0,			ISO_SUSP_IDFLAG },
	{ "CE", BC cd9660_rrip_cont,	0,			ISO_SUSP_CONT },
	{ "ST", BC cd9660_rrip_stop,	0,			ISO_SUSP_STOP },
	{ "",	0,			0,			0 }
};

int
cd9660_rrip_getname(isodir,outbuf,outlen,inump,imp)
	struct iso_directory_record *isodir;
	char *outbuf;
	u_short *outlen;
	ino_t *inump;
	struct iso_mnt *imp;
{
	ISO_RRIP_ANALYZE analyze;
	RRIP_TABLE *tab;

	analyze.outbuf = outbuf;
	analyze.outlen = outlen;
	analyze.maxlen = NAME_MAX;
	analyze.inump = inump;
	analyze.imp = imp;
	analyze.fields = ISO_SUSP_ALTNAME|ISO_SUSP_RELDIR|ISO_SUSP_CLINK|ISO_SUSP_PLINK;
	*outlen = 0;

	tab = rrip_table_getname;
	if (*isodir->name == 0
	    || *isodir->name == 1) {
		cd9660_rrip_defname(isodir,&analyze);

		analyze.fields &= ~ISO_SUSP_ALTNAME;
		tab++;
	}

	return cd9660_rrip_loop(isodir,&analyze,tab);
}

/*
 * Get Symbolic Name from 'SL' record
 *
 * Note: isodir should contains SL record!
 */
static RRIP_TABLE rrip_table_getsymname[] = {
	{ "SL", BC cd9660_rrip_slink,	0,			ISO_SUSP_SLINK },
	{ "RR", BC cd9660_rrip_idflag,	0,			ISO_SUSP_IDFLAG },
	{ "CE", BC cd9660_rrip_cont,	0,			ISO_SUSP_CONT },
	{ "ST", BC cd9660_rrip_stop,	0,			ISO_SUSP_STOP },
	{ "",	0,			0,			0 }
};

int
cd9660_rrip_getsymname(isodir,outbuf,outlen,imp)
	struct iso_directory_record *isodir;
	char *outbuf;
	u_short *outlen;
	struct iso_mnt *imp;
{
	ISO_RRIP_ANALYZE analyze;

	analyze.outbuf = outbuf;
	analyze.outlen = outlen;
	*outlen = 0;
	analyze.maxlen = MAXPATHLEN;
	analyze.cont = 1;		/* don't start with a slash */
	analyze.imp = imp;
	analyze.fields = ISO_SUSP_SLINK;

	return (cd9660_rrip_loop(isodir,&analyze,rrip_table_getsymname)&ISO_SUSP_SLINK);
}

static RRIP_TABLE rrip_table_extref[] = {
	{ "ER", BC cd9660_rrip_extref,	0,			ISO_SUSP_EXTREF },
	{ "CE", BC cd9660_rrip_cont,	0,			ISO_SUSP_CONT },
	{ "ST", BC cd9660_rrip_stop,	0,			ISO_SUSP_STOP },
	{ "",	0,			0,			0 }
};

/*
 * Check for Rock Ridge Extension and return offset of its fields.
 * Note: We require the ER field.
 */
int
cd9660_rrip_offset(isodir,imp)
	struct iso_directory_record *isodir;
	struct iso_mnt *imp;
{
	ISO_RRIP_OFFSET *p;
	ISO_RRIP_ANALYZE analyze;

	imp->rr_skip0 = 0;
	p = (ISO_RRIP_OFFSET *)(isodir->name + 1);
	if (bcmp(p,"SP\7\1\276\357",6)) {
		/* Maybe, it's a CDROM XA disc? */
		imp->rr_skip0 = 15;
		p = (ISO_RRIP_OFFSET *)((char *)p + 15);
		if (bcmp(p,"SP\7\1\276\357",6))
			return -1;
	}

	analyze.imp = imp;
	analyze.fields = ISO_SUSP_EXTREF;
	if (!(cd9660_rrip_loop(isodir,&analyze,rrip_table_extref)&ISO_SUSP_EXTREF))
		return -1;

	return isonum_711(p->skip);
}
