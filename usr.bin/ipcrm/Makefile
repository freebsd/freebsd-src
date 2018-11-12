# $FreeBSD$

PROG=	ipcrm
SRCS=	ipcrm.c ipc.c
LIBADD=	kvm
CFLAGS+=-I${.CURDIR}/../ipcs
.PATH:  ${.CURDIR}/../ipcs

WARNS?=	0

.include <bsd.prog.mk>
