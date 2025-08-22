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

LIB=		com_err
# SHLIB_MAJOR=	3
LDFLAGS=-Wl,--no-undefined
INCSDIR=${INCLUDEDIR}
LIBADD=	krb5support
VERSION_MAP=	${.CURDIR}/version.map

SRCS=	com_err.c \
	error_message.c \
	et_name.c \
	error_table.h \
	mit-sipb-copyright.h

INCS=	com_err.h

MAN=	com_err.3

# NOTES: com_err.h is copied to ../include

CFLAGS+=-I${KRB5_DIR}/util/et \
	-I${KRB5_DIR}/include \
	-I${.CURDIR:H:H}/include \
	-I${.OBJDIR}

FILESGROUPS=	ETSCRIPTS

ETSCRIPTS=	et_c.awk \
		et_h.awk

ETSCRIPTSDIR=	${KRB5_ETDIR}
ETSCRIPTSMODE=	0444
ETSCRIPTSTAGS=	dev

.include <bsd.lib.mk>

.SUFFIXES: .h .c .et .ct .man .3

.man.3:
	@cp ${.ALLSRC} ${.TARGET}

.PATH: ${KRB5_DIR}/util/et
