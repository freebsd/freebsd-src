# $FreeBSD: src/sys/boot/alpha/loader/Makefile,v 1.2.2.3 2000/07/06 23:27:28 obrien Exp $

BASE= 		loader
PROG= 		${BASE}
NOMAN=
NEWVERSWHAT=	"SRM disk boot" alpha
INSTALL_HELP=	yes
LOAD_ADDRESS=	${SECONDARY_LOAD_ADDRESS}

# Only disk support
CFLAGS+=	-DLOADER_DISK_SUPPORT	# -DLOADER_EXT2FS_SUPPORT

.include 	<${.CURDIR}/../common/Makefile.common>
