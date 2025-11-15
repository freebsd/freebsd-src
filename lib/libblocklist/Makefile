BLOCKLIST_DIR=${SRCTOP}/contrib/blocklist

.PATH: ${BLOCKLIST_DIR}/lib ${BLOCKLIST_DIR}/include ${BLOCKLIST_DIR}/port

PACKAGE=	blocklist
LIB=	blocklist
SHLIB_MAJOR= 0

LIBADD+= pthread

CFLAGS.clang+=-Wno-thread-safety-analysis

CFLAGS+=-I${BLOCKLIST_DIR}/include -I${BLOCKLIST_DIR}/port \
	-D_REENTRANT -DHAVE_CONFIG_H -DHAVE_DB_H -DHAVE_LIBUTIL_H \
	-DHAVE_CLOCK_GETTIME -DHAVE_FGETLN -DHAVE_GETPROGNAME \
	-DHAVE_STRLCAT -DHAVE_STRLCPY -DHAVE_STRUCT_SOCKADDR_SA_LEN \
	-DHAVE_SYS_CDEFS_H

SRCS=	bl.c blocklist.c vsyslog_r.c
INCS=	blocklist.h
MAN=	libblocklist.3

MLINKS=	libblocklist.3 blocklist_open.3		\
	libblocklist.3 blocklist_close.3	\
	libblocklist.3 blocklist.3		\
	libblocklist.3 blocklist_r.3		\
	libblocklist.3 blocklist_sa.3		\
	libblocklist.3 blocklist_sa_r.3

.include <bsd.lib.mk>
