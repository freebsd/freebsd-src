# Makefile for indxbib

PROG=		indxbib
SRCS=		indxbib.cc dirnamemax.c signal.c
CFLAGS+=	-I${.CURDIR}/../include
LDADD+= 	${LIBBIB} ${LIBGROFF} -lm
DPADD+= 	${LIBBIB} ${LIBGROFF} ${LIBM}

MANDEPEND=	indxbib.1
CLEANFILES+=	${MANDEPEND}

beforeinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${NOBINMODE} \
	    ${DIST_DIR}/eign ${DESTDIR}${SHAREDIR}/dict/

.include "../Makefile.cfg"
.include <bsd.prog.mk>
