# $FreeBSD$

KMOD=	lomac_plm
SRCS=	kernel_plm.c
NOMAN=	yes
CFLAGS+=	-I${.CURDIR}/../../../contrib/lomac

.PATH:	${.CURDIR}/../../../contrib/lomac

.include "../Makefile.inc"
.include <bsd.kmod.mk>
