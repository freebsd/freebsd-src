#	$Id: Makefile,v 1.31 1998/09/15 20:17:35 sos Exp $

# XXX present but broken: ip_mroute_mod mfs
.if	${MACHINE_ARCH} == "i386"
SUBDIR=	atapi coda ccd cd9660 coff fdesc ibcs2 if_disc if_ppp if_sl if_tun \
	ipfw joy kernfs linux msdos nfs nullfs \
	pcic portal procfs syscons umapfs vesa wcd
.endif

# XXX builds, but not useable with present design
# fpu gnufpu union

.include <bsd.subdir.mk>
