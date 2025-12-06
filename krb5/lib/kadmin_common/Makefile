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

LIB=	kadmin_common
INTERNALLIB=
LDFLAGS=-Wl,--no-undefined
INCS=	

SRCS=	kadmin.c \
	kadmin_ct.c \
	ktutil_ct.c \
	ss_wrapper.c \
	getdate.y

CFLAGS+=-I${KRB5_DIR}/lib/krad \
	-I${KRB5_DIR}/include \
	-I${KRB5_SRCTOP}/include \
	-I${KRB5_DIR}/util \
	-I${KRB5_OBJTOP}/util \
	-I${.OBJDIR}

CLEANFILES=	kadmin_ct.c ktutil_ct.c

kadmin_ct.c:	kadmin_ct.ct ss_err.h
	${MAKE_COMMANDS} ${KRB5_DIR}/kadmin/cli/kadmin_ct.ct

ktutil_ct.c:	ktutil_ct.ct ss_err.h
	${MAKE_COMMANDS} ${KRB5_DIR}/kadmin/ktutil/ktutil_ct.ct

.include <bsd.lib.mk>

.SUFFIXES: .h .c .ct

.PATH:	${KRB5_DIR}/kadmin/cli \
	${KRB5_DIR}/kadmin/ktutil \
	${KRB5_DIR}/util/ss \
	${KRB5_OBJTOP}/util/ss
