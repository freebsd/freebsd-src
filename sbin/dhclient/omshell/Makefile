# $FreeBSD$

DIST_DIR= ${.CURDIR}/../../../contrib/isc-dhcp

.PATH: ${DIST_DIR}/dhcpctl

PROG=	omshell

DPADD=	${LIBDHCP} ${LIBRES} ${LIBOMAPI} ${LIBDST} ${LIBDHCPCTL}
LDADD=	${LIBDHCP} ${LIBRES} ${LIBOMAPI} ${LIBDST} ${LIBDHCPCTL}

.include <bsd.prog.mk>
