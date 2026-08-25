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
INTERNALLIB=

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
	-fPIC \
	-I${KRB5_DIR}/util/profile \
	-I${KRB5_DIR}/util/et \
	-I${KRB5_SRCTOP}/include \
	-I${KRB5_DIR}/include \
	-I${.OBJDIR}

.include "Makefile.profile"

prof_file.c:	profile.h

.include <bsd.lib.mk>

.SUFFIXES: .et

.PATH: ${KRB5_DIR}/util/profile
