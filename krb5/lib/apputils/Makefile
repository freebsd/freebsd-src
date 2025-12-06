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

LIB=	apputils
INTERNALLIB=
LDFLAGS=-Wl,--no-undefined

SRCS=	net-server.c \
	udppktinfo.c \
	udppktinfo.h

CFLAGS+=-I${KRB5_DIR}/lib/apputils \
	-I${KRB5_DIR}/include \
	-I${KRB5_SRCTOP}/include \
	-I${.OBJDIR}

.include <bsd.lib.mk>

.PATH: ${KRB5_DIR}/lib/apputils
