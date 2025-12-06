#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 FreeBSD Foundation
#
# This sofware was developed by Cy Schubert <cy@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
#

INCSGROUPS=	INCS
.include "../Makefile.inc"

.PATH: ${KRB5_DIR}/include/gssrpc

INCSDIR=	${INCLUDEDIR}/gssrpc
INCS=	\
	auth.h \
	auth_gss.h \
	auth_gssapi.h \
	auth_unix.h \
	clnt.h \
	netdb.h \
	pmap_clnt.h \
	pmap_prot.h \
	pmap_rmt.h \
	rename.h \
	rpc_msg.h \
	rpc.h \
	svc_auth.h \
	svc.h \
	types.h \
	xdr.h

.include <bsd.prog.mk>
