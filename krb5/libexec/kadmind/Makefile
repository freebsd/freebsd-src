#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 FreeBSD Foundation
#
# This sofware was developed by Cy Schubert <cy@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
#

.include "../Makefile.inc"

PROG=	kadmind
LIBADD=	kadm5srv_mit kdb5 gssrpc gssapi_krb5 krb5 k5crypto com_err \
	krb5support apputils verto sys m

SRCS=	auth.c \
	auth_acl.c \
	auth_self.c \
	ipropd_svc.c \
	kadm_rpc_svc.c \
	misc.c \
	ovsec_kadmd.c \
	schpw.c \
	server_stubs.c

MAN=	kadmind.8

CFLAGS+=-I${KRB5_DIR}/include \
	-I${KRB5_SRCTOP}/include \
	-I${KRB5_OBJTOP}/lib/gssapi \
	-I${KRB5_OBJTOP}/lib \
	-I${KRB5_DIR}/lib \
	-I${KRB5_DIR}/lib/gssapi/krb5 \
	-I${KRB5_DIR}/lib/gssapi/generic

kadmind.8:	kadmind.man

CLEANFILES+=	kadmind.8

.include <bsd.prog.mk>

.SUFFIXES: .h .c .man .8

.man.8:
	@cp ${.ALLSRC} ${.TARGET}

.PATH:	${KRB5_DIR}/kadmin/server \
	${KRB5_DIR}/man
