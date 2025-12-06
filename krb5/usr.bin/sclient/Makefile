#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 FreeBSD Foundation
#
# This sofware was developed by Cy Schubert <cy@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
#

PROG=	sclient

LIBADD=	krb5 k5crypto com_err krb5profile krb5support sys

SRCS=	sclient.c

CFLAGS+=-I${KRB5_DIR}/include \
	-I${KRB5_SRCTOP}/include

MAN=	sclient.1

sclient.1:	sclient.man

CLEANFILES+=	sclient.1

.include <bsd.prog.mk>

.SUFFIXES: .h .c. .man .1

.man.1:
	@cp ${.ALLSRC} ${.TARGET}

.PATH:	${KRB5_DIR}/appl/sample/sclient \
	${KRB5_DIR}/man
