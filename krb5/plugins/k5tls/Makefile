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

LIBDIR=		${PLUGINSDIR}/tls
SHLIBDIR=	${LIBDIR}
LIB=		k5tls
LDFLAGS=-Wl,--no-undefined
LIBADD=	krb5 krb5profile krb5support ssl crypto k5crypto com_err sys
VERSION_MAP=	${.CURDIR}/version.map

SRCS=	notls.c \
	openssl.c

CFLAGS+=-I${KRB5_DIR}/plugins/tls/k5tls \
	-I${KRB5_DIR}/include \
	-I${KRB5_SRCTOP}/include

.include <bsd.lib.mk>

.SUFFIXES: .h .c

.PATH:	${KRB5_DIR}/plugins/tls/k5tls
