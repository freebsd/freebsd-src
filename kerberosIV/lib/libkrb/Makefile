# $FreeBSD$

LIB=		krb
CFLAGS+=	-I${KRB4DIR}/include \
		-I${INCLUDEOBJDIR} \
		-I${KRB4DIR}/lib/krb \
		-I${KRB4DIR}/lib/roken \
		-I${KRBOBJDIR} \
		-I${.OBJDIR}
DPADD+=		${LIBCRYPT}
LDADD+=		-lcrypt
SRCS=		check_time.c cr_err_reply.c create_auth_reply.c \
		create_ciph.c create_death_packet.c create_ticket.c \
		debug_decl.c decomp_ticket.c defaults.c dest_tkt.c \
		encrypt_ktext.c extra.c get_ad_tkt.c getfile.c get_cred.c \
		get_default_principal.c get_host.c get_in_tkt.c get_krbrlm.c \
		get_svc_in_tkt.c get_tf_fullname.c get_tf_realm.c getaddrs.c \
		getrealm.c getst.c k_getport.c k_getsockinst.c k_localtime.c \
		kdc_reply.c kntoln.c krb_check_auth.c krb_equiv.c \
		krb_err_txt.c krb_get_in_tkt.c kuserok.c lifetime.c \
		logging.c lsb_addr_comp.c mk_auth.c mk_err.c mk_priv.c \
		mk_req.c mk_safe.c month_sname.c name2name.c krb_net_read.c \
		krb_net_write.c one.c parse_name.c rd_err.c rd_priv.c \
		rd_req.c rd_safe.c read_service_key.c realm_parse.c \
		recvauth.c rw.c save_credentials.c send_to_kdc.c sendauth.c \
		solaris_compat.c stime.c str2key.c tf_util.c time.c \
		tkt_string.c unparse_name.c verify_user.c krb_ip_realm.c \
		base64.c concat.c flock.c gethostname.c gettimeofday.c \
		getuid.c net_read.c net_write.c resolve.c snprintf.c \
		strcasecmp.c strlcat.c strlcpy.c strdup.c strncasecmp.c \
		strnlen.c strtok_r.c swab.c ${KRBOBJDIR}/krb_err.c \
		${KRBOBJDIR}/krb_err.h protos.h
INCS=		krb.h klog.h krb-protos.h prot.h ${KRBOBJDIR}/krb_err.h
MAN=		${KRB4DIR}/man/kerberos.3 \
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

.include <bsd.lib.mk>

.PATH:		${KRB4DIR}/lib/krb
.PATH:		${KRB4DIR}/lib/roken
