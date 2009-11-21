# $FreeBSD: src/share/mk/bsd.port.pre.mk,v 1.4.36.1.2.1 2009/10/25 01:10:29 kensmith Exp $

BEFOREPORTMK=	yes

.include <bsd.port.mk>

.undef BEFOREPORTMK
