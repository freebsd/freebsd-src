# $FreeBSD: src/share/mk/bsd.port.post.mk,v 1.4.34.1 2009/04/15 03:14:26 kensmith Exp $

AFTERPORTMK=	yes

.include <bsd.port.mk>

.undef AFTERPORTMK
