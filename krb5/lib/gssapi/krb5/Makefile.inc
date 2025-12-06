#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 FreeBSD Foundation
#
# This sofware was developed by Cy Schubert <cy@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
#

.PATH:	${KRB5_DIR}/lib/gssapi/krb5 \
	${KRB5_SRCTOP}/lib/gssapi/krb5 \

SRCS+=	accept_sec_context.c \
	acquire_cred.c \
	canon_name.c \
	compare_name.c \
	context_time.c \
	copy_ccache.c \
	cred_store.c \
	delete_sec_context.c \
	disp_name.c \
	disp_status.c \
	duplicate_name.c \
	export_cred.c \
	export_name.c \
	export_sec_context.c \
	get_tkt_flags.c \
	gssapi_krb5.c \
	iakerb.c \
	import_cred.c \
	import_name.c \
	import_sec_context.c \
	indicate_mechs.c \
	init_sec_context.c \
	inq_context.c \
	inq_cred.c \
	inq_names.c \
	k5seal.c \
	k5sealiov.c \
	k5sealv3.c \
	k5sealv3iov.c \
	k5unsealiov.c \
	krb5_gss_glue.c \
	lucid_context.c \
	naming_exts.c \
	prf.c \
	process_context_token.c \
	rel_cred.c \
	rel_name.c \
	rel_oid.c \
	s4u_gss_glue.c \
	ser_sctx.c \
	set_allowable_enctypes.c \
	set_ccache.c \
	store_cred.c \
	unwrap.c \
	util_cksum.c \
	util_crypt.c \
	util_seed.c \
	util_seqnum.c \
	val_cred.c \
	verify_mic.c \
	wrap_size_limit.c \
	${K5GEN} \
	${K5GENI}

CFLAGS+=-I${KRB5_DIR}/lib/krb5 \
	-I${KRB5_OBJTOP}/lib/krb5 \
	-I${KRB5_DIR}/include \
	-I${KRB5_SRCTOP}/include

PFX=	krb5
.include "Makefile.et"
