#	$Id: Makefile,v 1.66 1999/07/23 05:47:55 wpaul Exp $

# XXX present but broken: atapi ip_mroute_mod joy pcic

SUBDIR=	ax ccd cd9660 coda fdesc fxp if_disc if_ppp if_sl if_tun ipfw \
	kernfs mfs msdos mx nfs ntfs nullfs portal procfs sk ti tl \
	umapfs union vn xl

# XXX some of these can move to the general case when de-i386'ed
.if ${MACHINE_ARCH} == "i386"
SUBDIR+=bktr coff fpu gnufpu ibcs2 linux splash streams \
	svr4 syscons vesa vinum
.endif

.include <bsd.subdir.mk>
