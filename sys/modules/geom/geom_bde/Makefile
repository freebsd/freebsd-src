# $FreeBSD: src/sys/modules/geom/geom_bde/Makefile,v 1.2 2006/06/09 17:36:50 pjd Exp $

.PATH: ${.CURDIR}/../../../geom/bde
.PATH: ${.CURDIR}/../../../crypto/rijndael
.PATH: ${.CURDIR}/../../../crypto/sha2

KMOD=	geom_bde
SRCS=	g_bde.c g_bde_crypt.c g_bde_lock.c g_bde_work.c
SRCS+=	rijndael-alg-fst.c rijndael-api-fst.c sha2.c

.include <bsd.kmod.mk>
