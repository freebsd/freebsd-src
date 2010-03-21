# $FreeBSD: src/share/mk/bsd.port.options.mk,v 1.1.16.1 2010/02/10 00:26:20 kensmith Exp $

USEOPTIONSMK=	yes
INOPTIONSMK=	yes

.include <bsd.port.mk>

.undef INOPTIONSMK
