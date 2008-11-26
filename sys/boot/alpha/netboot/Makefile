# $FreeBSD: src/sys/boot/alpha/netboot/Makefile,v 1.12.12.1 2008/10/02 02:57:24 kensmith Exp $
#	$NetBSD: Makefile,v 1.12 1998/02/19 14:18:36 drochner Exp $

BOOT= 		netboot
NO_MAN=
NEWVERSWHAT=	"SRM net boot" alpha
LOAD_ADDRESS=	${PRIMARY_LOAD_ADDRESS}

CFLAGS+=	-DLOADER_NET_SUPPORT

.include "${.CURDIR}/../common/Makefile.common"
