#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 FreeBSD Foundation
#
# This sofware was developed by Cy Schubert <cy@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
#

.include "../Makefile.inc"

PROG=	krb5kdc
LIBADD=	kadm5srv_mit gssrpc gssapi_krb5 krb5 kdb5 k5crypto com_err krb5support \
	apputils verto krb5profile m sys

SRCS=	authind.c \
	cammac.c \
	dispatch.c \
	do_as_req.c \
	do_tgs_req.c \
	extern.c \
	fast_util.c \
	kdc_audit.c \
	kdc_authdata.c \
	kdc_log.c \
	kdc_preauth.c \
	kdc_preauth_ec.c \
	kdc_preauth_encts.c \
	kdc_transit.c \
	kdc_util.c \
	main.c \
	ndr.c \
	policy.c \
	replay.c \
	tgs_policy.c \
	${GEN} \
	${GENI}

MAN=	krb5kdc.8

DEFINES=-DLIBDIR=\"${KRB5_LIBDIR}\"

CFLAGS+=${DEFINES} \
	-I${KRB5_DIR}/include \
	-I${KRB5_SRCTOP}/include \
	-I${.OBJDIR}

CLEANFILES+=	krb5kdc.8

krb5kdc.8:	krb5kdc.man

.include <bsd.prog.mk>

.SUFFIXES: .h .c .man .1

.man.8:
	cp ${.ALLSRC} ${.TARGET}

.PATH:	${KRB5_DIR}/kdc \
	${KRB5_DIR}/man
