#	$Id: Makefile,v 1.2 1997/12/16 14:42:08 bde Exp $

PROG=	chkey
SRCS=	chkey.c update.c generic.c

.PATH: ${.CURDIR}/../newkey

CFLAGS+=-DYP

DPADD=	${LIBRPCSVC} ${LIBMP} ${LIBGMP}
LDADD=	-lrpcsvc -lmp -lgmp

.include <bsd.prog.mk>
