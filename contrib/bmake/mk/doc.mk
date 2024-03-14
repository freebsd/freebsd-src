# $Id: doc.mk,v 1.9 2024/02/19 00:06:19 sjg Exp $

# should be set properly in sys.mk
_this ?= ${.PARSEFILE:S,bsd.,,}

.if !target(__${_this}__)
__${_this}__: .NOTMAIN

.include <init.mk>

BIB?=		bib
EQN?=		eqn
GREMLIN?=	grn
GRIND?=		vgrind -f
INDXBIB?=	indxbib
PIC?=		pic
REFER?=		refer
ROFF?=		groff -M/usr/share/tmac ${MACROS} ${PAGES}
SOELIM?=	soelim
TBL?=		tbl

.PATH: ${.CURDIR}

.if !defined(_SKIP_BUILD)
realbuild: paper.ps
.endif

.if !target(paper.ps)
paper.ps: ${SRCS}
	${ROFF} ${SRCS} > ${.TARGET}
.endif

.if !target(print)
print: paper.ps
	lpr -P${PRINTER} paper.ps
.endif

.if !target(manpages)
manpages:
.endif

.if !target(obj)
obj:
.endif

clean cleandir:
	rm -f paper.* [eE]rrs mklog ${CLEANFILES}

.if ${MK_DOC} == "no"
install:
.else
FILES?=	${SRCS}
install:
	test -d ${DESTDIR}${DOCDIR}/${DIR} || \
	    ${INSTALL} -d ${DOC_INSTALL_OWN} -m ${DIRMODE} ${DESTDIR}${DOCDIR}/${DIR}
	${INSTALL} ${COPY} ${DOC_INSTALL_OWN} -m ${DOCMODE} \
	    Makefile ${FILES} ${EXTRA} ${DESTDIR}${DOCDIR}/${DIR}
.endif

spell: ${SRCS}
	spell ${SRCS} | sort | comm -23 - spell.ok > paper.spell

.if !empty(DOCOWN)
DOC_INSTALL_OWN?= -o ${DOCOWN} -g ${DOCGRP}
.endif

.endif
