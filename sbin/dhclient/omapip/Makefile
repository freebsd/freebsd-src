# omapip
# $FreeBSD$

DIST_DIR= ${.CURDIR}/../../../contrib/isc-dhcp

.PATH: ${DIST_DIR}/omapip

LIB=	 omapi
NOPROFILE=		true
INTERNALLIB=		true
NOPIC=			true
INTERNALSTATICLIB=	true
NOMAN=			true

SRCS=    protocol.c buffer.c alloc.c result.c connection.c errwarn.c \
         listener.c dispatch.c generic.c support.c handle.c message.c \
         convert.c hash.c auth.c inet_addr.c array.c trace.c mrtrace.c \
         toisc.c

CFLAGS+= -I${DIST_DIR}/includes -I${DIST_DIR}

MAN=	omapi.3

.include <bsd.lib.mk>
