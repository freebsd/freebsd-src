# $FreeBSD: src/share/mk/bsd.port.options.mk,v 1.1.14.1.6.1 2010/12/21 17:09:25 kensmith Exp $

USEOPTIONSMK=	yes
INOPTIONSMK=	yes

.include <bsd.port.mk>

.undef INOPTIONSMK
