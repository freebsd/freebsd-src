# $FreeBSD: src/share/mk/bsd.docb.mk,v 1.4 1999/08/28 00:21:46 peter Exp $
#
# The include file <bsd.docb.mk> handles installing SGML/docbook documents.
#
# +++ variables +++
#
# DOC		Name of the document. 
#
# VOLUME	Name of the installation subdirectory.
#
# SGMLOPTS	Flags to sgmlfmt. 
#
# SGMLFMT	Format sgml files command. [sgmlfmt]
#
#
# +++ targets +++
#
#	all:
#		Converts sgml files to the specified output format
#		(see ${FORMATS}).
#
#	install:
#		Install formated output.


.if !target(__initialized__)
__initialized__:
.if exists(${.CURDIR}/../Makefile.inc)
.include "${.CURDIR}/../Makefile.inc"
.endif
.endif

# Use SGMLOPTS to pass extra flags to sgmlfmt(1).
VOLUME?=	${.CURDIR:T}
DOC?=		${.CURDIR:T}
SGMLFMT?=	sgmlfmt

_docs=	${DOC:S/$/.html/g}
CLEANFILES+=${_docs}

# A DocBook document has the suffix .docb or .sgml. If a document
# with both suffixes exists, .docb wins.
.SUFFIXES:	.docb .sgml .html

.docb.html .sgml.html: ${SRCS}
	${SGMLFMT} -d docbook -f html ${SGMLOPTS} ${.IMPSRC}

.MAIN:	all
all:	${_docs}

install:
	${INSTALL} ${COPY} -o ${DOCOWN} -g ${DOCGRP} -m ${DOCMODE} \
		${_docs} ${DESTDIR}${DOCDIR}/${VOLUME}

.include <bsd.obj.mk>
