# $FreeBSD: src/gnu/usr.bin/groff/src/devices/grohtml/Makefile,v 1.1.2.1 2001/04/26 17:08:23 ru Exp $

PROG=		post-grohtml
SRCS=		post-html.cc html-text.cc output.cc
DPADD=		${LIBDRIVER} ${LIBGROFF} ${LIBM}
LDADD=		${LIBDRIVER} ${LIBGROFF} -lm
NOMAN=		YES

.include <bsd.prog.mk>
