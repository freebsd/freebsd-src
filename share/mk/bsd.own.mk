#	$Id: bsd.own.mk,v 1.1 1994/08/04 21:10:08 wollman Exp $

BINGRP?=	bin
BINOWN?=	bin
BINMODE?=	555

.if !defined(DEBUG_FLAGS)
STRIP?=		-s
.endif

COPY?=		-c

MANDIR?=	/usr/share/man/man
MANGRP?=	bin
MANOWN?=	bin
MANMODE?=	444
