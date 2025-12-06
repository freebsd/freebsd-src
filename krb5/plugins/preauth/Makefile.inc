#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 FreeBSD Foundation
#
# This sofware was developed by Cy Schubert <cy@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
#

PACKAGE?=	kerberos-kdc
LIBDIR=		${PLUGINSDIR}/preauth
SHLIBDIR=	${LIBDIR}

.include "../Makefile.inc"
