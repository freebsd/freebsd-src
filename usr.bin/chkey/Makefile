#	$Id: Makefile,v 1.1.1.1 1997/05/28 15:54:04 wpaul Exp $

PROG=	chkey
SRCS=	chkey.c update.c generic.c

.PATH: ${.CURDIR}/../newkey

CFLAGS+=-DYP

DPADD=	${LIBRPCSVC} ${LIBMP} ${LIBGMP}
LDADD=	-lrpcsvc -lmp -lgmp

.include <bsd.prog.mk>
