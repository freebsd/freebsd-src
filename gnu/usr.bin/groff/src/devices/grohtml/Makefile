# $FreeBSD: src/gnu/usr.bin/groff/src/devices/grohtml/Makefile,v 1.1.2.3 2003/02/14 15:22:13 ru Exp $

PROG_CXX=	post-grohtml
SRCS=		post-html.cc html-table.cc html-text.cc output.cc
DPADD=		${LIBDRIVER} ${LIBGROFF} ${LIBM}
LDADD=		${LIBDRIVER} ${LIBGROFF} -lm
NOMAN=		YES

.include <bsd.prog.mk>
