#	$Id: Makefile,v 1.49 1999/01/30 06:31:51 newton Exp $

# XXX present but broken: ip_mroute_mod
# XXX not yet completed porting: acd
# XXX builds but not updated: atapi

SUBDIR=	ccd cd9660 coda fdesc if_disc if_ppp if_sl if_tun ipfw kernfs \
	mfs msdos nfs nullfs portal procfs umapfs union vinum vn ntfs

# XXX some of these can move to the general case when de-i386'ed
.if ${MACHINE_ARCH} == "i386"
SUBDIR+=atapi coff fpu gnufpu ibcs2 joy linux lkm pcic splash syscons vesa svr4
.endif

.include <bsd.subdir.mk>
