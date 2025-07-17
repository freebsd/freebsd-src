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

LIB=		gssapi_krb5
# SHLIB_MAJOR=	2
LDFLAGS=-Wl,--no-undefined
LIBADD=	krb5 k5crypto com_err krb5profile krb5support

# This is a contcatonation of:
#	crypto/krb5/src/lib/gssapi/libgssapi_krb5.exports
#	crypto/krb5/src/lib/gssapi/spnego/mech_spnego.exports

.include "${KRB5_SRCTOP}/lib/gssapi/generic/Makefile.inc"
.include "${KRB5_SRCTOP}/lib/gssapi/krb5/Makefile.inc"
.include "${KRB5_SRCTOP}/lib/gssapi/spnego/Makefile.inc"
.include "${KRB5_SRCTOP}/lib/gssapi/mechglue/Makefile.inc"

DEFINES=-D_GSS_STATIC_LINK=1

CFLAGS+=${DEFINES} \
	-I${KRB5_DIR}/lib/gssapi/generic \
	-I${KRB5_DIR}/lib/gssapi/krb5 \
	-I${KRB5_DIR}/lib/gssapi/spnego \
	-I${KRB5_DIR}/lib/gssapi/mechglue \
	-I${KRB5_DIR}/lib/gssapi \
	-I${KRB5_DIR}/lib \
	-I${KRB5_SRCTOP}/lib \
	-I${KRB5_SRCTOP}/lib/gssapi \
	-I${KRB5_OBJTOP}/lib/gssapi \
	-I${KRB5_OBJTOP}/lib \
	-I${KRB5_DIR}/include \
	-I${KRB5_SRCTOP}/include

.include <bsd.lib.mk>

.SUFFIXES: .h .c .et
