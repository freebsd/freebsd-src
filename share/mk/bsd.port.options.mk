# $FreeBSD: src/share/mk/bsd.port.options.mk,v 1.1.2.1.4.1 2008/10/02 02:57:24 kensmith Exp $

USEOPTIONSMK=	yes
INOPTIONSMK=	yes

.include <bsd.port.mk>

.undef INOPTIONSMK
