#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 FreeBSD Foundation
#
# This sofware was developed by Cy Schubert <cy@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
#

PROG=	gss-client

LIBADD=	gssapi_krb5 krb5 k5crypto com_err krb5profile krb5support sys

SRCS=	gss-client.c \
	gss-misc.c

CFLAGS+=-I${KRB5_DIR}/include \
	-I${KRB5_SRCTOP}/include \
	-I${KRB5_OBJTOP}/lib \
	-I${KRB5_DIR}/lib/gssapi/generic \
	-I${KRB5_DIR}/lib/gssapi/krb5 \
	-I${KRB5_DIR}/lib/gssapi/mechglue

MAN=

.include <bsd.prog.mk>

.PATH:	${KRB5_DIR}/appl/gss-sample
