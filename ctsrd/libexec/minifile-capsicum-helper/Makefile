# $FreeBSD$

.include <bsd.init.mk>

PROG=	minifile-capsicum-helper

SRCS=	minifile-capsicum.c

MAN=

CFLAGS+=	-I${.CURDIR}/../../usr.bin/minifile

LDADD+=		-lvuln_magic
LIBADD+=	z

.include <bsd.prog.mk>
