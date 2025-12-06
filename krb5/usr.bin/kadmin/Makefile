#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 FreeBSD Foundation
#
# This sofware was developed by Cy Schubert <cy@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
#

PROG=	kadmin

LIBADD=	kadmin_common kadm5clnt_mit gssrpc gssapi_krb5 krb5 k5crypto \
	com_err krb5ss krb5profile krb5support sys

SRCS=	keytab.c

SCRIPTS=	k5srvutil.sh

MAN=	kadmin.1 \
	k5srvutil.1

MLINKS=	kadmin.1 kadmin.local.8

CLEANFILES=	kadmin.1 k5srvutil.1

CFLAGS+=-I${KRB5_DIR}/include \
	-I${KRB5_SRCTOP}/include

kadmin.1:	kadmin.man

k5srvutil.1:	k5srvutil.man

.include <bsd.prog.mk>

.SUFFIXES: .man

.man.1:
	@cp ${.ALLSRC} ${.TARGET}

.PATH:	${KRB5_DIR}/kadmin/cli \
	${KRB5_DIR}/man
