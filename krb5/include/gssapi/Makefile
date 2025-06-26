#
# SPDX-License-Idendifier: BSD-2-Clause
#
# Copyright (c) 2025 FreeBSD Foundation
#
# This sofware was developed by Cy Schubert <cy@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
#

.include "../Makefile.inc"

.PATH:	${KRB5_DIR}/lib/gssapi/krb5 \
	${KRB5_DIR}/lib/gssapi/generic \
	${KRB5_DIR}/include

INCSGROUPS=	INCS GSSAPI_KRB5
INCSDIR=	${INCLUDEDIR}/gssapi
GSSAPI_KRB5DIR=	${INCLUDEDIR}/gssapi_krb5

INCS=	gssapi_alloc.h \
	gssapi_ext.h \
	gssapi_generic.h \
	gssapi_krb5.h

# This gssapi header file is only needed should an app need
# to build using the MIT KRB5 GSSAPI library.

GSSAPI_KRB5=	gssapi.h

.include <bsd.prog.mk>
