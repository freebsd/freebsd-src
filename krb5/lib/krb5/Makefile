#
# SPDX-License-Idendifier: BSD-2-Clause
#
# Copyright (c) 2025 FreeBSD Foundation
#
# This sofware was developed by Cy Schubert <cy@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
#

PACKAGE=	krb5

.include <src.opts.mk>

.include "../Makefile.inc"

LIB=		krb5
LDFLAGS=-Wl,--no-undefined
LIBADD=	krb5profile k5crypto com_err krb5support
# SHLIB_MAJOR=	3
VERSION_MAP=	${.CURDIR}/version.map

SRCS=	krb5_libinit.c

.include "${KRB5_SRCTOP}/lib/krb5/error_tables/Makefile.inc"
.include "${KRB5_SRCTOP}/lib/krb5/asn.1/Makefile.inc"
.include "${KRB5_SRCTOP}/lib/krb5/ccache/Makefile.inc"
.include "${KRB5_SRCTOP}/lib/krb5/keytab/Makefile.inc"
.include "${KRB5_SRCTOP}/lib/krb5/krb/Makefile.inc"
.include "${KRB5_SRCTOP}/lib/krb5/os/Makefile.inc"
.include "${KRB5_SRCTOP}/lib/krb5/rcache/Makefile.inc"
.include "${KRB5_SRCTOP}/lib/krb5/unicode/Makefile.inc"
.include "${KRB5_SRCTOP}/lib/krb5/docs/Makefile.inc"

DEFINES=-DHAS_STDARG \
	-DLIBDIR=\"${KRB5_LIBDIR}\" \
	-DBINDIR=\"${KRB5_BINDIR}\" \
	-DSBINDIR=\"${KRB5_SBINDIR}\" \
	-DLOCALEDIR=\"${KRB5_LOCALEDIR}\"

CFLAGS+=${DEFINES} \
	-I${KRB5_DIR}/lib/krb5/error_tables \
	-I${KRB5_DIR}/lib/krb5/ccache \
	-I${KRB5_DIR}/lib/krb5/keytab \
	-I${KRB5_DIR}/lib/krb5/krb \
	-I${KRB5_DIR}/lib/krb5/os \
	-I${KRB5_DIR}/lib/krb5/rcache \
	-I${KRB5_DIR}/lib/krb5/unicode \
	-I${KRB5_DIR}/lib/krb5/unicode/ucdata \
	-I${KRB5_DIR}/lib/krb5 \
	-I${KRB5_DIR}/util/profile \
	-I${KRB5_DIR}/include \
	-I${KRB5_SRCTOP}/include \
	-I${KRB5_DIR}

MAN=	k5identity.5 \
	k5login.5 \
	kadm5.acl.5 \
	kdc.conf.5 \
	krb5.conf.5 \
	kerberos.7

MLINKS= k5identity.5 .k5identity.5
MLINKS+=k5login.5 .k5login.5

.include <bsd.lib.mk>

.SUFFIXES: .h .c .et .man .5 .7

.man.5:
	@cp ${.ALLSRC} ${.TARGET}

.man.7:
	@cp ${.ALLSRC} ${.TARGET}

.PATH:	${KRB5_DIR}/lib/krb5 \
	${KRB5_DIR}/man
