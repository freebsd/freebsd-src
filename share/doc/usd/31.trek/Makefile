#	From: @(#)Makefile	8.1 (Berkeley) 6/8/93
# $FreeBSD: src/share/doc/usd/31.trek/Makefile,v 1.7 1999/08/28 00:19:00 peter Exp $

.if exists(${.CURDIR}/../../../../games)
VOLUME=	usd/31.trek
SRCS=	trek.me
MACROS=	-me
SRCDIR=	${.CURDIR}/../../../../games/trek/USD.doc
USE_TBL=	yes

.include <bsd.doc.mk>
.else
all clean cleandepend cleandir depend distribute install obj:
.endif
