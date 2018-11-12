# $FreeBSD$

.include <bsd.own.mk>			# SHAREDIR

TMACOWN?=	${BINOWN}
TMACGRP?=	${BINGRP}
TMACMODE?=	${NOBINMODE}
TMACDIR?=	${SHAREDIR}/tmac
MDOCDIR?=	${TMACDIR}/mdoc

MAN=		groff_ms.7 groff_man.7 groff_me.7 groff_mdoc.7
MAN+=		groff_trace.7 groff_www.7
MLINKS=		groff_ms.7 ms.7
MLINKS+=	groff_me.7 me.7
MLINKS+=	groff_mdoc.7 mdoc.samples.7

CLEANFILES=	${MAN}

NORMALFILES=	mandoc.tmac andoc.tmac an-old.tmac \
		me.tmac \
		mdoc.tmac \
		pic.tmac \
		a4.tmac \
		papersize.tmac \
		ec.tmac \
		safer.tmac \
		trace.tmac \
		ps.tmac psold.tmac pspic.tmac psatk.tmac \
		dvi.tmac \
		tty.tmac tty-char.tmac \
		latin1.tmac latin2.tmac latin9.tmac cp1047.tmac \
		unicode.tmac \
		X.tmac Xps.tmac \
		lj4.tmac \
		lbp.tmac \
		html.tmac html-end.tmac \
		devtag.tmac \
		europs.tmac \
		composite.tmac \
		eqnrc \
		troffrc troffrc-end \
		koi8-r.tmac hyphen.ru \
		hyphen.us hyphenex.us
# These are all generated into the OBJDIR.
SPECIALFILES=	an.tmac man.tmac s.tmac ms.tmac www.tmac
STRIPFILES=	e.tmac doc.tmac mdoc.local
MDOCFILES=	doc-common doc-ditroff doc-nroff doc-syms \
		fr.ISO8859-1 ru.KOI8-R
# These are in srcdir and must be built special to avoid colliding with
# CURDIR=OBJDIR.
SRCFILES=	fr.ISO8859-1 mdoc.local ru.KOI8-R
CLEANFILES+=	${SRCFILES}

FILESGROUPS=	FILES
FILES=		${NORMALFILES}
FILESOWN=	${TMACOWN}
FILESGRP=	${TMACGRP}
FILESMODE=	${TMACMODE}
FILESDIR=	${TMACDIR}
MDOCFILESDIR=	${MDOCDIR}

# Setup handling for the generated and special file groups
.for var in SPECIAL STRIP MDOC
FILESGROUPS+=	${var}FILES
CLEANFILES+=	${${var}FILES}
${var}FILESOWN?=${TMACOWN}
${var}FILESGRP?=${TMACGRP}
${var}FILESMODE?=${TMACMODE}
${var}FILESDIR?=${TMACDIR}
.endfor

beforeinstall:
.if !exists(${DESTDIR}${TMACDIR}/man.local)
	${INSTALL} -o ${TMACOWN} -g ${TMACGRP} -m ${TMACMODE} \
	    ${DIST_DIR}/man.local ${DESTDIR}${TMACDIR}
.endif

.include <bsd.prog.mk>

# Do this after ../Makefile.inc gets included so DIST_DIR is defined.
.for f in ${MDOCFILES} ${STRIPFILES}
# Generate the file from the contrib dir or src dir as needed.
.if ${SRCFILES:M${f}} != ""
${f}: ${.CURDIR}/${f}.in
.else
${f}: ${DIST_DIR}/${f}
.endif
	sed -f ${DIST_DIR}/strip.sed ${.ALLSRC} > ${.TARGET}
.endfor

.for f in ${SPECIALFILES}
${f}: ${DIST_DIR}/${f}
	sed -e "s;@TMAC_AN_PREFIX@;${tmac_an_prefix};g" \
	    -e "s;@TMAC_S_PREFIX@;${tmac_s_prefix};g" \
	    -e "s;@PNMTOPS_NOSETPAGE@;pnmtops;g" \
		${.ALLSRC} > ${.TARGET}
.endfor
