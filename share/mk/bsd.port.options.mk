# $FreeBSD: src/share/mk/bsd.port.options.mk,v 1.1.2.1 2007/06/08 16:03:38 pav Exp $

USEOPTIONSMK=	yes
INOPTIONSMK=	yes

.include <bsd.port.mk>

.undef INOPTIONSMK
