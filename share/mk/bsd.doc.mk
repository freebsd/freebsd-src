#	from: @(#)bsd.doc.mk	5.3 (Berkeley) 1/2/91
# $FreeBSD$
#
# The include file <bsd.doc.mk> handles installing BSD troff documents.
#
#
# +++ variables +++
#
# LPR		Printer command. [lpr]
#
# 	[incomplete]
#
# +++ targets +++
#
# 	[incomplete]

.include <bsd.init.mk>

PRINTERDEVICE?=	ascii

BIB?=		bib
GREMLIN?=	grn
GRIND?=		vgrind -f
INDXBIB?=	indxbib
PIC?=		pic
REFER?=		refer
.for _dev in ${PRINTERDEVICE:Mascii}
ROFF.ascii?=	groff -Tascii ${TRFLAGS} -mtty-char ${MACROS} -o${PAGES}
.endfor
.for _dev in ${PRINTERDEVICE:Nascii}
ROFF.${_dev}?=	groff -T${_dev} ${TRFLAGS} ${MACROS} -o${PAGES}
.endfor
SOELIM?=	soelim
TBL?=		tbl

DOC?=		paper
LPR?=		lpr

.if defined(USE_EQN)
TRFLAGS+=	-e
.endif
.if defined(USE_PIC)
TRFLAGS+=	-p
.endif
.if defined(USE_REFER)
TRFLAGS+=	-R
.endif
.if defined(USE_SOELIM)
TRFLAGS+=	-I${SRCDIR}
.endif
.if defined(USE_TBL)
TRFLAGS+=	-t
.endif

DCOMPRESS_EXT?=	${COMPRESS_EXT}
DCOMPRESS_CMD?=	${COMPRESS_CMD}
.for _dev in ${PRINTERDEVICE:Mhtml}
DFILE.html=	${DOC}.html
.endfor
.for _dev in ${PRINTERDEVICE:Nhtml}
.if defined(NODOCCOMPRESS)
DFILE.${_dev}=	${DOC}.${_dev}
.else
DFILE.${_dev}=	${DOC}.${_dev}${DCOMPRESS_EXT}
.endif
.endfor

PAGES?=		1-

UNROFF?=	unroff
HTML_SPLIT?=	yes
UNROFFFLAGS?=	-fhtml
.if ${HTML_SPLIT} == "yes"
UNROFFFLAGS+=	split=1
.endif

# Compatibility mode flag for groff.  Use this when formatting documents with
# Berkeley me macros (orig_me(7)).
COMPAT?=	-C

.PATH: ${.CURDIR} ${SRCDIR}

.for _dev in ${PRINTERDEVICE}
all: ${DFILE.${_dev}}
.endfor

.if !target(print)
.for _dev in ${PRINTERDEVICE}
print: ${DFILE.${_dev}}
.endfor
print:
.for _dev in ${PRINTERDEVICE}
.if defined(NODOCCOMPRESS)
	${LPR} ${DFILE.${_dev}}
.else
	${DCOMPRESS_CMD} -d ${DFILE.${_dev}} | ${LPR}
.endif
.endfor
.endif

.for _dev in ${PRINTERDEVICE:Nascii:Nps:Nhtml}
CLEANFILES+=	${DOC}.${_dev} ${DOC}.${_dev}${DCOMPRESS_EXT}
.endfor
CLEANFILES+=	${DOC}.ascii ${DOC}.ascii${DCOMPRESS_EXT} \
		${DOC}.ps ${DOC}.ps${DCOMPRESS_EXT} \
		${DOC}.html ${DOC}-*.html

realinstall:
.for _dev in ${PRINTERDEVICE:Mhtml}
	cd ${SRCDIR}; \
	    ${INSTALL} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${DOC}*.html ${DESTDIR}${BINDIR}/${VOLUME}
.endfor
.for _dev in ${PRINTERDEVICE:Nhtml}
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    ${DFILE.${_dev}} ${DESTDIR}${BINDIR}/${VOLUME}
.endfor

spell: ${SRCS}
	(cd ${.CURDIR}; spell ${SRCS} ) | sort | \
		comm -23 - ${.CURDIR}/spell.ok > ${DOC}.spell

BINDIR?=	/usr/share/doc
BINMODE=	444

SRCDIR?=	${.CURDIR}

.if defined(EXTRA) && !empty(EXTRA)
_stamp.extra: ${EXTRA}
	touch ${.TARGET}
.endif

CLEANFILES+=	_stamp.extra
.for _dev in ${PRINTERDEVICE:Nhtml}
.if !target(${DFILE.${_dev}})
.if target(_stamp.extra)
${DFILE.${_dev}}: _stamp.extra
.endif
${DFILE.${_dev}}: ${SRCS}
.if defined(NODOCCOMPRESS)
.if defined(CD_HACK)
	(cd ${CD_HACK}; ${ROFF.${_dev}} ${.ALLSRC:N_stamp.extra}) > ${.TARGET}
.else
	${ROFF.${_dev}} ${.ALLSRC:N_stamp.extra} > ${.TARGET}
.endif
.else
.if defined(CD_HACK)
	(cd ${CD_HACK}; ${ROFF.${_dev}} ${.ALLSRC:N_stamp.extra}) | \
	    ${DCOMPRESS_CMD} > ${.TARGET}
.else
	${ROFF.${_dev}} ${.ALLSRC:N_stamp.extra} | ${DCOMPRESS_CMD} > ${.TARGET}
.endif
.endif
.endif
.endfor

.for _dev in ${PRINTERDEVICE:Mhtml}
.if !target(${DFILE.html})
.if target(_stamp.extra)
${DFILE.html}: _stamp.extra
.endif
${DFILE.html}: ${SRCS}
.if defined(MACROS) && !empty(MACROS)
	cd ${SRCDIR}; ${UNROFF} ${MACROS} ${UNROFFFLAGS} \
	    document=${DOC} ${SRCS}
.else # unroff(1) requires a macro package as an argument
	cd ${SRCDIR}; ${UNROFF} -ms ${UNROFFFLAGS} \
	    document=${DOC} ${SRCS}
.else
.endif
.endif
.endfor

DISTRIBUTION?=	doc

.include <bsd.obj.mk>
