#	$Id: Makefile,v 1.3 1997/02/22 12:47:43 peter Exp $

.PATH:	${.CURDIR}/../../sys/isofs/cd9660
KMOD=	cd9660_mod
SRCS=	cd9660_bmap.c cd9660_lookup.c cd9660_node.c cd9660_rrip.c \
	cd9660_util.c cd9660_vfsops.c cd9660_vnops.c opt_diagnostic.h
NOMAN=
VFS_LKM=
CFLAGS+= -DCD9660
CLEANFILES+=opt_diagnostic.h

opt_diagnostic.h:
	touch opt_diagnostic.h

.include <bsd.kmod.mk>
