# $FreeBSD: src/share/mk/bsd.port.options.mk,v 1.1.10.1 2008/11/25 02:59:29 kensmith Exp $

USEOPTIONSMK=	yes
INOPTIONSMK=	yes

.include <bsd.port.mk>

.undef INOPTIONSMK
