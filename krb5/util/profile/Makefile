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

LIB=		krb5profile
LIBADD=	com_err krb5support
VERSION_MAP=	${.CURDIR}/version.map

SRCS=	prof_file.c \
	prof_get.c \
	prof_init.c \
	prof_parse.c \
	prof_set.c \
	prof_tree.c \
	argv_parse.h \
	prof_int.h \
	${GEN}

INCS=	profile.h

DEFINES=-DHAS_STDARG -DLIBDIR=\"${KRB5_LIBDIR}\"

CFLAGS+=${DEFINES} \
	-I${KRB5_DIR}/util/profile \
	-I${KRB5_DIR}/util/et \
	-I${KRB5_SRCTOP}/include \
	-I${KRB5_DIR}/include \
	-I${.OBJDIR}

GEN=	profile.h ${GEN_PROF_ERR_C} ${GEN_PROF_ERR_H}
GEN_PROF_ERR=	prof_err.et
GEN_PROF_ERR_C=	${GEN_PROF_ERR:S/.et$/.c/}
GEN_PROF_ERR_H=	${GEN_PROF_ERR:S/.et$/.h/}
${GEN_PROF_ERR:[2..-1]}: .NOMETA
CLEANFILES=	et-h-prof_err.et et-h-prof_err.c et-h-prof_err.h \
	${GEN}

.include "${KRB5_SRCTOP}/Makefile.et"

${GEN_PROF_ERR_H}: ${GEN_PROF_ERR}
	rm -f et-h-${.PREFIX}.et et-h-${.PREFIX}.c et-h-${.PREFIX}.h
	cp ${.ALLSRC} et-h-${.PREFIX}.et
	${COMPILE_ET} et-h-${.PREFIX}.et
	mv et-h-${.PREFIX}.h ${.PREFIX}.h
	rm -f et-h-${.PREFIX}.et et-h-${.PREFIX}.h

${GEN_PROF_ERR_C}: ${GEN_PROF_ERR}
	rm -f et-c-${.PREFIX}.et et-c-${.PREFIX}.c et-c-${.PREFIX}.h
	cp ${.ALLSRC} et-c-${.PREFIX}.et
	${COMPILE_ET} et-c-${.PREFIX}.et
	mv et-c-${.PREFIX}.c ${.PREFIX}.c
	rm -f et-c-${.PREFIX}.et et-c-${.PREFIX}.c

profile.h:	profile.hin prof_err.h
	cat ${.ALLSRC} > ${.TARGET}

prof_file.c:	profile.h

.include <bsd.lib.mk>

.SUFFIXES: .et

.PATH: ${KRB5_DIR}/util/profile
