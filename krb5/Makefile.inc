#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 FreeBSD Foundation
#
# This sofware was developed by Cy Schubert <cy@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
#

.include <src.opts.mk>

PACKAGE?=	kerberos
KRB5_VERSION=	1.22.1

# MIT KRB5 uses KRB5_DIR. Heimdal uses KRB5DIR.
KRB5_SRCTOP=	${SRCTOP}/krb5
KRB5_DIR=	${SRCTOP}/crypto/krb5/src
KRB5_ETDIR?=	/usr/share/et
KRB5_SRCLIBDIR=	${KRB5_DIR}/lib

KRB5_OBJTOP=	${OBJTOP}/krb5
KRB5_OBJHDR=	${OBJTOP}/include
COMPILE_ET=	compile_et -d ${KRB5_DIR}/util/et --textdomain mit-krb5
MAKE_COMMANDS=	${KRB5_OBJTOP}/util/ss/mk_cmds

# There are no WARNS levels for this.
CFLAGS+=	-Wno-deprecated-non-prototype \
		-Wno-unused-parameter

WARNS?=		1

KRB5RCTMPDIR=		/var/tmp
KRB5PREFIX=		/usr
KRB5_EXEC_PREFIX=	/usr
KRB5_BINDIR=		/usr/bin
KRB5_LIBDIR=		/usr/lib
KRB5_SBINDIR=		/usr/sbin
KRB5_MODULEDIR=		/usr/lib/krb5/plugins
KRB5_GSSMODULEDIR=	/usr/lib/gss
KRB5_LOCALEDIR=		/usr/share/locale
KRB5_DOCSDIR=		${SHAREDIR}/doc/krb5
KRB5_HTMLDOCSDIR=	${KRB5_DOCSDIR}/html
KRB5_LOCALSTATEDIR=	/var
KRB5_RUNSTATEDIR=	/var/run
KRB5_SYSCONFDIR=	/etc
KRB5_DYNOBJEXT=		.so
KRB5_SYSCONFCONF=	/etc/krb5.conf
