#	from: @(#)bsd.doc.mk	5.3 (Berkeley) 1/2/91
#	$Id: bsd.doc.mk,v 1.37 1997/05/11 10:42:00 wosch Exp $
#
# The include file <bsd.doc.mk> handles installing BSD troff documents.
# <bsd.prog.mk> includes the include files <bsd.dep.mk> and <bsd.obj.mk>.
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

PRINTERDEVICE?=	ascii

BIB?=		bib
EQN?=		eqn -T${PRINTERDEVICE}
GREMLIN?=	grn
GRIND?=		vgrind -f
INDXBIB?=	indxbib
PIC?=		pic
REFER?=		refer
.if ${PRINTERDEVICE} == "ascii"
ROFF?=          groff -mtty-char ${TRFLAGS} ${MACROS} -o${PAGES}
.else
ROFF?=		groff ${TRFLAGS} ${MACROS} -o${PAGES}
.endif
SOELIM?=	soelim
SOELIMPP=	sed ${SOELIMPPARGS}
SOELIMPPARGS0=	${SRCS} ${EXTRA}
SOELIMPPARGS1=	${SOELIMPPARGS0:S/^/-e\\ \'s:\(\.so[\\ \\	][\\ \\	]*\)\(/}
SOELIMPPARGS2=	${SOELIMPPARGS1:S/$/\)\$:\1${SRCDIR}\/\2:\'/}
SOELIMPPARGS=	${SOELIMPPARGS2:S/\\'/'/g}
TBL?=		tbl

DOC?=		paper
LPR?=		lpr

TRFLAGS+=	-T${PRINTERDEVICE}
.if defined(USE_EQN)
TRFLAGS+=	-e
.endif
.if defined(USE_TBL)
TRFLAGS+=	-t
.endif
.if defined(USE_PIC)
TRFLAGS+=	-p
.endif
.if defined(USE_SOELIM)
TRFLAGS+=	-s
.endif
.if defined(USE_REFER)
TRFLAGS+=	-R
.endif

DCOMPRESS_EXT?=	${COMPRESS_EXT}
.if defined(NODOCCOMPRESS) || ${PRINTERDEVICE} == "html"
DFILE=	${DOC}.${PRINTERDEVICE}
DCOMPRESS_CMD=	cat
.else
DFILE=	${DOC}.${PRINTERDEVICE}${DCOMPRESS_EXT}
DCOMPRESS_CMD?=	${COMPRESS_CMD}
.endif

PAGES?=		1-

UNROFF?=	unroff
HTML_SPLIT?=	yes
UNROFFFLAGS?=	-fhtml
.if ${HTML_SPLIT} == "yes"
UNROFFFLAGS+=	split=1
.endif

# Compatibility mode flag for groff.  Use this when formatting documents with
# Berkeley me macros.
COMPAT?=	-C

.PATH: ${.CURDIR} ${SRCDIR}

.MAIN:	all
all:	${DFILE}

.if !target(print)
print: ${DFILE}
.if defined(NODOCCOMPRESS)
	${LPR} ${DFILE}
.else
	${DCOMPRESS_CMD} -d ${DFILE} | ${LPR}
.endif
.endif

CLEANFILES+=	${DOC}.${PRINTERDEVICE} \
		${DOC}.${PRINTERDEVICE}${DCOMPRESS_EXT} \
		${DOC}.ascii ${DOC}.ascii${DCOMPRESS_EXT} \
		${DOC}.ps ${DOC}.ps${DCOMPRESS_EXT} \
		${DOC}.html ${DOC}-*.html


FILES?=	${SRCS}
realinstall:
.if ${PRINTERDEVICE} == "html"
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${DOC}*.html ${DESTDIR}${BINDIR}/${VOLUME}
.else
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${DFILE} ${DESTDIR}${BINDIR}/${VOLUME}
.endif

install:	beforeinstall realinstall afterinstall

.if !target(beforeinstall)
beforeinstall:

.endif
.if !target(afterinstall)
afterinstall:

.endif

DISTRIBUTION?=	doc
.if !target(distribute)
distribute:
.for dist in ${DISTRIBUTION}
	cd ${.CURDIR} ; $(MAKE) install DESTDIR=${DISTDIR}/${dist} SHARED=copies
.endfor
.endif

spell: ${SRCS}
	(cd ${.CURDIR};  spell ${SRCS} ) | sort | \
		comm -23 - ${.CURDIR}/spell.ok > ${DOC}.spell

BINDIR?=	/usr/share/doc
BINMODE=        444

SRCDIR?=	${.CURDIR}

.if !target(${DFILE})
.if ${PRINTERDEVICE} == "html"
${DFILE}:	${SRCS} ${EXTRA} ${OBJS}
	cd ${SRCDIR}; ${UNROFF} ${MACROS} ${UNROFFFLAGS} \
		document=${.OBJDIR}/${DOC} ${SRCS}
.else

${DFILE}::	${SRCS} ${EXTRA} ${OBJS}
# XXX ${.ALLSRC} doesn't work unless there are a lot of .PATH.foo statements.
ALLSRCS=	${SRCS:S;^;${SRCDIR}/;}
${DFILE}::	${SRCS}
.if defined(USE_SOELIMPP)
	${SOELIMPP} ${ALLSRCS} | ${ROFF} | ${DCOMPRESS_CMD} > ${.TARGET}
.else
	(cd ${SRCDIR}; ${ROFF} ${.ALLSRC}) | ${DCOMPRESS_CMD} > ${.TARGET}
.endif
.else
.if !defined(NODOCCOMPRESS)
${DFILE}:	${DOC}.${PRINTERDEVICE}
	${DCOMPRESS_CMD} ${DOC}.${PRINTERDEVICE} > ${.TARGET}
.endif
.endif
.endif

.if !target(depend)
depend:
.endif

.if !target(maninstall)
maninstall:
.endif

.include <bsd.dep.mk>
.include <bsd.obj.mk>
