# $FreeBSD: src/share/mk/bsd.port.post.mk,v 1.4 2002/04/19 07:42:41 ru Exp $

AFTERPORTMK=	yes

.include <bsd.port.mk>

.undef AFTERPORTMK
