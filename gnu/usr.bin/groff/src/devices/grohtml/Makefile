# $FreeBSD: src/gnu/usr.bin/groff/src/devices/grohtml/Makefile,v 1.4 2002/10/11 08:58:14 ru Exp $

PROG_CXX=	post-grohtml
SRCS=		post-html.cc html-table.cc html-text.cc output.cc
DPADD=		${LIBDRIVER} ${LIBGROFF} ${LIBM}
LDADD=		${LIBDRIVER} ${LIBGROFF} -lm
NOMAN=		YES

.include <bsd.prog.mk>
