# dst
# $FreeBSD: src/sbin/dhclient/dst/Makefile,v 1.2.2.2 2002/07/19 18:46:27 ru Exp $

DIST_DIR= ${.CURDIR}/../../../contrib/isc-dhcp

.PATH: ${DIST_DIR}/dst

LIB=	 dst
INTERNALLIB=		true

SRCS=	base64.c dst_api.c dst_support.c hmac_link.c md5_dgst.c prandom.c

CFLAGS+= -DHMAC_MD5 -DMINIRES_LIB

.include <bsd.lib.mk>
