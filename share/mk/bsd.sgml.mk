#       bsd.sgml.mk - 8 Sep 1995 John Fieber
#       This file is in the public domain.
#
#	$Id$

.if exists(${.CURDIR}/../Makefile.inc)
.include "${.CURDIR}/../Makefile.inc"
.endif

# Variables of possible interest to the user


# FORMATS indicates which output formats will be generated.  See
# the sgmlfmt(1) man page for a list of valid formats.  
# Specify "null" for FORMATS to build and install nothing.

FORMATS?=	ascii html

VOLUME?=	${.CURDIR:T}
DOC?=		${.CURDIR:T}
SGMLFMT?=	sgmlfmt
LPR?=		lpr
SGMLFLAGS+=	${SGMLOPTS}
BINDIR?=	/usr/share/doc
BINMODE?=	444
SRCDIR?=	${.CURDIR}
DISTRIBUTION?=	doc

# Everything else

DOCS=	${FORMATS:S/^/${DOC}./g}

.MAIN:	all
all:	${DOCS}

# Empty targets for FORMATS= null. 
${DOC}.null:
install-null:
print-null:

.if !target(obj)
.if defined(NOOBJ)
obj:
.else
obj:
	@cd ${.CURDIR}; rm -f obj; \
	here=`pwd`; dest=/usr/obj`echo $$here | sed 's,^/usr/src,,'`; \
	${ECHO} "$$here -> $$dest"; ln -s $$dest obj; \
	if test -d /usr/obj -a ! -d $$dest; then \
		mkdir -p $$dest; \
	else \
		true; \
	fi;
.endif
.endif

clean:
	rm -f [eE]rrs mklog ${CLEANFILES}

cleandir: clean
	cd ${.CURDIR}; rm -rf obj

install:	beforeinstall realinstall afterinstall

.if !target(beforeinstall)
beforeinstall:

.endif
.if !target(afterinstall)
afterinstall:

.endif
.if !target(maninstall)
maninstall:

.endif

realinstall: ${FORMATS:S/^/install-/g}

.if !target(print)
print: ${FORMATS:S/^/print-/g}

.endif

spell: ${SRCS}
	(cd ${.CURDIR};  spell ${SRCS} ) | sort | \
		comm -23 - ${.CURDIR}/spell.ok > ${DOC}.spell

.if !target(distribute)
distribute:
	cd ${.CURDIR} ; $(MAKE) install DESTDIR=${DISTDIR}/${DISTRIBUTION} SHARED=copies
.endif

.if !target(depend)
depend:

.endif


# For each FORMATS type, define a build, install and print target.
# Note that there is special case handling for html targets
# because the number of files generated is generally not possible
# to predict outside of sgmlfmt(1).

.for _XFORMAT in ${FORMATS}

# XXX This doesn't work:
#    .if ${_FORMAT} == "foobar"
# but defining another variable does:  (?!?!)

_FORMAT = ${_XFORMAT}

.if !target(print-${_FORMAT})
.if ${_FORMAT} == "html"
print-${_FORMAT}:

.else
print-${_FORMAT}: ${DOC}.${_FORMAT}
	${LPR} -P${.TARGET:S/print-//} ${DOC}.${_FORMAT}

.endif
.endif

.if !target(install-${_FORMAT})
.if ${_FORMAT} == "html"
install-${_FORMAT}: ${DOC}.${_FORMAT}
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		*.${.TARGET:S/install-//} ${DESTDIR}${BINDIR}/${VOLUME}

.else
install-${_FORMAT}: ${DOC}.${_FORMAT}
	${INSTALL} ${COPY} -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
		${DOC}.${.TARGET:S/install-//} ${DESTDIR}${BINDIR}/${VOLUME}

.endif
.endif

.if !target(${DOC}.${_FORMAT})
.if ${_FORMAT} == "html"
CLEANFILES+=	*.${_FORMAT}
.else
CLEANFILES+=	${DOC}.${_FORMAT}
.endif

${DOC}.${_FORMAT}:	${SRCS}
	(cd ${SRCDIR}; ${SGMLFMT} -f ${.TARGET:S/${DOC}.//} ${SGMLFLAGS} ${DOC})

.endif

.endfor
