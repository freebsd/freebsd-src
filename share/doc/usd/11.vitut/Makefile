#	From: @(#)Makefile	8.1 (Berkeley) 6/8/93
# $FreeBSD: src/share/doc/usd/11.vitut/Makefile,v 1.7.22.1.4.1 2010/06/14 02:09:06 kensmith Exp $

VOLUME=		usd/11.edit
SRCS=		edittut.ms
MACROS=		-ms
USE_TBL=
SRCDIR=		${.CURDIR}/../../../../contrib/nvi/docs/USD.doc/edit

# index for versatec is different from the one in edit.tut
# because the fonts are different and entries reference page
# rather than section numbers.  if you have a typesetter
# you should just use the index in edit.tut, and ignore editvindex.

#editvindex:
#	${TROFF} ${MACROS} -n22 edit.vindex

.include <bsd.doc.mk>
