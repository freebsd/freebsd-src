# $FreeBSD: src/share/mk/bsd.port.post.mk,v 1.3 1999/08/28 00:21:48 peter Exp $

AFTERPORTMK=	yes

.include "bsd.port.mk"

.undef AFTERPORTMK
