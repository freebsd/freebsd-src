# $FreeBSD$

KMOD=	syscall_gate
SRCS=	syscall_gate.c
NOMAN=	yes
CFLAGS+=	-I${.CURDIR}/../../../security/lomac

EXPORT_SYMS=	syscall_gate_register	\
		syscall_gate_deregister

.PATH:	${.CURDIR}/../../../security/lomac

.include "../Makefile.inc"
.include <bsd.kmod.mk>
