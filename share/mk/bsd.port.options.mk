# $FreeBSD: src/share/mk/bsd.port.options.mk,v 1.1.14.1.8.1 2012/03/03 06:15:13 kensmith Exp $

USEOPTIONSMK=	yes
INOPTIONSMK=	yes

.include <bsd.port.mk>

.undef INOPTIONSMK
