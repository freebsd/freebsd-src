# $FreeBSD$

LIB	=	keycap
CAPDIR	=	/usr/share/misc
CAPPATH	=	$(CAPDIR)/keycap.pcvt
KEYCAPSRC=	keycap.src
CFLAGS	+=	-DKEYCAP_PATH=\"$(CAPPATH)\"
SRCS	=	keycap.c
MAN	=	keycap.3
MLINKS+=	keycap.3 kgetent.3 \
		keycap.3 kgetnum.3 \
		keycap.3 kgetflag.3 \
		keycap.3 kgetstr.3
MAN	+=	man5/keycap.5
#CLEANFILES+=	keycap.0 man5/keycap.0

beforeinstall:
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m ${NOBINMODE} \
	    ${.CURDIR}/${KEYCAPSRC} ${DESTDIR}${CAPPATH}

.include <bsd.lib.mk>
