#	From: @(#)Makefile	5.1 (Berkeley) 6/25/90
#	$Id: Makefile,v 1.2 1995/01/14 20:51:05 wollman Exp $

LIB=	krb
SHLIB_MAJOR= 2
SHLIB_MINOR= 0
CFLAGS+=-DKERBEROS -DCRYPT -DDEBUG -I${.CURDIR}/../include -DBSD42
SRCS=	create_auth_reply.c create_ciph.c \
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

TDIR=	${.CURDIR}/..
krb_err.et.c: krb_err.et
	${COMPILE_ET} ${.CURDIR}/krb_err.et

beforedepend:	krb_err.et.c

CLEANFILES+=	krb_err.et.c krb_err.h

.include <bsd.lib.mk>
