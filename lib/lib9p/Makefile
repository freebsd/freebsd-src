.include <src.opts.mk>

.PATH:		${.CURDIR}/../../contrib/lib9p
CFLAGS+=	-I${.CURDIR}
CFLAGS+=	-I${.CURDIR}/../../contrib/lib9p

LIBADD=		sbuf

.if ${MK_CASPER} != "no"
CFLAGS+=	-DWITH_CASPER
LIBADD+=	casper cap_pwd cap_grp
.endif

LIB=		9p
PACKAGE=	lib${LIB}
SHLIB_MAJOR=	1
SRCS=		connection.c \
		genacl.c \
		hashtable.c \
		log.c \
		pack.c \
		request.c \
		rfuncs.c \
		threadpool.c \
		utils.c \
		backend/fs.c \
		transport/socket.c

INCSDIR=	${INCLUDEDIR}/lib9p
INCS=		fid.h lib9p.h backend/fs.h

.include <bsd.lib.mk>
