# $FreeBSD$

PROG_CXX=	indxbib
SRCS=		indxbib.cpp signal.c
DPADD=		${LIBBIB} ${LIBGROFF}
LDADD=		${LIBBIB} ${LIBGROFF}
LIBADD=		m
CLEANFILES=	${MAN}
FILES=		${DIST_DIR}/eign
FILESDIR=	${SHAREDIR}/dict/

.include <bsd.prog.mk>
