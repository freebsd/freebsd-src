# $FreeBSD: src/lib/libbind/Makefile,v 1.5 1999/11/30 04:40:54 peter Exp $

BIND_DIR=${.CURDIR}/../../contrib/bind

# contrib/bind/include/* must not override any real system includes
CFLAGS+= -I${BIND_DIR}/port/freebsd/include -I${BIND_DIR}/include

LIB=	bind
WANT_IRS=	for now

# This may or may not work yet.  It's not compatable with the core
# system components since it overrides the master.passwd handling etc.
.if defined(WANT_IRS)
.PATH:	${BIND_DIR}/lib/irs
SRCS+=	dns.c dns_gr.c dns_ho.c dns_nw.c dns_pr.c dns_pw.c \
	dns_sv.c gai_strerror.c gen.c gen_gr.c gen_ho.c \
	gen_ng.c gen_nw.c gen_pr.c gen_pw.c gen_sv.c \
	getaddrinfo.c getgrent.c getgrent_r.c gethostent.c \
	gethostent_r.c getnameinfo.c getnetent.c getnetent_r.c \
	getnetgrent.c getnetgrent_r.c getprotoent.c \
	getprotoent_r.c getpwent.c getpwent_r.c getservent.c \
	getservent_r.c hesiod.c irs_data.c \
	irp.c irp_gr.c irp_ho.c irp_ng.c irp_nw.c \
	irp_pr.c irp_pw.c irp_sv.c irpmarshall.c \
	lcl.c lcl_gr.c \
	lcl_ho.c lcl_ng.c lcl_nw.c lcl_pr.c lcl_pw.c \
	lcl_sv.c nis.c nis_gr.c nis_ho.c nis_ng.c nis_nw.c \
	nis_pr.c nis_pw.c nis_sv.c nul_ng.c util.c

.PATH:	${BIND_DIR}/lib/nameser
SRCS+=	ns_parse.c ns_print.c ns_netint.c ns_ttl.c ns_name.c \
	ns_sign.c ns_verify.c ns_date.c ns_samedomain.c

.PATH:	${BIND_DIR}/lib/resolv
SRCS+=	herror.c res_debug.c res_data.c res_comp.c res_init.c \
	res_mkquery.c res_query.c res_send.c res_sendsigned.c \
	res_mkupdate.c res_update.c res_findzonecut.c
.endif

.if defined(WANT_CYLINK) && exists(${BIND_DIR}/lib/cylink)
.PATH:	${BIND_DIR}/lib/cylink
CFLAGS+=-DCYLINK_DSS -I${BIND_DIR}/lib/cylink
SRCS+=	bn.c bn00.c lbn00.c lbnmem.c legal.c \
	bits.c dss.c math.c ctk_prime.c rand.c sha.c swap.c
.endif

.if defined(WANT_DNSSAFE) && exists(${BIND_DIR}/lib/dnssafe)
.PATH:	${BIND_DIR}/lib/dnssafe
CFLAGS+=-DDNSSAFE -I${BIND_DIR}/lib/dnssafe
SRCS+=	bgclrbit.c bgmdmpyx.c bgmdsqx.c bgmodexp.c \
	bgpegcd.c big2exp.c bigabs.c bigacc.c bigarith.c \
	bigcmp.c bigconst.c biginv.c biglen.c bigmodx.c \
	bigmpy.c bigpdiv.c bigpmpy.c bigpmpyh.c bigpmpyl.c \
	bigpsq.c bigqrx.c bigsmod.c bigtocan.c bigu.c \
	bigunexp.c cantobig.c crt2.c \
	digrand.c intbits.c md5.c md5rand.c prime.c \
	rsa.c rsakeygn.c seccbcd.c seccbce.c surrendr.c \
	ahchdig.c ahchencr.c ahchgen.c ahchrand.c ahdigest.c \
	ahencryp.c ahgen.c ahrandom.c ahrsaenc.c ahrsaepr.c \
	ahrsaepu.c aichdig.c aichenc8.c aichencn.c aichencr.c \
	aichgen.c aichrand.c aimd5.c \
	aimd5ran.c ainfotyp.c ainull.c airsaepr.c airsaepu.c \
	airsakgn.c airsaprv.c airsapub.c algchoic.c algobj.c \
	amcrte.c ammd5.c ammd5r.c \
	amrkg.c amrsae.c balg.c binfocsh.c bkey.c bmempool.c \
	digest.c encrypt.c generate.c intitem.c \
	keyobj.c ki8byte.c kiitem.c kinfotyp.c \
	kifulprv.c kipkcrpr.c kirsacrt.c kirsapub.c random.c
.endif

.PATH:	${BIND_DIR}/lib/dst
CFLAGS+=-DHMAC_MD5 -DUSE_MD5
SRCS+=	dst_api.c prandom.c rsaref_link.c support.c bsafe_link.c \
	cylink_link.c hmac_link.c md5_dgst.c eay_dss_link.c

.PATH:	${BIND_DIR}/lib/isc
SRCS+=	tree.c bitncmp.c assertions.c \
	memcluster.c logging.c heap.c \
	ctl_p.c ctl_srvr.c ctl_clnt.c \
	eventlib.c ev_connects.c ev_files.c \
	ev_timers.c ev_streams.c ev_waits.c
#	base64.c

INTERNALLIB=		true
NOPIC=			true
INTERNALSTATICLIB=	true

.include <bsd.lib.mk>
