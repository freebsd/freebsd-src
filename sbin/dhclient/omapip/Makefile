# omapip
# $FreeBSD: src/sbin/dhclient/omapip/Makefile,v 1.2.2.1 2002/04/11 10:29:48 murray Exp $

DIST_DIR= ${.CURDIR}/../../../contrib/isc-dhcp

.PATH: ${DIST_DIR}/omapip

LIB=	 omapi
INTERNALLIB=		true
INTERNALSTATICLIB=	true

SRCS=	alloc.c array.c auth.c buffer.c connection.c convert.c dispatch.c \
	errwarn.c generic.c handle.c hash.c inet_addr.c listener.c \
	message.c mrtrace.c protocol.c result.c support.c toisc.c trace.c

MAN=	omapi.3
NOMAN=	true

.include <bsd.lib.mk>
