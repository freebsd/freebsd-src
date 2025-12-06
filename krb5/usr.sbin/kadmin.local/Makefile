#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 FreeBSD Foundation
#
# This sofware was developed by Cy Schubert <cy@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
#

PACKAGE=	kerberos-kdc

PROG=	kadmin.local

LIBADD=	kadmin_common kadm5srv_mit kdb5 gssrpc gssapi_krb5 krb5 \
	k5crypto com_err krb5profile krb5support krb5ss sys

SRCS=	keytab_local.c

MAN=	kadmin.local.8

CFLAGS+=-I${KRB5_DIR}/include \
	-I${KRB5_SRCTOP}/include

MAN=

.include <bsd.prog.mk>

.SUFFIXES: .h .c .man .8

.PATH:	${KRB5_DIR}/kadmin/cli \
	${KRB5_DIR}/man
