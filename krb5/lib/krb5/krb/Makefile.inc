#
# SPDX-License-Identifier: BSD-2-Clause
#
# Copyright (c) 2025 FreeBSD Foundation
#
# This sofware was developed by Cy Schubert <cy@FreeBSD.org>
# under sponsorship from the FreeBSD Foundation.
#

.PATH:	${KRB5_DIR}/lib/krb5/krb

SRCS+=	addr_comp.c \
	addr_order.c \
	addr_srch.c \
	ai_authdata.c \
	allow_weak.c \
	appdefault.c \
	auth_con.c \
	authdata.c \
	authdata_dec.c \
	authdata_enc.c \
	authdata_exp.c \
	bld_pr_ext.c \
	bld_princ.c \
	cammac_util.c \
	chk_trans.c \
	chpw.c \
	conv_creds.c \
	conv_princ.c \
	copy_addrs.c \
	copy_athctr.c \
	copy_auth.c \
	copy_cksum.c \
	copy_creds.c \
	copy_ctx.c \
	copy_data.c \
	copy_key.c \
	copy_princ.c \
	copy_tick.c \
	cp_key_cnt.c \
	decode_kdc.c \
	decrypt_tk.c \
	deltat.c \
	enc_helper.c \
	enc_keyhelper.c \
	encode_kdc.c \
	encrypt_tk.c \
	etype_list.c \
	fast.c \
	fwd_tgt.c \
	gc_via_tkt.c \
	gen_save_subkey.c \
	gen_seqnum.c \
	gen_subkey.c \
	get_creds.c \
	get_etype_info.c \
	get_in_tkt.c \
	gic_keytab.c \
	gic_opt.c \
	gic_pwd.c \
	in_tkt_sky.c \
	init_ctx.c \
	init_keyblock.c \
	kdc_rep_dc.c \
	kerrs.c \
	kfree.c \
	libdef_parse.c \
	mk_cred.c \
	mk_error.c \
	mk_priv.c \
	mk_rep.c \
	mk_req.c \
	mk_req_ext.c \
	mk_safe.c \
	pac.c \
	pac_sign.c \
	padata.c \
	parse.c \
	parse_host_string.c \
	plugin.c \
	pr_to_salt.c \
	preauth2.c \
	preauth_ec.c \
	preauth_encts.c \
	preauth_otp.c \
	preauth_pkinit.c \
	preauth_sam2.c \
	princ_comp.c \
	privsafe.c \
	random_str.c \
	rd_cred.c \
	rd_error.c \
	rd_priv.c \
	rd_rep.c \
	rd_req.c \
	rd_req_dec.c \
	rd_safe.c \
	recvauth.c \
	response_items.c \
	s4u_creds.c \
	send_tgs.c \
	sendauth.c \
	ser_actx.c \
	ser_adata.c \
	ser_addr.c \
	ser_auth.c \
	ser_cksum.c \
	ser_ctx.c \
	ser_key.c \
	ser_princ.c \
	serialize.c \
	set_realm.c \
	sname_match.c \
	srv_dec_tkt.c \
	srv_rcache.c \
	str_conv.c \
	tgtname.c \
	unparse.c \
	val_renew.c \
	valid_times.c \
	vfy_increds.c \
	vic_opt.c \
	walk_rtree.c
