#	$Id: Makefile,v 1.34 1998/10/16 04:30:41 peter Exp $

# XXX present but broken: ip_mroute_mod
# XXX not yet completed porting: pcic coda syscons/*
# XXX builds but not updated: atapi, wcd, maybe vesa
.if	${MACHINE_ARCH} == "i386"
SUBDIR=	atapi ccd cd9660 coff fdesc fpu gnufpu ibcs2 if_disc if_ppp if_sl \
	if_tun ipfw joy kernfs linux mfs msdos nfs nullfs \
	portal procfs syscons umapfs vesa vinum wcd
.endif

# XXX builds, but not useable with present design (#ifdef UNION in kernel)
#union

.include <bsd.subdir.mk>
