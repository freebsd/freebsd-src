#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 FreeBSD Foundation
#
# This sofware was developed by Cy Schubert <cy@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
#

.include <src.opts.mk>

.include "../Makefile.inc"
.include "${KRB5_SRCTOP}/Makefile.pc"

LIB=		krb5
LDFLAGS=	-Wl,--no-undefined
LIBADD=		krb5profile k5crypto com_err krb5support
VERSION_MAP=	${.CURDIR}/version.map
PCFILES=	krb5.pc \
		mit-krb5.pc
CLEANFILES+=	${PCFILES}

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

MANGROUPS=	KRB5
KRB5=		k5identity.5 \
		k5login.5 \
		krb5.conf.5 \
		kerberos.7
KRB5PACKAGE=	kerberos
KRB5LINKS=k5identity.5 .k5identity.5
KRB5LINKS+=k5login.5 .k5login.5

MANGROUPS+=	KDC
KDC=		kadm5.acl.5 \
		kdc.conf.5
KDCPACKAGE=	kerberos-kdc


.include <bsd.lib.mk>

all: ${PCFILES}

.SUFFIXES: .et .man

.man.5:
	@cp ${.ALLSRC} ${.TARGET}

.man.7:
	@cp ${.ALLSRC} ${.TARGET}

.PATH:	${KRB5_DIR}/build-tools \
	${KRB5_DIR}/lib/krb5 \
	${KRB5_DIR}/man
