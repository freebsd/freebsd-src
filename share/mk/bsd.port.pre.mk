# $FreeBSD: src/share/mk/bsd.port.pre.mk,v 1.4.34.1 2009/04/15 03:14:26 kensmith Exp $

BEFOREPORTMK=	yes

.include <bsd.port.mk>

.undef BEFOREPORTMK
