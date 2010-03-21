# $FreeBSD: src/share/mk/bsd.port.post.mk,v 1.4.38.1 2010/02/10 00:26:20 kensmith Exp $

AFTERPORTMK=	yes

.include <bsd.port.mk>

.undef AFTERPORTMK
