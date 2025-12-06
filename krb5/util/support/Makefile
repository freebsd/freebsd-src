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

LIB=	krb5support
# SHLIB_MAJOR=	0
LDFLAGS=-Wl,--no-undefined
VERSION_MAP=	${.CURDIR}/version.map

.PATH: ${KRB5_DIR}/util/support

SRCS=	base64.c \
	bcmp.c \
	dir_filenames.c \
	errors.c \
	fake-addrinfo.c \
	gmt_mktime.c \
	hashtab.c \
	hex.c \
	init-addrinfo.c \
	json.c \
	k5buf.c \
	path.c \
	plugins.c \
	strerror_r.c \
	threads.c \
	utf8.c \
	utf8_conv.c \
	zap.c \
	cache-addrinfo.h \
	supp-int.h

CFLAGS+=-I${KRB5_DIR}/util/support \
	-I${KRB5_DIR}/include \
	-I${.CURDIR:H:H}/include \
	-I${.OBJDIR}

.include <bsd.lib.mk>
