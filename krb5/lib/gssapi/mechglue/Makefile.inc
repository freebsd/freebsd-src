#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 FreeBSD Foundation
#
# This sofware was developed by Cy Schubert <cy@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
#

.PATH:	${KRB5_DIR}/lib/gssapi/mechglue

SRCS+=	g_accept_sec_context.c \
	g_acquire_cred.c \
	g_acquire_cred_imp_name.c \
	g_acquire_cred_with_pw.c \
	g_authorize_localname.c \
	g_buffer_set.c \
	g_canon_name.c \
	g_compare_name.c \
	g_complete_auth_token.c \
	g_context_time.c \
	g_decapsulate_token.c \
	g_del_name_attr.c \
	g_delete_sec_context.c \
	g_dsp_name.c \
	g_dsp_name_ext.c \
	g_dsp_status.c \
	g_dup_name.c \
	g_encapsulate_token.c \
	g_exp_sec_context.c \
	g_export_cred.c \
	g_export_name.c \
	g_export_name_comp.c \
	g_get_name_attr.c \
	g_glue.c \
	g_imp_cred.c \
	g_imp_name.c \
	g_imp_sec_context.c \
	g_init_sec_context.c \
	g_initialize.c \
	g_inq_context.c \
	g_inq_context_oid.c \
	g_inq_cred.c \
	g_inq_cred_oid.c \
	g_inq_name.c \
	g_inq_names.c \
	g_map_name_to_any.c \
	g_mech_invoke.c \
	g_mechattr.c \
	g_mechname.c \
	g_negoex.c \
	g_oid_ops.c \
	g_prf.c \
	g_process_context.c \
	g_rel_buffer.c \
	g_rel_cred.c \
	g_rel_name.c \
	g_rel_name_mapping.c \
	g_rel_oid_set.c \
	g_saslname.c \
	g_seal.c \
	g_set_context_option.c \
	g_set_cred_option.c \
	g_set_name_attr.c \
	g_set_neg_mechs.c \
	g_sign.c \
	g_store_cred.c \
	g_unseal.c \
	g_unwrap_aead.c \
	g_unwrap_iov.c \
	g_verify.c \
	g_wrap_aead.c \
	g_wrap_iov.c \
	gssd_pname_to_uid.c

GSSAPI_INCS+=	mechglue.h
