# $FreeBSD: src/sys/boot/alpha/loader/Makefile,v 1.2.2.4 2000/10/30 10:02:47 obrien Exp $

BASE= 		loader
PROG= 		${BASE}
NEWVERSWHAT=	"SRM disk boot" alpha
INSTALL_HELP=	yes
LOAD_ADDRESS=	${SECONDARY_LOAD_ADDRESS}

# Only disk support
CFLAGS+=	-DLOADER_DISK_SUPPORT	# -DLOADER_EXT2FS_SUPPORT

.include 	<${.CURDIR}/../common/Makefile.common>
