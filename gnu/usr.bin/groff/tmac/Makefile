# $FreeBSD$

.include "../Makefile.inc"

TMACOWN?=	${BINOWN}
TMACGRP?=	${BINGRP}
TMACMODE?=	${NOBINMODE}
TMACDIR?=	${SHAREDIR}/tmac
MDOCDIR?=	${TMACDIR}/mdoc
LOCALEDIR?=     ${MDOCDIR}/locale

MAN5=		groff_tmac.5
MLINKS+=	groff_tmac.5 tmac.5
MAN7=		groff_ms.7 groff_man.7 groff_me.7 groff_msafer.7 groff_markup.7
MLINKS+=	groff_ms.7 ms.7
MLINKS+=	groff_me.7 me.7
MLINKS+=	groff_msafer.7 msafer.7
MLINKS+=	groff_markup.7 markup.7

CLEANFILES=	${MAN5} ${MAN7} ${MDOCFILES} ${STRIPFILES} ${SPECIALFILES}

FILES=          tmac.mandoc tmac.andoc tmac.pic tmac.ps tmac.psnew tmac.psold \
		tmac.pspic tmac.psatk tmac.dvi tmac.tty tmac.tty-char \
		tmac.X tmac.Xps tmac.latin1 tmac.lj4 eqnrc troffrc troffrc-end \
		tmac.safer tmac.html tmac.markup tmac.arkup tmac.a4 \
		tmac.trace tmac.me tmac.mdoc tmac.s tmac.lbp
STRIPFILES=     tmac.an tmac.e tmac.doc
SPECIALFILES=	tmac.man tmac.ms
MDOCFILES=	doc-common doc-ditroff doc-nroff doc-syms
LOCALEFILES=    locale-list koi8-r

all: ${MDOCFILES} ${STRIPFILES} ${SPECIALFILES}

.for f in ${MDOCFILES} ${STRIPFILES}
$f: ${DIST_DIR}/$f
	sed -f ${DIST_DIR}/strip.sed ${.ALLSRC} > ${.TARGET}
.endfor

.for f in ${SPECIALFILES}
$f: ${DIST_DIR}/$f
	sed -e "s;@TMAC_AN_PREFIX@;$(tmac_an_prefix);g" \
	    -e "s;@TMAC_S_PREFIX@;$(tmac_s_prefix);g" \
		${.ALLSRC} > ${.TARGET}
.endfor

beforeinstall:
	${INSTALL} ${COPY} -o ${TMACOWN} -g ${TMACGRP} -m ${TMACMODE} \
	    ${MDOCFILES} ${DESTDIR}${MDOCDIR}
	${INSTALL} ${COPY} -o ${TMACOWN} -g ${TMACGRP} -m ${TMACMODE} \
	    ${STRIPFILES} ${SPECIALFILES} ${DESTDIR}${TMACDIR}
	cd ${DIST_DIR}; \
	    ${INSTALL} -c -o ${TMACOWN} -g ${TMACGRP} -m ${TMACMODE} \
	    ${FILES} ${DESTDIR}${TMACDIR}
	cd ${DIST_DIR}/locale; \
	    ${INSTALL} -c -o ${TMACOWN} -g ${TMACGRP} -m ${TMACMODE} \
	    ${LOCALEFILES} ${DESTDIR}${LOCALEDIR}
.if !exists(${DESTDIR}${TMACDIR}/man.local)
	${INSTALL} -c -o ${TMACOWN} -g ${TMACGRP} -m ${TMACMODE} \
	    ${DIST_DIR}/man.local ${DESTDIR}${TMACDIR}
.endif

.include "../Makefile.cfg"
.include <bsd.prog.mk>
