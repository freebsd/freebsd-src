# $FreeBSD: src/share/mk/bsd.port.pre.mk,v 1.4.30.1 2008/10/02 02:57:24 kensmith Exp $

BEFOREPORTMK=	yes

.include <bsd.port.mk>

.undef BEFOREPORTMK
