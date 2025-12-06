#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 FreeBSD Foundation
#
# This sofware was developed by Cy Schubert <cy@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
#

PROG=	ktutil

LIBADD=	krb5 k5crypto com_err krb5profile krb5support krb5ss sys

SRCS=	ktutil.c \
	ktutil_ct.c \
	ktutil_funcs.c

CFLAGS+=-I${KRB5_DIR}/include \
	-I${KRB5_SRCTOP}/include \
	-I${KRB5_DIR}/util \
	-I${KRB5_OBJTOP}/util

MAN=	ktutil.1

ktutil.1:	ktutil.man

CLEANFILES+=	ktutil.1

ktutil_ct.c:	ktutil_ct.ct ss_err.h
	${MAKE_COMMANDS} ${KRB5_DIR}/kadmin/ktutil/ktutil_ct.ct

.include <bsd.prog.mk>

.SUFFIXES: .h .c. .man .1 .ct

.man.1:
	@cp ${.ALLSRC} ${.TARGET}

.PATH:	${KRB5_DIR}/kadmin/ktutil \
	${KRB5_DIR}/man \
	${KRB5_OBJTOP}/util/ss
