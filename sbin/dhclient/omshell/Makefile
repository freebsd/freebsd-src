# $FreeBSD$

DIST_DIR= ${.CURDIR}/../../../contrib/isc-dhcp

.PATH: ${DIST_DIR}/dhcpctl

PROG=	omshell
SRCS=	omshell.c

DPADD=	${LIBDHCP} ${LIBRES} ${LIBOMAPI} ${LIBDST} ${LIBDHCPCTL}
LDADD=	${LIBDHCP} ${LIBRES} ${LIBOMAPI} ${LIBDST} ${LIBDHCPCTL}

MAN=	omshell.1

.include <bsd.prog.mk>
