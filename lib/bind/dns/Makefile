# $FreeBSD$

.include <bsd.own.mk>

BIND_DIR=	${.CURDIR}/../../../contrib/bind9
LIB_BIND_REL=	..
LIB_BIND_DIR=	${.CURDIR}/${LIB_BIND_REL}
SRCDIR=		${BIND_DIR}/lib/dns

.include	"${LIB_BIND_DIR}/config.mk"

LIB=		dns

.PATH:		${SRCDIR}
SRCS+=		acache.c acl.c adb.c byaddr.c \
		cache.c callbacks.c compress.c \
		db.c dbiterator.c dbtable.c diff.c dispatch.c \
		dlz.c dnssec.c ds.c \
		dst_api.c dst_lib.c dst_parse.c dst_result.c \
		forward.c \
		gssapi_link.c gssapictx.c hmac_link.c \
		iptable.c \
		journal.c \
		key.c \
		keytable.c \
		lib.c log.c lookup.c \
		master.c masterdump.c message.c \
		name.c ncache.c nsec.c nsec3.c \
		openssl_link.c openssldh_link.c \
		openssldsa_link.c opensslrsa_link.c \
		order.c peer.c portlist.c \
		rbt.c rbtdb.c rbtdb64.c rcode.c rdata.c \
		rdatalist.c \
		rdataset.c rdatasetiter.c rdataslab.c request.c \
		resolver.c result.c rootns.c sdb.c sdlz.c soa.c ssu.c \
		stats.c tcpmsg.c time.c timer.c tkey.c \
		tsig.c ttl.c validator.c \
		version.c view.c xfrin.c zone.c zonekey.c zt.c

CFLAGS+=	-I${SRCDIR}/include/dst -I${SRCDIR}/include -I${SRCDIR}
CFLAGS+=	-I${.CURDIR}
CFLAGS+=	-I${BIND_DIR}/lib/isc/${ISC_ATOMIC_ARCH}/include

DPADD=		${CRYPTO_DPADD} ${PTHREAD_DPADD}
LDADD=		${CRYPTO_LDADD} ${PTHREAD_LDADD}

.if ${MK_BIND_LIBS} != "no"
DNSINCS=	${SRCDIR}/include/dns/acache.h \
		${SRCDIR}/include/dns/acl.h \
		${SRCDIR}/include/dns/adb.h \
		${SRCDIR}/include/dns/bit.h \
		${SRCDIR}/include/dns/byaddr.h \
		${SRCDIR}/include/dns/cache.h \
		${SRCDIR}/include/dns/callbacks.h \
		${SRCDIR}/include/dns/cert.h \
		${SRCDIR}/include/dns/compress.h \
		${SRCDIR}/include/dns/db.h \
		${SRCDIR}/include/dns/dbiterator.h \
		${SRCDIR}/include/dns/dbtable.h \
		${SRCDIR}/include/dns/diff.h \
		${SRCDIR}/include/dns/dispatch.h \
		${SRCDIR}/include/dns/dlz.h \
		${SRCDIR}/include/dns/dnssec.h \
		${SRCDIR}/include/dns/ds.h \
		${SRCDIR}/include/dns/events.h \
		${SRCDIR}/include/dns/fixedname.h \
		${SRCDIR}/include/dns/forward.h \
		${SRCDIR}/include/dns/iptable.h \
		${SRCDIR}/include/dns/journal.h \
		${SRCDIR}/include/dns/keyflags.h \
		${SRCDIR}/include/dns/keytable.h \
		${SRCDIR}/include/dns/keyvalues.h \
		${SRCDIR}/include/dns/lib.h \
		${SRCDIR}/include/dns/log.h \
		${SRCDIR}/include/dns/lookup.h \
		${SRCDIR}/include/dns/master.h \
		${SRCDIR}/include/dns/masterdump.h \
		${SRCDIR}/include/dns/message.h \
		${SRCDIR}/include/dns/name.h \
		${SRCDIR}/include/dns/ncache.h \
		${SRCDIR}/include/dns/nsec.h \
		${SRCDIR}/include/dns/nsec3.h \
		${SRCDIR}/include/dns/opcode.h \
		${SRCDIR}/include/dns/order.h \
		${SRCDIR}/include/dns/peer.h \
		${SRCDIR}/include/dns/portlist.h \
		${SRCDIR}/include/dns/rbt.h \
		${SRCDIR}/include/dns/rcode.h \
		${SRCDIR}/include/dns/rdata.h \
		${SRCDIR}/include/dns/rdatasetiter.h \
		${SRCDIR}/include/dns/rdataclass.h \
		${SRCDIR}/include/dns/rdatalist.h \
		${SRCDIR}/include/dns/rdataset.h \
		${SRCDIR}/include/dns/rdataslab.h \
		${SRCDIR}/include/dns/rdatatype.h \
		${SRCDIR}/include/dns/request.h \
		${SRCDIR}/include/dns/resolver.h \
		${SRCDIR}/include/dns/result.h \
		${SRCDIR}/include/dns/rootns.h \
		${SRCDIR}/include/dns/sdb.h \
		${SRCDIR}/include/dns/sdlz.h \
		${SRCDIR}/include/dns/secalg.h \
		${SRCDIR}/include/dns/secproto.h \
		${SRCDIR}/include/dns/soa.h \
		${SRCDIR}/include/dns/ssu.h \
		${SRCDIR}/include/dns/stats.h \
		${SRCDIR}/include/dns/tcpmsg.h \
		${SRCDIR}/include/dns/time.h \
		${SRCDIR}/include/dns/timer.h \
		${SRCDIR}/include/dns/tkey.h \
		${SRCDIR}/include/dns/tsig.h \
		${SRCDIR}/include/dns/ttl.h \
		${SRCDIR}/include/dns/types.h \
		${SRCDIR}/include/dns/validator.h \
		${SRCDIR}/include/dns/version.h \
		${SRCDIR}/include/dns/view.h \
		${SRCDIR}/include/dns/xfrin.h \
		${SRCDIR}/include/dns/zone.h \
		${SRCDIR}/include/dns/zonekey.h \
		${SRCDIR}/include/dns/zt.h \
		dns/enumtype.h \
		dns/enumclass.h \
		dns/rdatastruct.h

DNSINCSDIR=	${INCLUDEDIR}/dns

DSTINCS=	${SRCDIR}/include/dst/dst.h \
		${SRCDIR}/include/dst/gssapi.h \
		${SRCDIR}/include/dst/lib.h \
		${SRCDIR}/include/dst/result.h

DSTINCSDIR=	${INCLUDEDIR}/dst

INCSGROUPS=	DNSINCS DSTINCS
.endif

.if defined(MAINTAINER_MODE)
generate: ${.CURDIR}/dns/enumtype.h ${.CURDIR}/dns/enumclass.h \
	${.CURDIR}/dns/rdatastruct.h ${.CURDIR}/code.h

gen: ${SRCDIR}/gen.c

${.CURDIR}/dns/enumtype.h: gen
	(cd ${SRCDIR} && ${.OBJDIR}/gen -t) >${.TARGET}

${.CURDIR}/dns/enumclass.h: gen
	(cd ${SRCDIR} && ${.OBJDIR}/gen -c) >${.TARGET}

${.CURDIR}/dns/rdatastruct.h: gen
	(cd ${SRCDIR} && ${.OBJDIR}/gen -i -P rdata/rdatastructpre.h \
	    -S rdata/rdatastructsuf.h) >${.TARGET}

${.CURDIR}/code.h: gen
	(cd ${SRCDIR} && ${.OBJDIR}/gen) >${.TARGET}
.endif

.include <bsd.lib.mk>
