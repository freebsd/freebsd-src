# $FreeBSD: src/gnu/usr.bin/groff/src/devices/grohtml/Makefile,v 1.7.22.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG_CXX=	post-grohtml
SRCS=		post-html.cpp html-table.cpp html-text.cpp output.cpp
DPADD=		${LIBDRIVER} ${LIBGROFF} ${LIBM}
LDADD=		${LIBDRIVER} ${LIBGROFF} -lm
NO_MAN=

.include <bsd.prog.mk>
