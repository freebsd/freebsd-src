#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 FreeBSD Foundation
#
# This sofware was developed by Cy Schubert <cy@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
#

PROG=	sserver

LIBADD=	krb5 k5crypto com_err krb5profile krb5support sys

SRCS=	sserver.c

MAN=	sserver.8

CFLAGS+=-I${KRB5_DIR}/include \
	-I${KRB5_SRCTOP}/include

MAN=	sserver.8

sserver.8:	sserver.man

CLEANFILES+=	sserver.8

.include <bsd.prog.mk>

.SUFFIXES: .h .c. .man .8

.man.8:
	@cp ${.ALLSRC} ${.TARGET}

.PATH:	${KRB5_DIR}/appl/simple \
	${KRB5_DIR}/man
