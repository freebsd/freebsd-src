#	$Id$

LIB=		krb
CFLAGS+=	-I${KRB4DIR}/include \
		-I${INCLUDEOBJDIR} \
		-I${KRB4DIR}/lib/roken \
		-I${KRB4DIR}/lib/krb \
		-I${KRBOBJDIR}
SRCS=		cr_err_reply.c create_auth_reply.c create_ciph.c \
         	create_death_packet.c create_ticket.c dest_tkt.c get_in_tkt.c \
          	get_svc_in_tkt.c getrealm.c k_localtime.c krb_err_txt.c \
          	krb_get_in_tkt.c kuserok.c parse_name.c kntoln.c mk_auth.c \
	  	krb_check_auth.c mk_err.c mk_safe.c rd_err.c rd_safe.c \
          	recvauth.c mk_priv.c rd_req.c decomp_ticket.c lifetime.c \
          	month_sname.c stime.c read_service_key.c getst.c sendauth.c \
          	netread.c netwrite.c rd_priv.c krb_equiv.c str2key.c \
		get_ad_tkt.c mk_req.c get_cred.c get_tf_realm.c \
		get_tf_fullname.c one.c save_credentials.c send_to_kdc.c \
		get_host.c get_krbrlm.c k_gethostname.c tf_util.c \
		debug_decl.c k_flock.c tkt_string.c getaddrs.c \
		k_getsockinst.c k_getport.c lsb_addr_comp.c name2name.c \
		get_default_principal.c realm_parse.c verify_user.c \
	  	rw.c kdc_reply.c encrypt_ktext.c swab.c gettimeofday.c \
	  	check_time.c krb_err.c et_list.c resolve.c unparse_name.c \
	  	logging.c k_concat.c \
		snprintf.c strdup.c strtok_r.c strcasecmp.c \
		${KRBOBJDIR}/krb_err.h
MAN3=		${KRB4DIR}/man/kerberos.3 \
		${KRB4DIR}/man/krb_realmofhost.3 \
		${KRB4DIR}/man/krb_sendauth.3 \
		${KRB4DIR}/man/krb_set_tkt_string.3 \
		${KRB4DIR}/man/kuserok.3 \
		${KRB4DIR}/man/tf_util.3
MLINKS=		kerberos.3 krb_mk_req.3 \
		kerberos.3 krb_rd_req.3 \
		kerberos.3 krb_kntoln.3 \
		kerberos.3 krb_set_key.3 \
		kerberos.3 krb_get_cred.3 \
		kerberos.3 krb_mk_priv.3 \
		kerberos.3 krb_rd_priv.3 \
		kerberos.3 krb_mk_safe.3 \
		kerberos.3 krb_rd_safe.3 \
		kerberos.3 krb_mk_err.3 \
		kerberos.3 krb_rd_err.3 \
		kerberos.3 krb_ck_repl.3 \
		krb_realmofhost.3 krb_get_phost.3 \
		krb_realmofhost.3 krb_get_krbhst.3 \
		krb_realmofhost.3 krb_get_admhst.3 \
		krb_realmofhost.3 krb_get_lrealm.3 \
		krb_sendauth.3 krb_recvauth.3 \
		krb_sendauth.3 krb_net_write.3 \
		krb_sendauth.3 krb_net_read.3 \
		tf_util.3 tf_init.3 \
		tf_util.3 tf_get_pname.3 \
		tf_util.3 tf_get_pinst.3 \
		tf_util.3 tf_get_cred.3 \
		tf_util.3 tf_close.3

INCLUDES=	${KRB4DIR}/lib/krb/krb.h ${KRB4DIR}/lib/krb/klog.h \
		${KRB4DIR}/lib/krb/prot.h ${KRBOBJDIR}/krb_err.h

.include <bsd.lib.mk>

.PATH:		${KRB4DIR}/lib/krb
.PATH:		${KRB4DIR}/lib/roken
