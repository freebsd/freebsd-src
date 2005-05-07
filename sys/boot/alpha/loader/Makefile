# $FreeBSD: src/sys/boot/alpha/loader/Makefile,v 1.8 2004/02/11 22:01:17 ru Exp $

BOOT= 		loader
NEWVERSWHAT=	"SRM disk boot" alpha
INSTALL_HELP=	yes
LOAD_ADDRESS=	${SECONDARY_LOAD_ADDRESS}

# Only disk support
CFLAGS+=	-DLOADER_DISK_SUPPORT	# -DLOADER_EXT2FS_SUPPORT

.include "${.CURDIR}/../common/Makefile.common"
