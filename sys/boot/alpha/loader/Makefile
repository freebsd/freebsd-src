# $FreeBSD: src/sys/boot/alpha/loader/Makefile,v 1.7 2002/05/10 09:26:29 obrien Exp $

BASE= 		loader
PROG= 		${BASE}
NEWVERSWHAT=	"SRM disk boot" alpha
INSTALL_HELP=	yes
LOAD_ADDRESS=	${SECONDARY_LOAD_ADDRESS}

# Only disk support
CFLAGS+=	-ffreestanding -DLOADER_DISK_SUPPORT	# -DLOADER_EXT2FS_SUPPORT

.include 	<${.CURDIR}/../common/Makefile.common>
