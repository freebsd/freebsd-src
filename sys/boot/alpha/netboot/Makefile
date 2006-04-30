# $FreeBSD$
#	$NetBSD: Makefile,v 1.12 1998/02/19 14:18:36 drochner Exp $

BOOT= 		netboot
NO_MAN=
NEWVERSWHAT=	"SRM net boot" alpha
LOAD_ADDRESS=	${PRIMARY_LOAD_ADDRESS}

CFLAGS+=	-DLOADER_NET_SUPPORT

.include "${.CURDIR}/../common/Makefile.common"
