# $FreeBSD$

LIB=	keycap
MAN=	keycap.3
MLINKS+= keycap.3 kgetent.3 \
	keycap.3 kgetnum.3 \
	keycap.3 kgetflag.3 \
	keycap.3 kgetstr.3
MAN+=	man5/keycap.5
SRCS=	keycap.c

KEYCAPSRC=	keycap.src
CAPDIR=	/usr/share/misc
CAPPATH=	$(CAPDIR)/keycap.pcvt

CFLAGS+= -DKEYCAP_PATH=\"$(CAPPATH)\"

#CLEANFILES+=	keycap.0 man5/keycap.0

beforeinstall:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${NOBINMODE} \
	    ${.CURDIR}/${KEYCAPSRC} ${DESTDIR}${CAPPATH}

.include <bsd.lib.mk>
