# $FreeBSD$

BIND_DIR=${.CURDIR}/../../contrib/bind

CFLAGS+= -I${BIND_DIR}/port/freebsd/include -I${BIND_DIR}/include

LIB=	bind

.PATH:	${BIND_DIR}/lib/resolv
SRCS+=	herror.c res_debug.c res_data.c res_comp.c res_init.c \
	res_mkquery.c res_query.c res_send.c res_mkupdate.c \
	res_update.c

.if defined(WANT_IRS_AUDIT)
.PATH:	${BIND_DIR}/lib/irs
SRCS+=	dns.c dns_gr.c dns_ho.c dns_nw.c dns_pr.c dns_pw.c dns_sv.c gen.c \
	gen_gr.c gen_ho.c gen_ng.c gen_nw.c gen_pr.c gen_pw.c gen_sv.c \
	getgrent.c gethostent.c getnetent.c getnetgrent.c getprotoent.c \
	getpwent.c getservent.c hesiod.c irs_data.c lcl.c lcl_gr.c lcl_ho.c \
	lcl_ng.c lcl_nw.c lcl_pr.c lcl_pw.c lcl_sv.c nis.c nis_gr.c nis_ho.c \
	nis_ng.c nis_nw.c nis_pr.c nis_pw.c nis_sv.c nul_ng.c util.c
.endif

.PATH:	${BIND_DIR}/lib/isc
SRCS+=	tree.c base64.c bitncmp.c assertions.c \
	memcluster.c logging.c heap.c \
	eventlib.c ev_connects.c ev_files.c \
	ev_timers.c ev_streams.c ev_waits.c

.PATH:	${BIND_DIR}/lib/inet
SRCS+=	nsap_addr.c inet_addr.c inet_ntop.c inet_pton.c \
	inet_ntoa.c inet_neta.c inet_net_ntop.c inet_net_pton.c \
	inet_lnaof.c inet_makeaddr.c inet_netof.c inet_network.c

.PATH:	${BIND_DIR}/lib/nameser
SRCS+=	ns_parse.c ns_print.c ns_netint.c ns_ttl.c ns_name.c

INTERNALLIB=		true
NOPIC=			true
INTERNALSTATICLIB=	true

.include <bsd.lib.mk>
