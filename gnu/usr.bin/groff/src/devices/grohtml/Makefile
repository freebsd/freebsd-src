# $FreeBSD: src/gnu/usr.bin/groff/src/devices/grohtml/Makefile,v 1.1.2.2 2002/07/19 08:34:08 ru Exp $

PROG_CXX=	post-grohtml
SRCS=		post-html.cc html-text.cc output.cc
DPADD=		${LIBDRIVER} ${LIBGROFF} ${LIBM}
LDADD=		${LIBDRIVER} ${LIBGROFF} -lm
NOMAN=		YES

.include <bsd.prog.mk>
