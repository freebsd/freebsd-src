# $FreeBSD: src/sys/boot/alpha/netboot/Makefile,v 1.8.2.1 2000/07/06 23:29:47 obrien Exp $
#	$NetBSD: Makefile,v 1.12 1998/02/19 14:18:36 drochner Exp $

BASE= 		netboot
PROG= 		${BASE}
NOMAN=
NEWVERSWHAT=	"SRM net boot" alpha
LOAD_ADDRESS=	${PRIMARY_LOAD_ADDRESS}

CFLAGS+=	-DLOADER_NET_SUPPORT

.include	<${.CURDIR}/../common/Makefile.common>

