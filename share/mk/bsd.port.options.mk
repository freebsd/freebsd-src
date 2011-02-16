# $FreeBSD: src/share/mk/bsd.port.options.mk,v 1.1.18.1 2010/12/21 17:10:29 kensmith Exp $

USEOPTIONSMK=	yes
INOPTIONSMK=	yes

.include <bsd.port.mk>

.undef INOPTIONSMK
