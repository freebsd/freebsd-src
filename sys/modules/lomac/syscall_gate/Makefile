# $FreeBSD$

KMOD=	syscall_gate
SRCS=	syscall_gate.c
NOMAN=	yes
CFLAGS+=	-I${.CURDIR}/../../../security/lomac

.PATH:	${.CURDIR}/../../../security/lomac

.include "../Makefile.inc"
.include <bsd.kmod.mk>
