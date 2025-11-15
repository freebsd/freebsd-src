BLOCKLIST_DIR=${SRCTOP}/contrib/blocklist
.PATH: ${BLOCKLIST_DIR}/bin ${BLOCKLIST_DIR}/port

PACKAGE=	blocklist

CONFS=	blocklistd.conf
PROG=	blocklistd
SRCS=	blocklistd.c conf.c run.c state.c support.c internal.c \
	sockaddr_snprintf.c pidfile.c strtoi.c popenve.c vsyslog_r.c
MAN=	blocklistd.8 blocklistd.conf.5

LDFLAGS+=-L${LIBBLOCKLISTDIR}
LIBADD+= blocklist util

CFLAGS+=-I${BLOCKLIST_DIR}/include -I${BLOCKLIST_DIR}/port \
	-D_PATH_BLCONTROL=\"/usr/libexec/blocklistd-helper\" \
	-DHAVE_CONFIG_H -DHAVE_DB_H -DHAVE_LIBUTIL_H \
	-DHAVE_CLOCK_GETTIME -DHAVE_FGETLN -DHAVE_FPARSELN \
	-DHAVE_GETPROGNAME -DHAVE_STRLCAT -DHAVE_STRLCPY \
	-DHAVE_STRUCT_SOCKADDR_SA_LEN -DHAVE_SYS_CDEFS_H
# CFLAGS+= -D_REENTRANT

.include <bsd.prog.mk>
