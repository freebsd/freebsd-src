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

LIB=		kadm5srv_mit
LDFLAGS=	-Wl,--no-undefined
LIBADD=		krb5profile gssrpc gssapi_krb5 kdb5 krb5 k5crypto krb5support com_err
VERSION_MAP=	${.CURDIR}/version.map
PCFILES=	kadm-server.pc
CLEANFILES+=	${PCFILES}

SRCS=	alt_prof.c \
	chpass_util.c \
	logger.c \
	kadm_rpc_xdr.c \
	misc_free.c \
	str_conv.c \
	${SRCS_SRV} \
	${GEN}

.include "${KRB5_SRCTOP}/lib/kadm5srv/srv/Makefile.inc"

GEN=	kadm_err.c kadm_err.h chpass_util_strings.c chpass_util_strings.h
CLEANFILES=	${GEN} ${GENI}

CFLAGS+=-I${KRB5_DIR}/lib \
	-I${KRB5_DIR}/lib/kadm5 \
	-I${KRB5_DIR}/include \
	-I${KRB5_OBJTOP}/include/krb5_private \
	-I${KRB5_SRCTOP}/include \
	-I${KRB5_OBJTOP}/lib \
	-I${KRB5_OBJTOP}/lib/gssapi \
	-I${KRB5_DIR}/lib/gssapi/krb5 \
	-I${KRB5_DIR}/lib/gssapi/generic
CWARNFLAGS.gcc+=-Wno-alloc-size-larger-than

GEN_KADM_ERR=	kadm_err.et
GEN_KADM_ERR_C=	${GEN_KADM_ERR:S/.et$/.c/}
GEN_KADM_ERR_H=	${GEN_KADM_ERR:S/.et$/.h/}
CLEANFILES=	${GEN_KADM_ERR_C} ${GEN_KADM_ERR_H}
GEN_ET=		${GEN_KADM_ERR_C}
GENI_ET=	${GEN_KADM_ERR_H}

${GEN_KADM_ERR:[2..-1]}: .NOMETA
${GEN_KADM_ERR_H}: ${GEN_KADM_ERR}
	rm -f et-h-${.PREFIX}.et et-h-${.PREFIX}.c et-h-${.PREFIX}.h
	cp ${.ALLSRC} et-h-${.PREFIX}.et
	${COMPILE_ET} et-h-${.PREFIX}.et
	mv et-h-${.PREFIX}.h ${.PREFIX}.h
	rm -f et-h-${.PREFIX}.et et-h-${.PREFIX}.h

${GEN_KADM_ERR_C}: ${GEN_KADM_ERR}
	rm -f et-c-${.PREFIX}.et et-c-${.PREFIX}.c et-c-${.PREFIX}.h
	cp ${.ALLSRC} et-c-${.PREFIX}.et
	${COMPILE_ET} et-c-${.PREFIX}.et
	mv et-c-${.PREFIX}.c ${.PREFIX}.c
	rm -f et-c-${.PREFIX}.et et-c-${.PREFIX}.c

GEN_CHPASS_UTIL_STRINGS=	chpass_util_strings.et
GEN_CHPASS_UTIL_STRINGS_C=	${GEN_CHPASS_UTIL_STRINGS:S/.et$/.c/}
GEN_CHPASS_UTIL_STRINGS_H=	${GEN_CHPASS_UTIL_STRINGS:S/.et$/.h/}
CLEANFILES+=	${GEN_CHPASS_UTIL_STRINGS_C} ${GEN_CHPASS_UTIL_STRINGS_H}
GEN_ET+=	${GEN_CHPASS_UTIL_STRINGS_C}
GENI_ET+=	${GEN_CHPASS_UTIL_STRINGS_H}

${GEN_CHPASS_UTIL_STRINGS:[2..-1]}: .NOMETA
${GEN_CHPASS_UTIL_STRINGS_H}: ${GEN_CHPASS_UTIL_STRINGS}
	rm -f et-h-${.PREFIX}.et et-h-${.PREFIX}.c et-h-${.PREFIX}.h
	cp ${.ALLSRC} et-h-${.PREFIX}.et
	${COMPILE_ET} et-h-${.PREFIX}.et
	mv et-h-${.PREFIX}.h ${.PREFIX}.h
	rm -f et-h-${.PREFIX}.et et-h-${.PREFIX}.h

${GEN_CHPASS_UTIL_STRINGS_C}: ${GEN_CHPASS_UTIL_STRINGS}
	rm -f et-c-${.PREFIX}.et et-c-${.PREFIX}.c et-c-${.PREFIX}.h
	cp ${.ALLSRC} et-c-${.PREFIX}.et
	${COMPILE_ET} et-c-${.PREFIX}.et
	mv et-c-${.PREFIX}.c ${.PREFIX}.c
	rm -f et-c-${.PREFIX}.et et-c-${.PREFIX}.c

.include <bsd.lib.mk>

all: ${PCFILES}

.SUFFIXES: .h .c .et

.PATH:	${KRB5_DIR}/build-tools \
	${KRB5_DIR}/lib/kadm5
