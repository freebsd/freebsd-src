# Makefile for groff macros

TMACOWN?=	bin
TMACGRP?=	bin
TMACMODE?=	444
TMACDIR?=	/usr/share/tmac
MDOCDIR?=	$(TMACDIR)/mdoc

MAN7=		groff_ms.7 me.7
MLINKS=		groff_ms.7 ms.7

MANDEPEND=	${MAN7}
CLEANFILES+=    temp ${MANDEPEND} tmac.groff_me tmac.groff_ms tmac.groff_an \
		tmac.an

FILES=          tmac.pic tmac.ps tmac.psnew tmac.psold\
		tmac.pspic tmac.psatk tmac.dvi tmac.tty tmac.tty-char tmac.X\
		tmac.Xps tmac.latin1 eqnrc troffrc
STRIPFILES=     tmac.doc tmac.doc.old
MDOCFILES=	doc-common doc-ditroff doc-nroff doc-syms


afterinstall:
	for f in ${FILES}; do \
		install -c -o ${TMACOWN} -g ${TMACGRP} -m ${TMACMODE} \
			${.CURDIR}/$$f ${DESTDIR}${TMACDIR}; \
	done
	sed -f ${.CURDIR}/strip.sed ${.CURDIR}/tmac.e >tmac.groff_me
	install -c -o ${TMACOWN} -g ${TMACGRP} -m ${TMACMODE} \
		tmac.groff_me ${DESTDIR}${TMACDIR}
	sed -f ${.CURDIR}/strip.sed ${.CURDIR}/tmac.s >tmac.groff_ms
	install -c -o ${TMACOWN} -g ${TMACGRP} -m ${TMACMODE} \
		tmac.groff_ms ${DESTDIR}${TMACDIR}
	sed -f ${.CURDIR}/strip.sed ${.CURDIR}/tmac.an >tmac.groff_an
	install -c -o ${TMACOWN} -g ${TMACGRP} -m ${TMACMODE} \
		tmac.groff_an ${DESTDIR}${TMACDIR}
	sed -f ${.CURDIR}/strip.sed ${.CURDIR}/tmac.andoc >tmac.an
	install -c -o ${TMACOWN} -g ${TMACGRP} -m ${TMACMODE} \
		tmac.an ${DESTDIR}${TMACDIR}
	for f in ${STRIPFILES}; do \
		rm -f temp; \
	  	sed -f ${.CURDIR}/strip.sed ${.CURDIR}/$$f >temp; \
		install -c -o ${TMACOWN} -g ${TMACGRP} -m ${TMACMODE} \
			temp ${DESTDIR}${TMACDIR}/$$f; \
	done
	for f in ${MDOCFILES}; do \
		rm -f temp; \
		sed -f ${.CURDIR}/strip.sed ${.CURDIR}/$$f >temp; \
		install -c -o ${TMACOWN} -g ${TMACGRP} -m ${TMACMODE} \
			temp ${DESTDIR}${MDOCDIR}/$$f; \
	done
	-test -f ${DESTDIR}${TMACDIR}/man.local || \
		install -c -o ${TMACOWN} -g ${TMACGRP} -m ${TMACMODE} \
			${.CURDIR}/man.local ${DESTDIR}${MDOCDIR}

.include "../Makefile.cfg"
.include <bsd.prog.mk>
