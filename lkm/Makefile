#	$Id: Makefile,v 1.34 1998/11/03 08:07:08 peter Exp $

# XXX present but broken: ip_mroute_mod mfs
.if	${MACHINE_ARCH} == "i386"
SUBDIR=	atapi coda ccd cd9660 coff fdesc ibcs2 if_disc if_ppp if_sl if_tun \
	ipfw joy kernfs linux msdos nfs nullfs \
	pcic portal procfs syscons umapfs vesa vinum
.endif

# XXX builds, but not useable with present design
# fpu gnufpu
# XXX builds, but needs more code to make it work as an LKM.
# This probably will not happen since LKM's are near end of life (see kld).
# union

.include <bsd.subdir.mk>
