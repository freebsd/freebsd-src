/*
 * Parts of this file are not covered by:
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dknet.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: inflate.h,v 1.2 1994/10/07 23:18:18 csgr Exp $
 *
 * This module handles execution of a.out files which have been run through
 * "gzip -9".
 *
 * For now you need to use exactly this command to compress the binaries:
 *
 *		gzip -9 -v < /bin/sh > /tmp/sh
 *
 * TODO:
 *	text-segments should be made R/O after being filled
 *	is the vm-stuff safe ?
 * 	should handle the entire header of gzip'ed stuff.
 *	inflate isn't quite reentrant yet...
 *	error-handling is a mess...
 *	so is the rest...
 *	tidy up unnecessary includes
 */


#ifndef _SYS_INFLATE_H_
#define _SYS_INFLATE_H_

#ifdef KERNEL

#include <sys/param.h>
#include <sys/imgact.h>
#include <sys/imgact_aout.h>


/* needed to make inflate() work */
#define	uch u_char
#define	ush u_short
#define	ulg u_long


#define WSIZE 0x8000

struct gzip {
	struct	image_params *ip;
	struct  exec a_out;
	int	error;
	int	where;
	u_char  *inbuf;
	u_long	offset;
	u_long	output;
	u_long	len;
	int	idx;
	u_long	virtual_offset, file_offset, file_end, bss_size;
        unsigned gz_wp;
	u_char	*gz_slide;
};

/*
 * Global variables used by inflate and friends.
 * This structure is used in order to make inflate() reentrant.
 */
struct gz_global {
	ulg		gz_bb;		/* bit buffer */
	unsigned	gz_bk;		/* bits in bit buffer */
	unsigned	gz_hufts;	/* track memory usage */
	struct huft 	*gz_fixed_tl;	/* must init to NULL !! */
	struct huft	*gz_fixed_td;
	int		gz_fixed_bl;
	int		gz_fixed_bd;
};

int inflate __P((struct gzip *, struct gz_global *));
int do_aout_hdr __P((struct gzip *));

#define slide (gz->gz_slide)
#define wp    (gz->gz_wp)

#endif /* KERNEL */

#endif /* ! _SYS_INFLATE_H_ */
