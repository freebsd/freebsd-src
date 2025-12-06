#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 FreeBSD Foundation
#
# This sofware was developed by Cy Schubert <cy@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
#

PACKAGE=	kerberos-kdc

PROG=	kdb5_util

LIBADD=	kadmin_common kadm5srv_mit gssrpc gssapi_krb5 krb5 k5crypto \
	kdb5 com_err krb5profile krb5support sys

SRCS=	dump.c \
	kadm5_create.c \
	kdb5_create.c \
	kdb5_destroy.c \
	kdb5_mkey.c \
	kdb5_stash.c \
	kdb5_util.c \
	strtok.c \
	tabdump.c \
	tdumputil.c \
	${GEN}

DPSRCS=	${GENI}

MAN=	kdb5_util.8

CFLAGS+=-I. \
	-I${KRB5_DIR}/include \
	-I${KRB5_DIR}/lib \
	-I${KRB5_SRCTOP}/include

MAN=	kdb5_util.8

kdb5_util.8:	kdb5_util.man

GEN_INPUT_ERR=           import_err.et
GEN_INPUT_ERR_C= ${GEN_INPUT_ERR:S/.et$/.c/}
GEN_INPUT_ERR_H= ${GEN_INPUT_ERR:S/.et$/.h/}
CLEANFILES+=    ${GEN_INPUT_ERR_C} ${GEN_INPUT_ERR_H}
GEN+=           ${GEN_INPUT_ERR_C}
GENI+=          ${GEN_INPUT_ERR_H}

${GEN_INPUT_ERR:[2..-1]}: .NOMETA
${GEN_INPUT_ERR_H}: ${GEN_INPUT_ERR}
	rm -f et-h-${.PREFIX}.et et-h-${.PREFIX}.c et-h-${.PREFIX}.h
	cp ${.ALLSRC} et-h-${.PREFIX}.et
	${COMPILE_ET} et-h-${.PREFIX}.et
	mv et-h-${.PREFIX}.h ${.PREFIX}.h
	rm -f et-h-${.PREFIX}.et et-h-${.PREFIX}.h

${GEN_INPUT_ERR_C}: ${GEN_INPUT_ERR}
	rm -f et-c-${.PREFIX}.et et-c-${.PREFIX}.c et-c-${.PREFIX}.h
	cp ${.ALLSRC} et-c-${.PREFIX}.et
	${COMPILE_ET} et-c-${.PREFIX}.et
	mv et-c-${.PREFIX}.c ${.PREFIX}.c
	rm -f et-c-${.PREFIX}.et et-c-${.PREFIX}.c


CLEANFILES+=	kdb5_util.8

.include <bsd.prog.mk>

.SUFFIXES: .h .c .man .8

.man.8:
	cp ${.ALLSRC} ${.TARGET}

.PATH:	${KRB5_DIR}/kadmin/cli \
	${KRB5_DIR}/kadmin/dbutil \
	${KRB5_DIR}/man
