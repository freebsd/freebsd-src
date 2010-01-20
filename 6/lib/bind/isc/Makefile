# $FreeBSD$

BIND_DIR=	${.CURDIR}/../../../contrib/bind9
LIB_BIND_REL=	..
LIB_BIND_DIR=	${.CURDIR}/${LIB_BIND_REL}
SRCDIR=		${BIND_DIR}/lib/isc

.include	"${LIB_BIND_DIR}/config.mk"

LIB=		isc

.PATH:		${SRCDIR}/unix
SRCS+=		app.c dir.c entropy.c \
		errno2result.c file.c fsaccess.c \
		interfaceiter.c keyboard.c net.c \
		os.c resource.c socket.c stdio.c \
		stdtime.c strerror.c syslog.c time.c \

.PATH:		${SRCDIR}/nls
SRCS+=		msgcat.c \

.PATH:		${SRCDIR}/nothreads
SRCS+=		condition.c mutex.c \
		thread.c

.PATH:		${SRCDIR}
SRCS+=		inet_pton.c \
		assertions.c base64.c bitstring.c buffer.c \
		bufferlist.c commandline.c error.c event.c \
		hash.c heap.c hex.c hmacmd5.c \
		lex.c lfsr.c lib.c log.c md5.c \
		mem.c mutexblock.c netaddr.c netscope.c ondestroy.c \
		parseint.c portset.c print.c quota.c random.c \
		ratelimiter.c region.c result.c rwlock.c \
		serial.c sha1.c  sockaddr.c string.c strtoul.c \
		symtab.c task.c taskpool.c timer.c version.c

CFLAGS+=	-I${SRCDIR}/unix/include -I${SRCDIR}/nothreads/include
CFLAGS+=	-I${SRCDIR}/include -I${.CURDIR}

.if defined(WITH_BIND_LIBS)
INCS=		${SRCDIR}/include/isc/app.h \
		${SRCDIR}/include/isc/assertions.h \
		${SRCDIR}/include/isc/base64.h \
		${SRCDIR}/include/isc/bitstring.h \
		${SRCDIR}/include/isc/boolean.h \
		${SRCDIR}/include/isc/buffer.h \
		${SRCDIR}/include/isc/bufferlist.h \
		${SRCDIR}/include/isc/commandline.h \
		${SRCDIR}/include/isc/entropy.h \
		${SRCDIR}/include/isc/error.h \
		${SRCDIR}/include/isc/event.h \
		${SRCDIR}/include/isc/eventclass.h \
		${SRCDIR}/include/isc/file.h \
		${SRCDIR}/include/isc/formatcheck.h \
		${SRCDIR}/include/isc/fsaccess.h \
		${SRCDIR}/include/isc/hash.h \
		${SRCDIR}/include/isc/heap.h \
		${SRCDIR}/include/isc/hex.h \
		${SRCDIR}/include/isc/hmacmd5.h \
		${SRCDIR}/include/isc/interfaceiter.h \
		${SRCDIR}/include/isc/ipv6.h \
		${SRCDIR}/include/isc/lang.h \
		${SRCDIR}/include/isc/lex.h \
		${SRCDIR}/include/isc/lfsr.h \
		${SRCDIR}/include/isc/lib.h \
		${SRCDIR}/include/isc/list.h \
		${SRCDIR}/include/isc/log.h \
		${SRCDIR}/include/isc/magic.h \
		${SRCDIR}/include/isc/md5.h \
		${SRCDIR}/include/isc/mem.h \
		${SRCDIR}/include/isc/msgcat.h \
		${SRCDIR}/include/isc/msgs.h \
		${SRCDIR}/include/isc/mutexblock.h \
		${SRCDIR}/include/isc/netaddr.h \
		${SRCDIR}/include/isc/netscope.h \
		${SRCDIR}/include/isc/ondestroy.h \
		${SRCDIR}/include/isc/os.h \
		${SRCDIR}/include/isc/parseint.h \
		${SRCDIR}/include/isc/portset.h \
		${SRCDIR}/include/isc/print.h \
		${SRCDIR}/include/isc/quota.h \
		${SRCDIR}/include/isc/random.h \
		${SRCDIR}/include/isc/ratelimiter.h \
		${SRCDIR}/include/isc/refcount.h \
		${SRCDIR}/include/isc/region.h \
		${SRCDIR}/include/isc/resource.h \
		${SRCDIR}/include/isc/result.h \
		${SRCDIR}/include/isc/resultclass.h \
		${SRCDIR}/include/isc/rwlock.h \
		${SRCDIR}/include/isc/serial.h \
		${SRCDIR}/include/isc/sha1.h \
		${SRCDIR}/include/isc/sockaddr.h \
		${SRCDIR}/include/isc/socket.h \
		${SRCDIR}/include/isc/stdio.h \
		${SRCDIR}/include/isc/stdlib.h \
		${SRCDIR}/include/isc/string.h \
		${SRCDIR}/include/isc/symtab.h \
		${SRCDIR}/include/isc/task.h \
		${SRCDIR}/include/isc/taskpool.h \
		${SRCDIR}/include/isc/timer.h \
		${SRCDIR}/include/isc/types.h \
		${SRCDIR}/include/isc/util.h \
		${SRCDIR}/include/isc/version.h \
		${SRCDIR}/nothreads/include/isc/condition.h \
		${SRCDIR}/nothreads/include/isc/mutex.h \
		${SRCDIR}/nothreads/include/isc/once.h \
		${SRCDIR}/nothreads/include/isc/thread.h \
		${SRCDIR}/unix/include/isc/dir.h \
		${SRCDIR}/unix/include/isc/int.h \
		${SRCDIR}/unix/include/isc/keyboard.h \
		${SRCDIR}/unix/include/isc/net.h \
		${SRCDIR}/unix/include/isc/netdb.h \
		${SRCDIR}/unix/include/isc/offset.h \
		${SRCDIR}/unix/include/isc/stat.h \
		${SRCDIR}/unix/include/isc/stdtime.h \
		${SRCDIR}/unix/include/isc/strerror.h \
		${SRCDIR}/unix/include/isc/syslog.h \
		${SRCDIR}/unix/include/isc/time.h \
		isc/platform.h

INCSDIR=	${INCLUDEDIR}/isc
.endif

.include <bsd.lib.mk>
