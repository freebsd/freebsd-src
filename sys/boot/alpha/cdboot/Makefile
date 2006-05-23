# $FreeBSD: src/sys/boot/alpha/cdboot/Makefile,v 1.8 2004/02/11 22:01:17 ru Exp $

BOOT= 		cdboot
NOMAN=
NEWVERSWHAT=	"SRM CD9660 boot" alpha
LOAD_ADDRESS=	${PRIMARY_LOAD_ADDRESS}

CFLAGS+=	-DLOADER_CDROM_SUPPORT

.include "${.CURDIR}/../common/Makefile.common"
