#	$Id: Makefile,v 1.24 1996/06/23 13:31:20 bde Exp $

# XXX present but broken: ip_mroute_mod mfs
SUBDIR=	atapi ccd cd9660 coff fdesc ibcs2 if_disc if_ppp if_sl if_tun \
	ipfw joy kernfs linux msdos nfs nullfs \
	pcic portal procfs qcam syscons umapfs 
# BROKEN due to lkm.h changes precluding > 1 module device per file.
#wcd

# XXX builds, but not useable with present design
# fpu gnufpu union

.include <bsd.subdir.mk>
