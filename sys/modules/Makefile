#	$Id: Makefile,v 1.56 1999/04/17 16:31:13 peter Exp $

# XXX present but broken: atapi ip_mroute_mod
# XXX not yet completed porting: acd
# XXX builds but not updated: joy
# XXX deprecated: lkm

SUBDIR=	ccd cd9660 coda fdesc fxp if_disc if_ppp if_sl if_tun ipfw kernfs \
	mfs msdos nfs ntfs nullfs portal procfs umapfs union vinum vn

# XXX some of these can move to the general case when de-i386'ed
.if ${MACHINE_ARCH} == "i386"
SUBDIR+=coff fpu gnufpu ibcs2 joy linux pcic splash streams \
	svr4 syscons vesa
.endif

.include <bsd.subdir.mk>
