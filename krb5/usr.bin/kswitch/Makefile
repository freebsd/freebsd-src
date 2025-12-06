#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 FreeBSD Foundation
#
# This sofware was developed by Cy Schubert <cy@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
#

PROG=	kswitch

LIBADD=	krb5 k5crypto com_err krb5profile krb5support sys

SRCS=	kswitch.c

CFLAGS+=-I${KRB5_DIR}/include \
	-I${KRB5_SRCTOP}/include

MAN=	kswitch.1

kswitch.1:	kswitch.man

CLEANFILES+=	kswitch.1

.include <bsd.prog.mk>

.SUFFIXES: .h .c. .man .1

.man.1:
	@cp ${.ALLSRC} ${.TARGET}

.PATH:	${KRB5_DIR}/clients/kswitch \
	${KRB5_DIR}/man
