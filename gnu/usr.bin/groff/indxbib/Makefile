# Makefile for indxbib

PROG=		indxbib
SRCS=		indxbib.cc dirnamemax.c signal.c
CFLAGS+=	-I${.CURDIR}/../include
LDADD+= 	${LIBBIB} ${LIBGROFF} -lm
DPADD+= 	${LIBBIB} ${LIBGROFF} ${LIBM}

MANDEPEND=	indxbib.1
CLEANFILES+=	${MANDEPEND}

# Ugh - I can't grab /usr/share from ../../../share/Makefile.inc
# since I've already grabbed a BINDIR file for installing indxbib.
# Sigh... Hardcode it.  	- jkh
#
afterinstall:
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 \
		${DISTDIR}/eign ${DESTDIR}${SHAREDIR}/dict

.include "../Makefile.cfg"
.include <bsd.prog.mk>
