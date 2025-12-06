#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 FreeBSD Foundation
#
# This sofware was developed by Cy Schubert <cy@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
#

PACKAGE?=	kerberos-kdc
MK_INSTALLLIB=	no
SHLIB_NAME?=	${LIB}.so.${SHLIB_MAJOR}
PLUGINSDIR=	${LIBDIR_BASE}/krb5/plugins
SHLIBDIR=	${LIBDIR}
SHLIB_MAJOR=	122

.include "../Makefile.inc"
