#	from: @(#)bsd.doc.mk	5.3 (Berkeley) 1/2/91
#	$Id: bsd.doc.mk,v 1.25 1996/06/24 04:23:54 jkh Exp $

PRINTER?=	ascii

BIB?=		bib
EQN?=		eqn -T${PRINTER}
GREMLIN?=	grn
GRIND?=		vgrind -f
INDXBIB?=	indxbib
PIC?=		pic
REFER?=		refer
.if ${PRINTER} == "ascii"
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

TRFLAGS+=	-T${PRINTER}
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
TRFALGS+=	-R
.endif

.if defined(NODOCCOMPRESS)
DFILE=	${DOC}.${PRINTER}
GZIPCMD=	cat
.else
DFILE=	${DOC}.${PRINTER}.gz
GZIPCMD=	gzip -c
.endif

PAGES?=		1-

# Compatibility mode flag for groff.  Use this when formatting documents with
# Berkeley me macros.
COMPAT?=	-C

.PATH: ${.CURDIR} ${SRCDIR}

.MAIN:	all
all:	${DFILE}

.if !target(print)
print: ${DFILE}
.if defined(NODOCCOMPRESS)
	lpr -P${PRINTER} ${DFILE}
.else
	${GZIPCMD} -d ${DFILE} | lpr -P${PRINTER}
.endif
.endif

clean:
	rm -f ${DOC}.${PRINTER} ${DOC}.ps ${DOC}.ascii \
		${DOC}.ps.gz ${DOC}.ascii.gz Errs errs mklog ${CLEANFILES}

FILES?=	${SRCS}
realinstall:
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${DFILE} ${DESTDIR}${BINDIR}/${VOLUME}

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
	cd ${.CURDIR} ; $(MAKE) install DESTDIR=${DISTDIR}/${DISTRIBUTION} SHARED=copies
.endif

spell: ${SRCS}
	(cd ${.CURDIR};  spell ${SRCS} ) | sort | \
		comm -23 - ${.CURDIR}/spell.ok > ${DOC}.spell

BINDIR?=	/usr/share/doc
BINMODE=        444

SRCDIR?=	${.CURDIR}

.if !target(${DFILE})
${DFILE}::	${SRCS} ${EXTRA} ${OBJS}
# XXX ${.ALLSRC} doesn't work unless there are a lot of .PATH.foo statements.
ALLSRCS=	${SRCS:S;^;${SRCDIR}/;}
${DFILE}::	${SRCS}
.if defined(USE_SOELIMPP)
	${SOELIMPP} ${ALLSRCS} | ${ROFF} | ${GZIPCMD} > ${.TARGET}
.else
	(cd ${SRCDIR}; ${ROFF} ${.ALLSRC}) | ${GZIPCMD} > ${.TARGET}
.endif
.else
.if !defined(NODOCCOMPRESS)
${DFILE}:	${DOC}.${PRINTER}
	${GZIPCMD} ${DOC}.${PRINTER} > ${.TARGET}
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
