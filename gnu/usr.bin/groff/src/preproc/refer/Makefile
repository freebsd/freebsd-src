# $FreeBSD: src/gnu/usr.bin/groff/src/preproc/refer/Makefile,v 1.1.2.1 2001/04/26 17:08:31 ru Exp $

PROG=		refer
SRCS=		command.cc label.cc ref.cc refer.cc token.cc
CFLAGS+=	-I${DIST_DIR}
DPADD=		${LIBBIB} ${LIBGROFF} ${LIBM}
LDADD=		${LIBBIB} ${LIBGROFF} -lm
CLEANFILES=	label.cc label_tab.h ${MAN} y.tab.c y.tab.h

.include <bsd.prog.mk>
