#	$Id: Makefile,v 1.15 1998/03/21 12:12:55 bde Exp $

.include "../Makefile.inc"

TMACOWN?=	${BINOWN}
TMACGRP?=	${BINGRP}
TMACMODE?=	${NOBINMODE}
TMACDIR?=	/usr/share/tmac
MDOCDIR?=	${TMACDIR}/mdoc

MAN7=		groff_ms.7 me.7 
MLINKS=		groff_ms.7 ms.7 me.7 groff_me.7
LINKS=          ${TMACDIR}/tmac.andoc ${TMACDIR}/tmac.an

CLEANFILES=	${MAN7} ${MDOCFILES} ${STRIPFILES} tmac.groff_an

FILES=          tmac.andoc tmac.pic tmac.ps tmac.psnew tmac.psold\
		tmac.pspic tmac.psatk tmac.dvi tmac.tty tmac.tty-char tmac.X\
		tmac.Xps tmac.latin1 tmac.lj4 eqnrc troffrc
STRIPFILES=     tmac.e tmac.s tmac.doc
MDOCFILES=	doc-common doc-ditroff doc-nroff doc-syms

all: ${MDOCFILES} ${STRIPFILES} tmac.groff_an

.for f in ${MDOCFILES} ${STRIPFILES}
$f: ${DIST_DIR}/$f
	sed -f ${DIST_DIR}/strip.sed ${.ALLSRC} > ${.TARGET}
.endfor

tmac.groff_an: tmac.an
	sed -f ${DIST_DIR}/strip.sed ${.ALLSRC} > ${.TARGET}

beforeinstall:
	${INSTALL} ${COPY} -o ${TMACOWN} -g ${TMACGRP} -m ${TMACMODE} \
	    ${MDOCFILES} ${DESTDIR}${MDOCDIR}
	${INSTALL} ${COPY} -o ${TMACOWN} -g ${TMACGRP} -m ${TMACMODE} \
	    ${STRIPFILES} tmac.groff_an ${DESTDIR}${TMACDIR}
	cd ${DIST_DIR}; \
	    ${INSTALL} -c -o ${TMACOWN} -g ${TMACGRP} -m ${TMACMODE} \
	    ${FILES} ${DESTDIR}${TMACDIR}
.if !exists(${DESTDIR}${TMACDIR}/man.local)
	${INSTALL} -c -o ${TMACOWN} -g ${TMACGRP} -m ${TMACMODE} \
	    ${DIST_DIR}/man.local ${DESTDIR}${TMACDIR}
.endif

.include "../Makefile.cfg"
.include <bsd.prog.mk>
