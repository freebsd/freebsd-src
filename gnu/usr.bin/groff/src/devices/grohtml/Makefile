# $FreeBSD$

PROG=		post-grohtml
SRCS=		post-html.cc html-text.cc output.cc
DPADD=		${LIBDRIVER} ${LIBGROFF} ${LIBM} ${LIBSTDCPLUSPLUS}
LDADD=		${LIBDRIVER} ${LIBGROFF} -lm -lstdc++
NOMAN=		YES

.include <bsd.prog.mk>
