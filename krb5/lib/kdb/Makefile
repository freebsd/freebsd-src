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

LIB=		kdb5
LDFLAGS=	-Wl,--no-undefined
LIBADD=		krb5profile gssrpc krb5 k5crypto com_err krb5support gssapi_krb5
VERSION_MAP=	${.CURDIR}/version.map
PCFILES=	kdb.pc
CLEANFILES+=	${PCFILES}

SRCS=	decrypt_key.c \
	encrypt_key.c \
	iprop_xdr.c \
	kdb5.c \
	kdb_convert.c \
	kdb_cpw.c \
	kdb_default.c \
	kdb_log.c \
	keytab.c \
	${GEN} \
	${GENI}

DEFINES=-DKDB5_USE_LIB_KDB_DB2

CFLAGS+=${DEFINES} \
	-I${KRB5_DIR}/lib/kdb \
	-I${KRB5_OBJTOP}/lib/kdb \
	-I${KRB5_DIR}/include \
	-I${KRB5_SRCTOP}/include

ADB_ERR=	adb_err.et
ADB_ERR_C=	${ADB_ERR:S/.et$/.c/}
ADB_ERR_H=	${ADB_ERR:S/.et$/.h/}
CLEANFILES=	${ADB_ERR_C} ${ADB_ERR_H}
GEN+=		${ADB_ERR_C}
GENI+=		${ADB_ERR_H}

${ADB_ERR_H}: ${ADB_ERR}
	rm -f et-h-${.PREFIX}.et et-h-${.PREFIX}.c et-h-${.PREFIX}.h
	cp ${.ALLSRC} et-h-${.PREFIX}.et
	${COMPILE_ET} et-h-${.PREFIX}.et
	mv et-h-${.PREFIX}.h ${.PREFIX}.h
	rm -f et-h-${.PREFIX}.et et-h-${.PREFIX}.h

${ADB_ERR_C}: ${ADB_ERR}
	rm -f et-c-${.PREFIX}.et et-c-${.PREFIX}.c et-c-${.PREFIX}.h
	cp ${.ALLSRC} et-c-${.PREFIX}.et
	${COMPILE_ET} et-c-${.PREFIX}.et
	mv et-c-${.PREFIX}.c ${.PREFIX}.c
	rm -f et-c-${.PREFIX}.et et-c-${.PREFIX}.c

.include <bsd.lib.mk>

all: ${PCFILES}

.SUFFIXES: .h .c

.PATH:	${KRB5_DIR}/build-tools \
	${KRB5_DIR}/lib/kdb
