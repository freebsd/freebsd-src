#	From: @(#)Makefile	8.1 (Berkeley) 6/8/93
# $FreeBSD: src/share/doc/usd/30.rogue/Makefile,v 1.8 1999/08/28 00:18:59 peter Exp $

.if exists(${.CURDIR}/../../../../games)
VOLUME=	usd/30.rogue
SRCS=	rogue.me
MACROS=	-me
SRCDIR=	${.CURDIR}/../../../../games/rogue/USD.doc
USE_TBL=	yes

.include <bsd.doc.mk>
.else
all clean cleandepend cleandir depend distribute install obj:
.endif
