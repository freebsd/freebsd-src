# minires
# $FreeBSD$

DIST_DIR= ${.CURDIR}/../../../contrib/isc-dhcp

.PATH: ${DIST_DIR}/minires

LIB=	res
INTERNALLIB=		true
INTERNALSTATICLIB=	true

SRCS=	ns_date.c ns_name.c ns_parse%FIXED.c ns_samedomain.c ns_sign.c ns_verify.c \
	res_comp.c res_findzonecut.c res_init.c res_mkquery.c res_mkupdate.c \
	res_query%FIXED.c res_send.c res_sendsigned.c res_update.c

CFLAGS+= -DHMAC_MD5 -DMINIRES_LIB

NOMAN=	true

CLEANFILES+=	ns_parse%FIXED.c res_query%FIXED.c
ns_parse%FIXED.c: ns_parse.c
	sed 's/struct _ns_flagdata _ns_flagdata/struct _ns_flagdata _ns_flagdata_DONT/'	\
	    ${.ALLSRC} > ${.TARGET}
res_query%FIXED.c: res_query.c
	sed -e 's/RES_SET_H_ERRNO(\(.*\),\(.*\))/(h_errno = (\1)->res_h_errno = (\2))/' \
	    ${.ALLSRC} > ${.TARGET}

.include <bsd.lib.mk>
