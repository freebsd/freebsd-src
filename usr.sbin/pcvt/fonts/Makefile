#	$Id$

.SUFFIXES: .uu
.uu:
	rm -f ${.TARGET}
	uudecode ${.IMPSRC}

FONTS=	vt220h.808 vt220h.810 vt220h.814 vt220h.816 \
	vt220l.808 vt220l.810 vt220l.814 vt220l.816
#FONTS+=	vt100pc.814 vt100sg.814
CLEANFILES= 	${FONTS}

all: ${FONTS}

beforeinstall:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${NOBINMODE} \
	    ${FONTS} ${DESTDIR}${FONTDIR}

.include <bsd.prog.mk>
