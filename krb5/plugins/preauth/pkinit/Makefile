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

LIB=		pkinit
LIBDIR=		${PLUGINSDIR}/preauth
LDFLAGS=-Wl,--no-undefined
LIBADD=	krb5profile krb5 com_err k5crypto crypto krb5support
VERSION_MAP=	${.CURDIR}/version.map

.PATH:	${KRB5_DIR}/plugins/preauth/pkinit

SRCS=	pkinit_accessor.c \
	pkinit_clnt.c \
	pkinit_constants.c \
	pkinit_crypto_openssl.c  \
	pkinit_identity.c \
	pkinit_kdf_test.c \
	pkinit_lib.c \
	pkinit_matching.c \
	pkinit_profile.c \
	pkinit_srv.c

CFLAGS+=-I${KRB5_DIR}/plugins/preauth/pkinit \
	-I${KRB5_DIR}/include \
	-I${KRB5_SRCTOP}/include

.include <bsd.lib.mk>
