# $FreeBSD: src/share/mk/bsd.port.post.mk,v 1.4.36.1.8.1 2012/03/03 06:15:13 kensmith Exp $

AFTERPORTMK=	yes

.include <bsd.port.mk>

.undef AFTERPORTMK
