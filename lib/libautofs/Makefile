# $Id: Makefile,v 1.3 2004/08/31 08:49:56 bright Exp $
# $FreeBSD$

LIB=autofs
SHLIB_MAJOR= 1
SHLIB_MINOR= 0
WARNS=4
CFLAGS+=	-I${.CURDIR}/../autofs -Werror
CFLAGS+=	-g

SRCS=	libautofs.c
MAN+=	libautofs.3

.include <bsd.lib.mk>
