#	$Id: Makefile,v 1.55 1999/04/13 18:53:54 peter Exp $

# XXX present but broken: ip_mroute_mod
# XXX not yet completed porting: acd
# XXX builds but not updated: atapi
# XXX depreciated: lkm

SUBDIR=	ccd cd9660 coda fdesc if_disc if_ppp if_sl if_tun ipfw kernfs \
	mfs msdos nfs ntfs nullfs portal procfs umapfs union vinum vn

# XXX some of these can move to the general case when de-i386'ed
.if ${MACHINE_ARCH} == "i386"
SUBDIR+=coff fpu gnufpu ibcs2 joy linux pcic splash streams \
	syscons vesa svr4
.endif

.include <bsd.subdir.mk>
