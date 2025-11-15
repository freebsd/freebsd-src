#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 FreeBSD Foundation
#
# This sofware was developed by Cy Schubert <cy@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
#

.PATH:	${KRB5_DIR}/lib/gssapi/generic \
	${KRB5_SRCTOP}/lib/gssapi/generic \
	${KRB5_SRCTOP}/include

SRCS+=	disp_com_err_status.c \
	disp_major_status.c \
	errmap.h \
	gssapi.h \
	gssapi_generic.c \
	oid_ops.c \
	rel_buffer.c \
	rel_oid_set.c \
	util_buffer.c \
	util_buffer_set.c \
	util_errmap.c \
	util_seqstate.c \
	util_token.c \
	${GGEN} \
	${GGENI}

# The gssapi subdir is only needed should we want an app to be able to
# build using the MIT KRB5 GSSAPI library.
# XXX We may wish to either comment this out or use a knob to enable/disable
# XXX it.

INCSGROUPS=	GSSAPI_INCS
GSSAPI_INCS=	gssapi.h
INCS+=		${GENI}
GSSAPI_INCSDIR=	${INCLUDEDIR}/gssapi

CLEANFILES+=	gssapi.h ${GGEN} ${GGENI}

INCLUDE_XOM=	echo "/* no xom.h */"

gssapi.h:	gssapi.hin
	echo "Creating gssapi.h" ; \
	h=gss$$$$; rm -f $$h; \
	(echo "/* This is the gssapi.h prologue. */"; \
	${INCLUDE_XOM} && \
	echo "/* End of gssapi.h prologue. */"&& \
	cat ${KRB5_DIR}/lib/gssapi/generic/gssapi.hin )> $$h && \
	(set -x; mv $$h ${.TARGET}) ; e=$$?; rm -f $$h; exit $$e

PFX=	generic
.include "Makefile.et"
