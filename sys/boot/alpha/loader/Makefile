# $FreeBSD: src/sys/boot/alpha/loader/Makefile,v 1.2 1999/09/05 15:16:49 peter Exp $

BASE= 		loader
PROG= 		${BASE}
NOMAN=
NEWVERSWHAT=	"SRM disk boot" alpha
INSTALL_HELP=	yes
LOAD_ADDRESS=	${SECONDARY_LOAD_ADDRESS}

# Only disk support
CFLAGS+=	-DLOADER_DISK_SUPPORT

.include 	<${.CURDIR}/../common/Makefile.common>
