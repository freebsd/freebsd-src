#	From: @(#)Makefile	5.1 (Berkeley) 6/25/90
#	$Id$

LIB=	krb
SHLIB_MINOR=   1
CFLAGS+=-DKERBEROS -DCRYPT -DDEBUG -DBSD42
SRCS=	krb_err.c create_auth_reply.c create_ciph.c \
	create_death_packet.c create_ticket.c debug_decl.c decomp_ticket.c \
	des_rw.c dest_tkt.c extract_ticket.c fgetst.c get_ad_tkt.c \
	get_admhst.c get_cred.c get_in_tkt.c get_krbhst.c get_krbrlm.c \
	get_phost.c get_pw_tkt.c get_request.c get_svc_in_tkt.c \
	get_tf_fullname.c get_tf_realm.c getrealm.c getst.c in_tkt.c \
	k_gethostname.c klog.c kname_parse.c kntoln.c kparse.c \
	krb_err_txt.c krb_get_in_tkt.c kuserok.c log.c mk_err.c \
	mk_priv.c mk_req.c mk_safe.c month_sname.c \
	netread.c netwrite.c one.c pkt_cipher.c pkt_clen.c rd_err.c \
	rd_priv.c rd_req.c rd_safe.c read_service_key.c recvauth.c \
	save_credentials.c send_to_kdc.c sendauth.c stime.c tf_util.c \
	tkt_string.c util.c

LDADD+=	-lcom_err

beforeinstall: krb_err.c
	-cd ${.OBJDIR}; cmp -s krb_err.h \
	    ${DESTDIR}/usr/include/kerberosIV/krb_err.h || \
	    install -c -o ${BINOWN} -g ${BINGRP} -m 444 krb_err.h \
	    ${DESTDIR}/usr/include/kerberosIV

MAN3=	krb.3 krb_realmofhost.3 krb_sendauth.3 krb_set_tkt_string.3 \
	kuserok.3 tf_util.3

MLINKS=	krb.3 krb_mk_req.3 krb.3 krb_rd_req.3 krb.3 krb_kntoln.3 \
	krb.3 krb_set_key.3 krb.3 krb_get_cred.3 krb.3 krb_mk_priv.3 \
	krb.3 krb_rd_priv.3 krb.3 krb_mk_safe.3 krb.3 krb_rd_safe.3 \
	krb.3 krb_mk_err.3 krb.3 krb_rd_err.3 krb.3 krb_ck_repl.3 \
	krb.3 krb_get_local_addr.3 krb.3 krb_bind_local_addr.3

MLINKS+=krb_realmofhost.3 krb_get_phost.3 krb_realmofhost.3 krb_get_krbhst.3 \
	krb_realmofhost.3 krb_get_admhst.3 krb_realmofhost.3 krb_get_lrealm.3

MLINKS+=krb_realmofhost.3 realm.3

MLINKS+=krb_sendauth.3 krb_recvauth.3 krb_sendauth.3 krb_net_write.3 \
	krb_sendauth.3 krb_net_read.3

MLINKS+=krb_sendauth.3 ksend.3

MLINKS+=tf_util.3 tf_init.3 tf_util.3 tf_get_pname.3 \
	tf_util.3 tf_get_pinst.3 tf_util.3 tf_get_cred.3 \
	tf_util.3 tf_close.3

.include <bsd.lib.mk>

krb_err.c: ${KRBOBJDIR}/krb_err.h

