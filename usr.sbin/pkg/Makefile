# $FreeBSD$

PROG=	pkg
SRCS=	pkg.c dns_utils.c config.c
MAN=	pkg.7

CFLAGS+=-I${.CURDIR}/../../contrib/libucl/include
.PATH:	${.CURDIR}/../../contrib/libucl/include
LIBADD=	archive fetch ucl sbuf crypto ssl

.include <bsd.prog.mk>
