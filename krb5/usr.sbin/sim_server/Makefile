#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 FreeBSD Foundation
#
# This sofware was developed by Cy Schubert <cy@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
#

PROG=	sim_server

LIBADD= krb5 k5crypto com_err krb5profile krb5support sys

SRCS=	sim_server.c

CFLAGS+=-I${KRB5_DIR}/include \
	-I${KRB5_SRCTOP}/include \
	-I${KRB5_DIR}/appl/simple

MAN=

.include <bsd.prog.mk>

.PATH:	${KRB5_DIR}/appl/simple/server
