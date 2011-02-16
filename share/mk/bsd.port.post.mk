# $FreeBSD: src/share/mk/bsd.port.post.mk,v 1.4.40.1 2010/12/21 17:10:29 kensmith Exp $

AFTERPORTMK=	yes

.include <bsd.port.mk>

.undef AFTERPORTMK
