# $FreeBSD$

PROG_CXX=	post-grohtml
SRCS=		post-html.cc html-table.cc html-text.cc output.cc
DPADD=		${LIBDRIVER} ${LIBGROFF} ${LIBM}
LDADD=		${LIBDRIVER} ${LIBGROFF} -lm
NOMAN=		YES

.include <bsd.prog.mk>
