# $FreeBSD: src/gnu/usr.bin/groff/src/devices/grohtml/Makefile,v 1.5.6.1 2005/02/13 07:23:09 obrien Exp $

PROG_CXX=	post-grohtml
SRCS=		post-html.cpp html-table.cpp html-text.cpp output.cpp
DPADD=		${LIBDRIVER} ${LIBGROFF} ${LIBM}
LDADD=		${LIBDRIVER} ${LIBGROFF} -lm
NOMAN=

.include <bsd.prog.mk>
