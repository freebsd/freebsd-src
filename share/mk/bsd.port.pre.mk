# $FreeBSD: src/share/mk/bsd.port.pre.mk,v 1.3.2.1 2002/07/17 19:08:23 ru Exp $

BEFOREPORTMK=	yes

.include <bsd.port.mk>

.undef BEFOREPORTMK
