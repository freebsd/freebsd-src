# minires
# $FreeBSD$

DIST_DIR= ${.CURDIR}/../../../contrib/isc-dhcp

.PATH: ${DIST_DIR}/minires

LIB= res
NOPROFILE=		true
INTERNALLIB=		true
NOPIC=			true
INTERNALSTATICLIB=	true
NOMAN=			true

SRCS= 	res_mkupdate.c res_init.c res_update.c res_send.c res_comp.c	     \
        res_sendsigned.c res_findzonecut.c res_query.c res_mkquery.c	     \
        ns_date.c ns_parse.c ns_sign.c ns_name.c ns_samedomain.c ns_verify.c

CFLAGS+= -I${DIST_DIR}/includes -I${DIST_DIR} -DHMAC_MD5 -DMINIRES_LIB

.include <bsd.lib.mk>
