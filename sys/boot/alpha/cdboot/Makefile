# $FreeBSD: src/sys/boot/alpha/cdboot/Makefile,v 1.9 2004/12/21 08:47:12 ru Exp $

BOOT= 		cdboot
NO_MAN=
NEWVERSWHAT=	"SRM CD9660 boot" alpha
LOAD_ADDRESS=	${PRIMARY_LOAD_ADDRESS}

CFLAGS+=	-DLOADER_CDROM_SUPPORT

.include "${.CURDIR}/../common/Makefile.common"
