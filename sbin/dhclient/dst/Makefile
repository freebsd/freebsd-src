# dst
# $FreeBSD$

DIST_DIR= ${.CURDIR}/../../../contrib/isc-dhcp

.PATH: ${DIST_DIR}/dst

LIB=	 dst
INTERNALLIB=

SRCS=	base64.c dst_api.c dst_support.c hmac_link.c md5_dgst.c prandom.c

CFLAGS+= -DHMAC_MD5 -DMINIRES_LIB

.include <bsd.lib.mk>
