#	$Id: Makefile,v 1.42 1998/12/28 05:27:19 peter Exp $

# XXX present but broken: ip_mroute_mod
# XXX not yet completed porting: pcic syscons/*
# XXX builds but not updated: atapi, wcd
SUBDIR=	ccd cd9660 coda fdesc if_disc if_ppp if_sl if_tun ipfw kernfs \
	mfs msdos nfs nullfs portal procfs umapfs union vinum 

# XXX some of these can move to the general case when de-i386'ed
.if ${MACHINE_ARCH} == "i386"
SUBDIR+=atapi coff fpu gnufpu ibcs2 joy linux syscons vesa
.endif

.include <bsd.subdir.mk>
