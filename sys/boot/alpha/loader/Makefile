# $FreeBSD$

BASE= 		loader
PROG= 		${BASE}
NOMAN=
NEWVERSWHAT=	"SRM disk boot" alpha
INSTALL_HELP=	yes
INSTALL_FORTH=	yes
LOAD_ADDRESS=	${SECONDARY_LOAD_ADDRESS}

# Only disk support
CFLAGS+=	-DLOADER_DISK_SUPPORT	# -DLOADER_EXT2FS_SUPPORT

.include 	<${.CURDIR}/../common/Makefile.common>
