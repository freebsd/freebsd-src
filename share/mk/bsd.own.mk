#	$Id: bsd.own.mk,v 1.2 1994/09/16 14:30:21 jkh Exp $

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
