# $FreeBSD: src/gnu/usr.bin/groff/src/preproc/refer/Makefile,v 1.1.2.2 2002/07/19 08:34:10 ru Exp $

PROG_CXX=	refer
SRCS=		command.cc label.cc ref.cc refer.cc token.cc
CFLAGS+=	-I${DIST_DIR}
DPADD=		${LIBBIB} ${LIBGROFF} ${LIBM}
LDADD=		${LIBBIB} ${LIBGROFF} -lm
CLEANFILES=	label.cc label_tab.h ${MAN} y.tab.c y.tab.h

.include <bsd.prog.mk>
