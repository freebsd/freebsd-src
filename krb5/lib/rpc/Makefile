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
.include "${KRB5_SRCTOP}/Makefile.pc"

LIB=		gssrpc
LDFLAGS=	-Wl,--no-undefined
LIBADD=		gssapi_krb5 krb5 k5crypto com_err krb5support
VERSION_MAP=	${.CURDIR}/version.map
PCFILES=	gssrpc.pc
CLEANFILES+=	${PCFILES}

SRCS=	auth_gss.c \
	auth_gssapi.c \
	auth_gssapi_misc.c \
	auth_none.c \
	auth_unix.c \
	authgss_prot.c \
	authunix_prot.c \
	bindresvport.c \
	clnt_generic.c \
	clnt_perror.c \
	clnt_raw.c \
	clnt_simple.c \
	clnt_tcp.c \
	clnt_udp.c \
	dyn.c \
	get_myaddress.c \
	getrpcport.c \
	pmap_clnt.c \
	pmap_getmaps.c \
	pmap_getport.c \
	pmap_prot.c \
	pmap_prot2.c \
	pmap_rmt.c \
	rpc_callmsg.c \
	rpc_commondata.c \
	rpc_dtablesize.c \
	rpc_prot.c \
	svc.c \
	svc_auth.c \
	svc_auth_gss.c \
	svc_auth_gssapi.c \
	svc_auth_none.c \
	svc_auth_unix.c \
	svc_raw.c \
	svc_run.c \
	svc_simple.c \
	svc_tcp.c \
	svc_udp.c \
	xdr.c \
	xdr_alloc.c \
	xdr_array.c \
	xdr_float.c \
	xdr_mem.c \
	xdr_rec.c \
	xdr_reference.c \
	xdr_sizeof.c \
	xdr_stdio.c

CFLAGS+=-I${KRB5_DIR}/lib/rpc \
	-I${KRB5_DIR}/include \
	-I${KRB5_SRCTOP}/include \
	-I${KRB5_OBJTOP}/lib \
	-DGSSAPI_KRB5 \
	-DDEBUG_GSSAPI=0 \
	-DGSSRPC__IMPL

.include <bsd.lib.mk>

all: ${PCFILES}

.PATH:	${KRB5_DIR}/build-tools \
	${KRB5_DIR}/lib/rpc
