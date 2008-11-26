# $FreeBSD: src/sys/boot/alpha/loader/Makefile,v 1.8.20.1 2008/10/02 02:57:24 kensmith Exp $

BOOT= 		loader
NEWVERSWHAT=	"SRM disk boot" alpha
INSTALL_HELP=	yes
LOAD_ADDRESS=	${SECONDARY_LOAD_ADDRESS}

# Only disk support
CFLAGS+=	-DLOADER_DISK_SUPPORT	# -DLOADER_EXT2FS_SUPPORT

.include "${.CURDIR}/../common/Makefile.common"
