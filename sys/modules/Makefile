#	$Id: Makefile,v 1.58 1999/04/19 20:36:06 peter Exp $

# XXX present but broken: atapi ip_mroute_mod joy
# XXX not yet completed porting: acd
# XXX deprecated: lkm

SUBDIR=	ccd cd9660 coda fdesc fxp if_disc if_ppp if_sl if_tun ipfw kernfs \
	mfs msdos nfs ntfs nullfs portal procfs umapfs union vinum vn

# XXX some of these can move to the general case when de-i386'ed
.if ${MACHINE_ARCH} == "i386"
SUBDIR+=coff fpu gnufpu ibcs2 linux splash streams \
	svr4 syscons vesa
.endif

.include <bsd.subdir.mk>
