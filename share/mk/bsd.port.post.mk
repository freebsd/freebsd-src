# $FreeBSD: src/share/mk/bsd.port.post.mk,v 1.1.4.2 1999/08/29 16:47:46 peter Exp $

AFTERPORTMK=	yes

.include "bsd.port.mk"

.undef AFTERPORTMK
