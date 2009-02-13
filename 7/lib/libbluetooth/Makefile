# $Id: Makefile,v 1.5 2003/07/22 18:38:04 max Exp $
# $FreeBSD$

LIB=		bluetooth
MAN=		bluetooth.3

WARNS?=		2
CFLAGS+=	-I${.CURDIR} -I${.CURDIR}/../../sys

SHLIB_MAJOR=	3

SRCS=		bluetooth.c
INCS=		bluetooth.h

MLINKS+=	bluetooth.3 bt_gethostbyname.3
MLINKS+=	bluetooth.3 bt_gethostbyaddr.3
MLINKS+=	bluetooth.3 bt_gethostent.3
MLINKS+=	bluetooth.3 bt_sethostent.3
MLINKS+=	bluetooth.3 bt_endhostent.3

MLINKS+=	bluetooth.3 bt_getprotobyname.3
MLINKS+=	bluetooth.3 bt_getprotobynumber.3
MLINKS+=	bluetooth.3 bt_getprotoent.3
MLINKS+=	bluetooth.3 bt_setprotoent.3
MLINKS+=	bluetooth.3 bt_endprotoent.3

MLINKS+=	bluetooth.3 bt_ntoa.3
MLINKS+=	bluetooth.3 bt_aton.3

MLINKS+=	bluetooth.3 bdaddr_same.3
MLINKS+=	bluetooth.3 bdaddr_any.3
MLINKS+=	bluetooth.3 bdaddr_copy.3

.include <bsd.lib.mk>
