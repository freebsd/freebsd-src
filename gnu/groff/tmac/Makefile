# Makefile for groff macros

TMACOWN?=	bin
TMACGRP?=	bin
TMACMODE?=	444
TMACDIR?=	/usr/share/tmac

MAN7=		groff_ms.7 
MLINKS=		groff_ms.7 ms.7

FILES=		tmac.pic tmac.ps tmac.psnew tmac.psold tmac.pspic tmac.psatk\
		tmac.dvi tmac.tty tmac.tty-char tmac.X tmac.Xps tmac.latin1\
		man.local eqnrc troffrc

afterinstall:
	for f in ${FILES}; do \
		install -c -o ${TMACOWN} -g ${TMACGRP} -m ${TMACMODE} \
			${.CURDIR}/$$f ${DESTDIR}${TMACDIR}; \
	done
	install -c -o ${TMACOWN} -g ${TMACGRP} -m ${TMACMODE} \
		${.CURDIR}/tmac.s ${DESTDIR}${TMACDIR}/tmac.s
	install -c -o ${TMACOWN} -g ${TMACGRP} -m ${TMACMODE} \
		${.CURDIR}/tmac.an ${DESTDIR}${TMACDIR}/tmac.an.old

.include <bsd.prog.mk>
.include "../Makefile.cfg"
