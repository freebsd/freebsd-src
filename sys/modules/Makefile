#	$Id: Makefile,v 1.47.2.1 1999/04/19 14:29:32 semenu Exp $

# XXX present but broken: ip_mroute_mod
# XXX not yet completed porting: acd
# XXX builds but not updated: atapi

SUBDIR=	ccd cd9660 coda fdesc if_disc if_ppp if_sl if_tun ipfw kernfs \
	mfs msdos nfs ntfs nullfs portal procfs umapfs union vinum 

# XXX some of these can move to the general case when de-i386'ed
.if ${MACHINE_ARCH} == "i386"
SUBDIR+=atapi coff fpu gnufpu ibcs2 joy linux lkm splash syscons vesa # pcic
.endif

.include <bsd.subdir.mk>
