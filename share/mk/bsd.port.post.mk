# $FreeBSD: src/share/mk/bsd.port.post.mk,v 1.3.2.1 2002/07/17 19:08:23 ru Exp $

AFTERPORTMK=	yes

.include <bsd.port.mk>

.undef AFTERPORTMK
