#       bsd.sgml.mk - 8 Sep 1995 John Fieber
#       This file is in the public domain.
#
#	$FreeBSD$
#
# The include file <bsd.sgml.mk> handles installing sgml documents.
# <bsd.prog.mk> includes the file named "../Makefile.inc" if it exists,
# as well as the include files <bsd.dep.mk> and <bsd.obj.mk>.
#
#
# +++ variables +++
#
# DISTRIBUTION	Name of distribution. [doc]
#
# FORMATS 	Indicates which output formats will be generated
#		(ascii, html, latex, latin1, nroff). [html latin1]
#
# LPR		Printer command. [lpr]
#
# SGMLFLAGS	Flags to sgmlfmt. [${SGMLOPTS}]
#
# SGMLFMT	Format sgml files command. [sgmlfmt]
#
# Variables DOCOWN, DOCGRP, DOCMODE, DOCDIR, DESTDIR, DISTDIR are
# set by other Makefiles (e.g. bsd.own.mk)
#
#
# +++ targets +++
#
#	all:
#		Converts sgml files to the specified output format
#		(see ${FORMATS}).
#
#	distribute:
# 		This is a variant of install, which will
# 		put the stuff into the right "distribution".
#
#	install:
#		Install formated output.
#
#	print:
#		Print formated output.
#
#
# bsd.obj.mk: clean, cleandir, obj
#


.if exists(${.CURDIR}/../Makefile.inc)
.include "${.CURDIR}/../Makefile.inc"
.endif

# FORMATS indicates which output formats will be generated.  See
# the sgmlfmt(1) man page for a list of valid formats.  
# If FORMATS is empty, nothing will be built or installed.
# Use SGMLOPTS to pass extra flags to sgmlfmt(1).

FORMATS?=	html latin1
SGMLFLAGS+=	${SGMLOPTS}

VOLUME?=	${.CURDIR:T}
DOC?=		${.CURDIR:T}
SRCDIR?=	${.CURDIR}
DISTRIBUTION?=	doc
SGMLFMT?=	sgmlfmt
LPR?=		lpr

DOCS=	${FORMATS:S/^/${DOC}./g}
CLEANFILES+=	${DOCS}

.MAIN:	all
all:	${DOCS}

# If FORMATS is empty, do nothing
.if empty(FORMATS)
${DOC}. install- print- clean-:
.endif

install:	beforeinstall realinstall afterinstall

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
install-${_FORMAT}:
	${INSTALL} ${COPY} -o ${DOCOWN} -g ${DOCGRP} -m ${DOCMODE} \
		${DOC}*.html ${DESTDIR}${DOCDIR}/${VOLUME}
	if [ -f ${.OBJDIR}/${DOC}.ln ]; then \
		(cd ${DESTDIR}${DOCDIR}/${VOLUME}; \
		sh ${.OBJDIR}/${DOC}.ln); \
	fi

.else
install-${_FORMAT}:
	${INSTALL} ${COPY} -o ${DOCOWN} -g ${DOCGRP} -m ${DOCMODE} \
		${DOC}.${.TARGET:S/install-//} ${DESTDIR}${DOCDIR}/${VOLUME}

.endif
.endif

.if !target(${DOC}.${_FORMAT})
${DOC}.${_FORMAT}: ${SRCS}
	${SGMLFMT} -f ${.TARGET:S/${DOC}.//} ${SGMLFLAGS} ${.CURDIR}/${DOC}.sgml

.endif

.if ${_FORMAT} == "html"
CLEANFILES+= ${DOC}*.html ${DOC}.ln
.endif

.endfor


.for __target in beforeinstall afterinstall maninstall depend _SUBDIR
.if !target(__target)
${__target}:
.endif
.endfor

.include <bsd.dep.mk>
.include <bsd.obj.mk>
