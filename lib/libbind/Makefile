# $FreeBSD$

BIND_DIR=${.CURDIR}/../../contrib/bind

# contrib/bind/include/* must not override any real system includes
CFLAGS+= -I${BIND_DIR}/port/freebsd/include -I${DESTDIR}/usr/include \
	 -I${BIND_DIR}/include

LIB=	bind

# This may or may not work yet.  It's not compatable with the core
# system components since it overrides the master.passwd handling etc.
.if defined(WANT_IRS)
.PATH:	${BIND_DIR}/lib/irs
SRCS+=	dns.c dns_gr.c dns_ho.c dns_nw.c dns_pr.c dns_pw.c dns_sv.c gen.c \
	gen_gr.c gen_ho.c gen_ng.c gen_nw.c gen_pr.c gen_pw.c gen_sv.c \
	getgrent.c gethostent.c getnetent.c getnetgrent.c getprotoent.c \
	getpwent.c getservent.c hesiod.c irs_data.c lcl.c lcl_gr.c lcl_ho.c \
	lcl_ng.c lcl_nw.c lcl_pr.c lcl_pw.c lcl_sv.c nis.c nis_gr.c nis_ho.c \
	nis_ng.c nis_nw.c nis_pr.c nis_pw.c nis_sv.c nul_ng.c util.c
.endif

.PATH:	${BIND_DIR}/lib/isc
SRCS+=	tree.c bitncmp.c assertions.c \
	memcluster.c logging.c heap.c \
	eventlib.c ev_connects.c ev_files.c \
	ev_timers.c ev_streams.c ev_waits.c
#	base64.c

INTERNALLIB=		true
NOPIC=			true
INTERNALSTATICLIB=	true

.include <bsd.lib.mk>
