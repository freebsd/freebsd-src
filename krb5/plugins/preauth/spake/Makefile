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

LIB=		spake
LIBDIR=		${PLUGINSDIR}/preauth
LDFLAGS=-Wl,--no-undefined
LIBADD=	krb5profile krb5 k5crypto com_err krb5support crypto sys
VERSION_MAP=	${.CURDIR}/version.map

.PATH:	${KRB5_DIR}/plugins/preauth/spake

SRCS=	edwards25519.c \
	groups.c \
	iana.c \
	openssl.c \
	spake_client.c \
	spake_kdc.c \
	util.c

CFLAGS+=-I${KRB5_DIR}/plugins/preauth/spake \
	-I${KRB5_DIR}/include \
	-I${KRB5_SRCTOP}/include

.include <bsd.lib.mk>
