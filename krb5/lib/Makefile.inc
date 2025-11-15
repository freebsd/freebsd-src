#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 FreeBSD Foundation
#
# This sofware was developed by Cy Schubert <cy@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
#

.include "../Makefile.inc"

LIB_PACKAGE=

KRB5_KRB5LIBDIR=	${KRB5_SRCLIBDIR}/krb5
KRB5_K5CRYPTODIR=	${KRB5_SRCLIBDIR}/crypto

SHLIBDIR?=		/usr/lib
SHLIB_MAJOR?=		122
