#	$Id: Makefile,v 1.33 1998/09/28 05:38:22 grog Exp $

# XXX present but broken: ip_mroute_mod
# XXX not yet completed porting: pcic coda syscons/*
.if	${MACHINE_ARCH} == "i386"
SUBDIR=	atapi ccd cd9660 coff fdesc fpu gnufpu ibcs2 if_disc if_ppp if_sl \
	if_tun ipfw joy kernfs linux mfs msdos nfs nullfs \
	portal procfs syscons umapfs vesa vinum wcd
.endif

# XXX builds, but not useable with present design
#union

.include <bsd.subdir.mk>
