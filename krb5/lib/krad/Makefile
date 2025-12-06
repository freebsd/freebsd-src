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

LIB=	krad
# SHLIB_MAJOR=	0
LDFLAGS=-Wl,--no-undefined
LIBADD=	krb5 k5crypto com_err krb5profile krb5support verto
VERSION_MAP=	${.CURDIR}/version.map

SRCS=	attr.c \
	attrset.c \
	client.c \
	code.c \
	packet.c \
	remote.c

CFLAGS+=-I${KRB5_DIR}/lib/krad \
	-I${KRB5_DIR}/include \
	-I${KRB5_SRCTOP}/include \
	-I${.OBJDIR}

.include <bsd.lib.mk>

.PATH: ${KRB5_DIR}/lib/krad
