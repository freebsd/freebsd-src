#	$Id: Makefile,v 1.45 1999/01/11 03:34:42 yokota Exp $

# XXX present but broken: ip_mroute_mod
# XXX not yet completed porting: acd pcic
# XXX builds but not updated: atapi
# XXX builds, but possibly unusable with present design: fpu gnufpu

SUBDIR=	ccd cd9660 coda fdesc if_disc if_ppp if_sl if_tun ipfw kernfs \
	mfs msdos nfs nullfs portal procfs umapfs union vinum 

# XXX some of these can move to the general case when de-i386'ed
.if ${MACHINE_ARCH} == "i386"
SUBDIR+=atapi coff fpu gnufpu ibcs2 joy linux lkm splash syscons vesa
.endif

.include <bsd.subdir.mk>
