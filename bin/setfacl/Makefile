# $FreeBSD$

CFLAGS+=	-g ${BDECFLAGS}

PROG=		setfacl
SRCS=		file.c mask.c merge.c remove.c setfacl.c util.c
LDADD=		-lposix1e

NOSHARED=	yes

.include <bsd.prog.mk>
