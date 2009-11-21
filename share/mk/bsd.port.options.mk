# $FreeBSD: src/share/mk/bsd.port.options.mk,v 1.1.14.1.2.1 2009/10/25 01:10:29 kensmith Exp $

USEOPTIONSMK=	yes
INOPTIONSMK=	yes

.include <bsd.port.mk>

.undef INOPTIONSMK
