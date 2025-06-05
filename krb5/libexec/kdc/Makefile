#
# SPDX-License-Idendifier: BSD-2-Clause
#
# Copyright (c) 2025 FreeBSD Foundation
#
# This sofware was developed by Cy Schubert <cy@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
#

PACKAGE=	krb5

.include "../Makefile.inc"

PROG=	krb5kdc
LIBADD=	kadm5srv_mit gssrpc gssapi_krb5 krb5 kdb5 k5crypto com_err krb5support \
	apputils verto krb5profile m sys

SRCS=	authind.c \
	cammac.c \
	dispatch.c \
	do_as_req.c \
	do_tgs_req.c \
	extern.c \
	fast_util.c \
	kdc_audit.c \
	kdc_authdata.c \
	kdc_log.c \
	kdc_preauth.c \
	kdc_preauth_ec.c \
	kdc_preauth_encts.c \
	kdc_transit.c \
	kdc_util.c \
	main.c \
	ndr.c \
	policy.c \
	replay.c \
	tgs_policy.c \
	${GEN} \
	${GENI}

MAN=	krb5kdc.8

DEFINES=-DLIBDIR=\"${KRB5_LIBDIR}\"

CFLAGS+=${DEFINES} \
	-I${KRB5_DIR}/include \
	-I${KRB5_SRCTOP}/include \
	-I${.OBJDIR}

GEN_KDC5_ERR=		kdc5_err.et
GEN_KDC5_ERR_C=	${GEN_KDC5_ERR:S/.et$/.c/}
GEN_KDC5_ERR_H=	${GEN_KDC5_ERR:S/.et$/.h/}
CLEANFILES+=	${GEN_KDC5_ERR_C} ${GEN_KDC5_ERR_H}
GEN+=		${GEN_KDC5_ERR_C}
GENI+=		${GEN_KDC5_ERR_H}

${GEN_KDC5_ERR:[2..-1]}: .NOMETA
${GEN_KDC5_ERR_H}: ${GEN_KDC5_ERR}
	rm -f et-h-${.PREFIX}.et et-h-${.PREFIX}.c et-h-${.PREFIX}.h
	cp ${.ALLSRC} et-h-${.PREFIX}.et
	${COMPILE_ET} et-h-${.PREFIX}.et
	mv et-h-${.PREFIX}.h ${.PREFIX}.h
	rm -f et-h-${.PREFIX}.et et-h-${.PREFIX}.h

${GEN_KDC5_ERR_C}: ${GEN_KDC5_ERR}
	rm -f et-c-${.PREFIX}.et et-c-${.PREFIX}.c et-c-${.PREFIX}.h
	cp ${.ALLSRC} et-c-${.PREFIX}.et
	${COMPILE_ET} et-c-${.PREFIX}.et
	mv et-c-${.PREFIX}.c ${.PREFIX}.c
	rm -f et-c-${.PREFIX}.et et-c-${.PREFIX}.c

CLEANFILES+=	krb5kdc.8

krb5kdc.8:	krb5kdc.man

.include <bsd.prog.mk>

.SUFFIXES: .h .c .man .1

.man.8:
	cp ${.ALLSRC} ${.TARGET}

.PATH:	${KRB5_DIR}/kdc \
	${KRB5_DIR}/man
