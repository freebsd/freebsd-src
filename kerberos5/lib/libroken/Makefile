# $FreeBSD$

LIB=	roken
LDADD=	-lcrypt
DPADD=	${LIBCRYPT}
INCS=	roken.h roken-common.h

SRCS=	base64.c \
	bswap.c \
	concat.c \
	copyhostent.c \
	dumpdata.c \
	ecalloc.c \
	emalloc.c \
	environment.c \
	eread.c \
	erealloc.c \
	esetenv.c \
	estrdup.c \
	ewrite.c \
	get_default_username.c \
	get_window_size.c \
	getaddrinfo_hostspec.c \
	getarg.c \
	getnameinfo_verified.c \
	hex.c \
	hostent_find_fqdn.c \
	issuid.c \
	k_getpwnam.c \
	k_getpwuid.c \
	mini_inetd.c \
	ndbm_wrap.c \
	net_read.c \
	net_write.c \
	parse_bytes.c \
	parse_time.c \
	parse_units.c \
	resolve.c \
	roken.h \
	roken_gethostby.c \
	rtbl.c \
	simple_exec.c \
	snprintf.c \
	socket.c \
	strcollect.c \
	strlwr.c \
	strndup.c \
	strnlen.c \
	strpool.c \
	strsep_copy.c \
	strupr.c \
	timeval.c \
	tm2time.c \
	unvis.c \
	verify.c \
	vis.c \
	warnerr.c \
	write_pid.c

CFLAGS+=-I${KRB5DIR}/lib/roken -I.

CLEANFILES= roken.h

roken.h: ../../tools/make-roken/make-roken
	../../tools/make-roken/make-roken > ${.TARGET}

../../tools/make-roken/make-roken:
	cd ${.CURDIR}/../../tools/make-roken && ${MAKE}

.include <bsd.lib.mk>

.PATH: ${KRB5DIR}/lib/roken
