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

LIB=		verto
# SHLIB_MAJOR=	0
VERSION_MAP=	${.CURDIR}/version.map

.PATH: ${KRB5_DIR}/util/verto

SRCS=	module.c \
	verto-k5ev.c \
	verto.c \
	ev.h \
	ev_vars.h \
	ev_wrap.h \
	module.h \
	${GEN}

INCS=	verto-module.h \
	verto.h \

# See util/verto/Makefile.in and util/verto/Makefile

DEFINES=-DDEFAULT_LIBRARY=\"k5ev\" -DBUILTIN_MODULE=k5ev

CFLAGS+=${DEFINES} \
	-I${KRB5_DIR}/util/verto \
	-I${KRB5_DIR}/include \
	-I${.CURDIR:H:H}/include \
	-I${.OBJDIR}

GEN=	rename.h
CLEANFILES=	${GEN}

rename.h: Symbols.ev
	@sed -e 's/.*/#define & k5&/' < ${.ALLSRC} > ${.TARGET}

.include <bsd.lib.mk>
