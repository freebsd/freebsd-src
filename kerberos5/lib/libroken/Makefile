# $FreeBSD$

LIB=		roken
CFLAGS+=	-I${KRB5DIR}/include \
		-I${INCLUDEOBJDIR} \
		-I${KRB5DIR}/lib/roken \
		-I${.OBJDIR}
SRCS=		base64.c concat.c emalloc.c eread.c erealloc.c estrdup.c \
		ewrite.c get_default_username.c get_window_size.c getarg.c \
		getnameinfo_verified.c issuid.c k_getpwnam.c k_getpwuid.c \
		mini_inetd.c net_read.c net_write.c parse_bytes.c \
		parse_time.c parse_units.c print_version.c resolve.c \
		roken_gethostby.c signal.c simple_exec.c snprintf.c \
		socket.c tm2time.c verify.c warnerr.c copyhostent.c \
		strlwr.c strndup.c strnlen.c strupr.c get_default_username.c \
		strcollect.c getaddrinfo_hostspec.c \
		print_version.h roken.h
NOPIC=		yes

install:

.include <bsd.lib.mk>

beforedepend all:	print_version.h

.PATH:		${KRB5DIR}/lib/roken

build-tools:	make-print-version

print_version.h: make-print-version
	./make-print-version print_version.h

make-print-version: make-print-version.c
	${CC} ${CFLAGS} -o ${.TARGET} ${.OODATE}

CLEANFILES+=	make-print-version print_version.h
