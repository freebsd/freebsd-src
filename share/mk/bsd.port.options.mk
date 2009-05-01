# $FreeBSD: src/share/mk/bsd.port.options.mk,v 1.1.12.1 2009/04/15 03:14:26 kensmith Exp $

USEOPTIONSMK=	yes
INOPTIONSMK=	yes

.include <bsd.port.mk>

.undef INOPTIONSMK
