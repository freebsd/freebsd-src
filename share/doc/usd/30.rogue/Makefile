#	From: @(#)Makefile	8.1 (Berkeley) 6/8/93
#	$Id: Makefile,v 1.6 1997/02/22 13:06:45 peter Exp $

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
