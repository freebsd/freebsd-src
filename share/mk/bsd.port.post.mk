# $FreeBSD: src/share/mk/bsd.port.post.mk,v 1.4.32.1 2008/11/25 02:59:29 kensmith Exp $

AFTERPORTMK=	yes

.include <bsd.port.mk>

.undef AFTERPORTMK
