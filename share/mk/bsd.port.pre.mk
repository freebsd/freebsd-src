# $FreeBSD: src/share/mk/bsd.port.pre.mk,v 1.3 1999/08/28 00:21:49 peter Exp $

BEFOREPORTMK=	yes

.include "bsd.port.mk"

.undef BEFOREPORTMK
