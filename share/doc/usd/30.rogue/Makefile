#	From: @(#)Makefile	8.1 (Berkeley) 6/8/93
#	$Id: Makefile,v 1.3.6.3 1997/03/15 23:36:38 jkh Exp $

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
