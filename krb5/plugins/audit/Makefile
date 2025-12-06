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

LIB=		kdc_j_encode
LIBDIR=		${PLUGINSDIR}/audit
LDFLAGS=-Wl,--no-undefined
VERSION_MAP=	${.CURDIR}/version.map

.PATH:	${KRB5_DIR}/plugins/audit

SRCS=	kdc_j_encode.c

CFLAGS+=-I${KRB5_DIR}/plugins/audit

.include <bsd.lib.mk>

.SUFFIXES: .h .c
