#	$Id: Makefile,v 1.64 1999/06/12 15:11:33 roger Exp $

# XXX present but broken: atapi ip_mroute_mod joy pcic

SUBDIR=	ccd cd9660 coda fdesc fxp if_disc if_ppp if_sl if_tun ipfw kernfs \
	mfs msdos nfs ntfs nullfs portal procfs umapfs union vn

# XXX some of these can move to the general case when de-i386'ed
.if ${MACHINE_ARCH} == "i386"
SUBDIR+=bktr coff fpu gnufpu ibcs2 linux splash streams \
	svr4 syscons vesa vinum
.endif

.include <bsd.subdir.mk>
