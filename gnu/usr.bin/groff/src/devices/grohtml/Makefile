# $FreeBSD: src/gnu/usr.bin/groff/src/devices/grohtml/Makefile,v 1.7.16.1 2008/10/02 02:57:24 kensmith Exp $

PROG_CXX=	post-grohtml
SRCS=		post-html.cpp html-table.cpp html-text.cpp output.cpp
DPADD=		${LIBDRIVER} ${LIBGROFF} ${LIBM}
LDADD=		${LIBDRIVER} ${LIBGROFF} -lm
NO_MAN=

.include <bsd.prog.mk>
