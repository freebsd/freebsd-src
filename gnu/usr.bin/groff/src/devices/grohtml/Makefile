# $FreeBSD: src/gnu/usr.bin/groff/src/devices/grohtml/Makefile,v 1.1.2.4 2003/06/26 13:13:53 ru Exp $

PROG_CXX=	post-grohtml
SRCS=		post-html.cpp html-table.cpp html-text.cpp output.cpp
DPADD=		${LIBDRIVER} ${LIBGROFF} ${LIBM}
LDADD=		${LIBDRIVER} ${LIBGROFF} -lm
NOMAN=		YES

.include <bsd.prog.mk>
