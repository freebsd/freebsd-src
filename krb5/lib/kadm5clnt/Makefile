#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 FreeBSD Foundation
#
# This sofware was developed by Cy Schubert <cy@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
#

.include <src.opts.mk>

.include "../Makefile.inc"
.include "${KRB5_SRCTOP}/Makefile.pc"

LIB=		kadm5clnt_mit
LDFLAGS=	-Wl,--no-undefined
LIBADD=		krb5profile gssrpc gssapi_krb5 krb5 k5crypto krb5support com_err
VERSION_MAP=	${.CURDIR}/version.map
PCFILES=	kadm-client.pc
CLEANFILES+=	${PCFILES}

SRCS=	alt_prof.c \
	chpass_util.c \
	logger.c \
	kadm_rpc_xdr.c \
	misc_free.c \
	str_conv.c \
	${GEN} \
	${GENI}

.include "${KRB5_SRCTOP}/lib/kadm5clnt/clnt/Makefile.inc"

CFLAGS+=-I${KRB5_DIR}/lib \
	-I${KRB5_DIR}/lib/kadm5 \
	-I${KRB5_DIR}/include \
	-I${KRB5_OBJTOP}/include/krb5_private \
	-I${KRB5_SRCTOP}/include \
	-I${KRB5_OBJTOP}/lib

INCSDIR=${INCLUDEDIR}/kadm5
INCS=	admin.h \
	${GENI}

KADM_ERR=	kadm_err.et
KADM_ERR_C=	${KADM_ERR:S/.et$/.c/}
KADM_ERR_H=	${KADM_ERR:S/.et$/.h/}
CLEANFILES+=	${KADM_ERR_C} ${KADM_ERR_H}
GEN+=		${KADM_ERR_C}
GENI+=		${KADM_ERR_H}

${KADM_ERR:[2..-1]}: .NOMETA
${KADM_ERR_H}: ${KADM_ERR}
	rm -f et-h-${.PREFIX}.et et-h-${.PREFIX}.c et-h-${.PREFIX}.h
	cp ${.ALLSRC} et-h-${.PREFIX}.et
	${COMPILE_ET} et-h-${.PREFIX}.et
	mv et-h-${.PREFIX}.h ${.PREFIX}.h
	rm -f et-h-${.PREFIX}.et et-h-${.PREFIX}.h

${KADM_ERR_C}: ${KADM_ERR}
	rm -f et-c-${.PREFIX}.et et-c-${.PREFIX}.c et-c-${.PREFIX}.h
	cp ${.ALLSRC} et-c-${.PREFIX}.et
	${COMPILE_ET} et-c-${.PREFIX}.et
	mv et-c-${.PREFIX}.c ${.PREFIX}.c
	rm -f et-c-${.PREFIX}.et et-c-${.PREFIX}.c

CHPASS_UTIL_STRINGS_ERR=	chpass_util_strings.et
CHPASS_UTIL_STRINGS_ERR_C=	${CHPASS_UTIL_STRINGS_ERR:S/.et$/.c/}
CHPASS_UTIL_STRINGS_ERR_H=	${CHPASS_UTIL_STRINGS_ERR:S/.et$/.h/}
CLEANFILES+=	${CHPASS_UTIL_STRINGS_ERR_C} ${CHPASS_UTIL_STRINGS_ERR_H}
GEN+=		${CHPASS_UTIL_STRINGS_ERR_C}
GENI+=		${CHPASS_UTIL_STRINGS_ERR_H}

${CHPASS_UTIL_STRINGS_ERR:[2..-1]}: .NOMETA
${CHPASS_UTIL_STRINGS_ERR_H}: ${CHPASS_UTIL_STRINGS_ERR}
	rm -f et-h-${.PREFIX}.et et-h-${.PREFIX}.c et-h-${.PREFIX}.h
	cp ${.ALLSRC} et-h-${.PREFIX}.et
	${COMPILE_ET} et-h-${.PREFIX}.et
	mv et-h-${.PREFIX}.h ${.PREFIX}.h
	rm -f et-h-${.PREFIX}.et et-h-${.PREFIX}.h

${CHPASS_UTIL_STRINGS_ERR_C}: ${CHPASS_UTIL_STRINGS_ERR}
	rm -f et-c-${.PREFIX}.et et-c-${.PREFIX}.c et-c-${.PREFIX}.h
	cp ${.ALLSRC} et-c-${.PREFIX}.et
	${COMPILE_ET} et-c-${.PREFIX}.et
	mv et-c-${.PREFIX}.c ${.PREFIX}.c
	rm -f et-c-${.PREFIX}.et et-c-${.PREFIX}.c

afterinstall:
	${INSTALL_LIBSYMLINK} ${DEV_TAG_ARGS} ${SHLIB_LINK} \
	    ${DESTDIR}${LIBDIR}/libkadm5clnt.so

.include <bsd.lib.mk>

all: ${PCFILES}

.SUFFIXES: .h .c

.PATH:	${KRB5_DIR}/build-tools \
	${KRB5_DIR}/lib/kadm5
