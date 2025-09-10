#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 FreeBSD Foundation
#
# This sofware was developed by Cy Schubert <cy@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
#

PROG=	ksu
.if defined(ENABLE_SUID_K5SU)
BINMODE=4555
PRECIOUSPROG=
.endif

LIBADD=	krb5 k5crypto com_err krb5profile krb5support sys

SRCS=	authorization.c \
	ccache.c \
	heuristic.c \
	krb_auth_su.c \
	main.c \
	xmalloc.c

CFLAGS+=-I${KRB5_DIR}/include \
	-I${KRB5_SRCTOP}/include \
	-DGET_TGT_VIA_PASSWD \
	-DPRINC_LOOK_AHEAD

MAN=	ksu.1

ksu.1:	ksu.man

CLEANFILES+=	ksu.1

.include <bsd.prog.mk>

.SUFFIXES: .h .c. .man .1

.man.1:
	@cp ${.ALLSRC} ${.TARGET}

.PATH:	${KRB5_DIR}/clients/ksu \
	${KRB5_DIR}/man
