# $FreeBSD: src/gnu/usr.bin/groff/src/devices/grohtml/Makefile,v 1.7.18.1 2008/11/25 02:59:29 kensmith Exp $

PROG_CXX=	post-grohtml
SRCS=		post-html.cpp html-table.cpp html-text.cpp output.cpp
DPADD=		${LIBDRIVER} ${LIBGROFF} ${LIBM}
LDADD=		${LIBDRIVER} ${LIBGROFF} -lm
NO_MAN=

.include <bsd.prog.mk>
