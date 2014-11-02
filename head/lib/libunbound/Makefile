# $FreeBSD$

# Vendor sources and generated files
LDNSDIR= ${.CURDIR}/../../contrib/ldns
UNBOUNDDIR= ${.CURDIR}/../../contrib/unbound

# Hold my beer and watch this
.PATH: ${UNBOUNDDIR} ${UNBOUNDDIR}/iterator ${UNBOUNDDIR}/ldns ${UNBOUNDDIR}/libunbound ${UNBOUNDDIR}/services ${UNBOUNDDIR}/services/cache ${UNBOUNDDIR}/util ${UNBOUNDDIR}/util/data ${UNBOUNDDIR}/util/storage ${UNBOUNDDIR}/validator 

LIB=	unbound
PRIVATELIB=

CFLAGS= -I${UNBOUNDDIR}	-I${LDNSDIR} -I${.OBJDIR}

SRCS=	alloc.c autotrust.c config_file.c configlexer.l configparser.y \
	context.c dname.c dns.c dnstree.c fptr_wlist.c infra.c \
	iter_delegpt.c iter_donotq.c iter_fwd.c iter_hints.c iter_priv.c \
	iter_resptype.c iter_scrub.c iter_utils.c iterator.c keyraw.c \
	libunbound.c libworker.c listen_dnsport.c localzone.c locks.c log.c \
	lookup3.c lruhash.c mesh.c mini_event.c modstack.c module.c \
	msgencode.c msgparse.c msgreply.c net_help.c netevent.c \
	outbound_list.c outside_network.c packed_rrset.c parse.c \
	parseutil.c random.c rbtree.c regional.c rrdef.c rrset.c rtt.c \
	sbuffer.c slabhash.c str2wire.c timehist.c tube.c val_anchor.c \
	val_kcache.c val_kentry.c val_neg.c val_nsec.c val_nsec3.c \
	val_secalgo.c val_sigcrypt.c val_utils.c validator.c \
	winsock_event.c wire2str.c

WARNS?=	3

DPADD+=	${LIBSSL} ${LIBCRYPTO} ${LIBPTHREAD}
LDADD+=	-lssl -lcrypto -lpthread

# Misnamed file in upstream source
configlexer.l: configlexer.lex
	cp -fp ${.ALLSRC} ${.TARGET}
CLEANFILES+= configlexer.l

# Symbol prefix for lex and yacc
LFLAGS= -Pub_c_
YFLAGS= -pub_c_ -d

.include <bsd.lib.mk>
