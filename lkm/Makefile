#	$Id: Makefile,v 1.29 1998/02/18 14:30:07 msmith Exp $

# XXX present but broken: ip_mroute_mod mfs
.if	${MACHINE_ARCH} == "i386"
SUBDIR=	atapi ccd cd9660 coff fdesc ibcs2 if_disc if_ppp if_sl if_tun \
	ipfw joy kernfs linux msdos nfs nullfs \
	pcic portal procfs syscons umapfs wcd
.endif

# XXX builds, but not useable with present design
# fpu gnufpu union

.include <bsd.subdir.mk>
