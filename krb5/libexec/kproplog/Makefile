#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 FreeBSD Foundation
#
# This sofware was developed by Cy Schubert <cy@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
#

PROG=	kproplog
LIBADD=	kadm5srv_mit kdb5 gssrpc gssapi_krb5 krb5 k5crypto com_err \
	krb5support sys

SRCS=	kproplog.c

MAN=	kproplog.8

CFLAGS+=-I${KRB5_DIR}/include \
	-I${KRB5_SRCTOP}/include

.include <bsd.prog.mk>

.SUFFIXES: .h .c .man .8

.man.8:
	@cp ${.ALLSRC} ${.TARGET}

.PATH:	${KRB5_DIR}/kprop \
	${KRB5_DIR}/man
