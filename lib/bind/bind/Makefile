# $FreeBSD$

#.include <bsd.own.mk>

BIND_DIR=	${.CURDIR}/../../../contrib/bind9
LIB_BIND_REL=	..
LIB_BIND_DIR=	${.CURDIR}/${LIB_BIND_REL}
SRCDIR=		${BIND_DIR}/lib/bind

# XXX These should come before -I's from config.mk.
CFLAGS+=	-I${SRCDIR}/port/freebsd/include -I${SRCDIR}/include
CFLAGS+=	-I${.CURDIR}

.include	"${LIB_BIND_DIR}/config.mk"

LIB=		bind

.PATH:		${SRCDIR}/bsd
SRCS+=		ftruncate.c gettimeofday.c \
		mktemp.c putenv.c readv.c setenv.c \
		setitimer.c strcasecmp.c strdup.c \
		strerror.c strpbrk.c strtoul.c utimes.c \
		writev.c

.PATH:		${SRCDIR}/dst
SRCS+=		dst_api.c hmac_link.c md5_dgst.c support.c

.PATH:		${SRCDIR}/inet
SRCS+=		inet_addr.c inet_cidr_ntop.c inet_cidr_pton.c \
		inet_data.c inet_lnaof.c inet_makeaddr.c \
		inet_net_ntop.c inet_net_pton.c inet_neta.c \
		inet_netof.c inet_network.c inet_ntoa.c \
		inet_ntop.c inet_pton.c nsap_addr.c

.PATH:		${SRCDIR}/irs
SRCS+=		gethostent_r.c getnetgrent_r.c \
		getprotoent_r.c getservent_r.c \
		dns.c dns_ho.c dns_nw.c dns_pr.c \
		dns_sv.c gai_strerror.c gen.c gen_ho.c \
		gen_ng.c gen_nw.c gen_pr.c gen_sv.c \
		getaddrinfo.c gethostent.c getnameinfo.c \
		getnetent.c getnetent_r.c getnetgrent.c \
		getprotoent.c getservent.c hesiod.c \
		irp.c irp_ho.c irp_ng.c irp_nw.c \
		irp_pr.c irp_sv.c irpmarshall.c irs_data.c \
		lcl.c lcl_ho.c lcl_ng.c lcl_nw.c \
		lcl_pr.c lcl_sv.c nis.c nul_ng.c util.c

.PATH:		${SRCDIR}/isc
SRCS+=		assertions.c base64.c bitncmp.c ctl_clnt.c \
		ctl_p.c ctl_srvr.c ev_connects.c ev_files.c \
		ev_streams.c ev_timers.c ev_waits.c \
		eventlib.c heap.c hex.c logging.c \
		memcluster.c movefile.c tree.c

.PATH:		${SRCDIR}/nameser
SRCS+=		ns_date.c ns_name.c ns_netint.c \
		ns_parse.c ns_print.c ns_samedomain.c \
		ns_sign.c ns_ttl.c ns_verify.c

.PATH:		${SRCDIR}/resolv
SRCS+=		herror.c res_comp.c res_data.c \
		res_debug.c res_findzonecut.c res_init.c \
		res_mkquery.c res_mkupdate.c res_query.c \
		res_send.c res_sendsigned.c res_update.c

DPADD=		${PTHREAD_DPADD}
LDADD=		${PTHREAD_LDADD}

#.if ${MK_BIND_LIBS} != "no"
#INCS=		${SRCDIR}/include/isc/assertions.h \
#		${SRCDIR}/include/isc/ctl.h \
#		${SRCDIR}/include/isc/dst.h \
#		${SRCDIR}/include/isc/eventlib.h \
#		${SRCDIR}/include/isc/heap.h \
#		${SRCDIR}/include/isc/irpmarshall.h \
#		${SRCDIR}/include/isc/list.h \
#		${SRCDIR}/include/isc/logging.h \
#		${SRCDIR}/include/isc/memcluster.h \
#		${SRCDIR}/include/isc/misc.h \
#		${SRCDIR}/include/isc/tree.h
#
#INCSDIR=	${INCLUDEDIR}/isc
#.endif

.include <bsd.lib.mk>
