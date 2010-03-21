# $FreeBSD: src/share/mk/bsd.port.pre.mk,v 1.4.38.1 2010/02/10 00:26:20 kensmith Exp $

BEFOREPORTMK=	yes

.include <bsd.port.mk>

.undef BEFOREPORTMK
