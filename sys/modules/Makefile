#	$Id: Makefile,v 1.52 1999/03/14 09:20:01 julian Exp $

# XXX present but broken: ip_mroute_mod
# XXX not yet completed porting: acd
# XXX builds but not updated: atapi

SUBDIR=	ccd cd9660 coda fdesc if_disc if_ppp if_sl if_tun ipfw kernfs \
	mfs msdos nfs nullfs portal procfs umapfs union vinum ntfs vn

# XXX some of these can move to the general case when de-i386'ed
.if ${MACHINE_ARCH} == "i386"
SUBDIR+=atapi coff fpu gnufpu ibcs2 joy linux lkm pcic splash streams \
	syscons vesa svr4
.endif

.include <bsd.subdir.mk>
