# $FreeBSD: src/share/mk/bsd.port.pre.mk,v 1.1.4.2 1999/08/29 16:47:46 peter Exp $

BEFOREPORTMK=	yes

.include "bsd.port.mk"

.undef BEFOREPORTMK
