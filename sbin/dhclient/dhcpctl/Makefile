# $FreeBSD: src/sbin/dhclient/dhcpctl/Makefile,v 1.1 2003/03/25 03:16:52 mdodd Exp $

DIST_DIR=	${.CURDIR}/../../../contrib/isc-dhcp

.PATH: ${DIST_DIR}/dhcpctl

LIB=	dhcpctl
INTERNALLIB=	true

SRCS=	dhcpctl.c callback.c remote.c

MAN=	dhcpctl.3
NOMAN=	true

.include <bsd.lib.mk>
