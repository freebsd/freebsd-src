# Makefile for groff macros

TMACOWN?=	bin
TMACGRP?=	bin
TMACMODE?=	444
TMACDIR?=	/usr/share/tmac
MDOCDIR?=	${TMACDIR}/mdoc

MAN7=		groff_ms.7 me.7 
MLINKS=		groff_ms.7 ms.7 me.7 groff_me.7
LINKS=          ${TMACDIR}/tmac.andoc ${TMACDIR}/tmac.an

MANDEPEND=	${MAN7}
CLEANFILES+=    temp ${MANDEPEND} tmac.groff_an

FILES=          tmac.andoc tmac.pic tmac.ps tmac.psnew tmac.psold\
		tmac.pspic tmac.psatk tmac.dvi tmac.tty tmac.tty-char tmac.X\
		tmac.Xps tmac.latin1 tmac.lj4 eqnrc troffrc
STRIPFILES=     tmac.e tmac.s tmac.doc
MDOCFILES=	doc-common doc-ditroff doc-nroff doc-syms


beforeinstall:
	for f in ${FILES}; do \
		${INSTALL} -c -o ${TMACOWN} -g ${TMACGRP} -m ${TMACMODE} \
			${DIST_DIR}/$$f ${DESTDIR}${TMACDIR}; \
	done
	sed -f ${DIST_DIR}/strip.sed ${DIST_DIR}/tmac.an >tmac.groff_an
	${INSTALL} ${COPY} -o ${TMACOWN} -g ${TMACGRP} -m ${TMACMODE} \
		tmac.groff_an ${DESTDIR}${TMACDIR}
	for f in ${STRIPFILES}; do \
		rm -f temp; \
	  	sed -f ${DIST_DIR}/strip.sed ${DIST_DIR}/$$f >temp; \
		${INSTALL} ${COPY} -o ${TMACOWN} -g ${TMACGRP} -m ${TMACMODE} \
			temp ${DESTDIR}${TMACDIR}/$$f; \
	done
	for f in ${MDOCFILES}; do \
		rm -f temp; \
		sed -f ${DIST_DIR}/strip.sed ${DIST_DIR}/$$f >temp; \
		${INSTALL} ${COPY} -o ${TMACOWN} -g ${TMACGRP} -m ${TMACMODE} \
			temp ${DESTDIR}${MDOCDIR}/$$f; \
	done
	test -f ${DESTDIR}${TMACDIR}/man.local || \
		${INSTALL} -c -o ${TMACOWN} -g ${TMACGRP} -m ${TMACMODE} \
			${DIST_DIR}/man.local ${DESTDIR}${TMACDIR}

.include "../Makefile.cfg"
.include <bsd.prog.mk>
