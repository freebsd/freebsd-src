#       bsd.sgml.mk - 8 Sep 1995 John Fieber
#       This file is in the public domain.
#
#	$Id: bsd.sgml.mk,v 1.3.2.1 1995/10/15 08:33:48 jkh Exp $

.if exists(${.CURDIR}/../Makefile.inc)
.include "${.CURDIR}/../Makefile.inc"
.endif

# FORMATS indicates which output formats will be generated.  See
# the sgmlfmt(1) man page for a list of valid formats.  
# If FORMATS is empty, nothing will be built or installed.
# Use SGMLOPTS to pass extra flags to sgmlfmt(1).

FORMATS?=	ascii html
SGMLFLAGS+=	${SGMLOPTS}

VOLUME?=	${.CURDIR:T}
DOC?=		${.CURDIR:T}
BINDIR?=	/usr/share/doc
SRCDIR?=	${.CURDIR}
DISTRIBUTION?=	bin
SGMLFMT?=	sgmlfmt
LPR?=		lpr

DOCS=	${FORMATS:S/^/${DOC}./g}

.MAIN:	all
all:	${DOCS}

# If FORMATS is empty, do nothing
.if empty(FORMATS)
${DOC}. install- print- clean-:
.endif

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

clean: ${FORMATS:S/^/clean-/g}
	rm -f [eE]rrs mklog

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


# For each FORMATS type, define a build, install, clean and print target.
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
	${INSTALL} ${COPY} -o ${MANOWN} -g ${MANGRP} -m ${MANMODE} \
		*.${.TARGET:S/install-//} ${DESTDIR}${BINDIR}/${VOLUME}

.else
install-${_FORMAT}: ${DOC}.${_FORMAT}
	${INSTALL} ${COPY} -o ${MANOWN} -g ${MANGRP} -m ${MANMODE} \
		${DOC}.${.TARGET:S/install-//} ${DESTDIR}${BINDIR}/${VOLUME}

.endif
.endif

.if !target(${DOC}.${_FORMAT})
${DOC}.${_FORMAT}:	${SRCS}
	${SGMLFMT} -f ${.TARGET:S/${DOC}.//} ${SGMLFLAGS} ${.CURDIR}/${DOC}.sgml

.endif

.if !target(clean-${_FORMAT})
.if ${_FORMAT} == "html"
clean-${_FORMAT}:
	rm -f *.${.TARGET:S/clean-//}

.else
clean-${_FORMAT}: 
	rm -f ${DOC}.${.TARGET:S/clean-//}

.endif
.endif

.endfor
