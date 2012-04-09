# $FreeBSD: src/share/mk/bsd.port.pre.mk,v 1.4.36.1.8.1 2012/03/03 06:15:13 kensmith Exp $

BEFOREPORTMK=	yes

.include <bsd.port.mk>

.undef BEFOREPORTMK
