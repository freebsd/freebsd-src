# dst
# $FreeBSD$

DIST_DIR= ${.CURDIR}/../../../contrib/isc-dhcp

.PATH: ${DIST_DIR}/dst

LIB=	 dst
NOPROFILE=		true
INTERNALLIB=		true
NOPIC=			true
INTERNALSTATICLIB=	true
NOMAN=			true

SRCS=	 dst_support.c dst_api.c hmac_link.c md5_dgst.c base64.c prandom.c

#CFLAGS = $(DEBUG) $(PREDEFINES) $(INCLUDES) $(COPTS) -DHMAC_MD5 -DMINIRES_LIB
CFLAGS+= -I${DIST_DIR}/includes -I${DIST_DIR} -DHMAC_MD5 -DMINIRES_LIB

.include <bsd.lib.mk>
