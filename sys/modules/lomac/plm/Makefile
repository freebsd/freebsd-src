# $FreeBSD$

KMOD=	lomac_plm
SRCS=	vnode_if.h kernel_plm.c
NOMAN=	yes
CFLAGS+=	-I${.CURDIR}/../../../security/lomac

.PATH:	${.CURDIR}/../../../security/lomac

.include "../Makefile.inc"
.include <bsd.kmod.mk>
