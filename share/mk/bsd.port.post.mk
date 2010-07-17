# $FreeBSD: src/share/mk/bsd.port.post.mk,v 1.4.36.1.4.1 2010/06/14 02:09:06 kensmith Exp $

AFTERPORTMK=	yes

.include <bsd.port.mk>

.undef AFTERPORTMK
