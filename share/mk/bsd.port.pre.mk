# $FreeBSD: src/share/mk/bsd.port.pre.mk,v 1.4 2002/04/19 07:42:41 ru Exp $

BEFOREPORTMK=	yes

.include <bsd.port.mk>

.undef BEFOREPORTMK
