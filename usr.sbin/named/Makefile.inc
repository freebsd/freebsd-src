# From: Id: Makefile.inc,v 8.4 1996/03/03 17:42:43 vixie Exp
# $Id: Makefile.inc,v 1.2 1996/09/04 22:25:21 bde Exp $

.ifndef (Mk.Inc)
Mk.Inc?=defined

BIND_DIR=	${.CURDIR}/../../contrib/bind

VER!=		awk -F' *= *' '$$1 == "VER" { print $$2 ; exit }' \
		${BIND_DIR}/Makefile

PIDDIR=		/var/run
INDOT=
XFER_INDOT=	${INDOT}
PS=		ps
DESTSBIN=	/usr/sbin
IOT=		ABRT

CONFIG?=	-DUSE_OPTIONS_H
INCLUDE?=	-I${BIND_DIR} -I${BIND_DIR}/include
CFLAGS+=	${INCLUDE} ${CONFIG}

.include	"Makefile.maninc"
.endif
