LIB=		9p
SHLIB_MAJOR=	1
SRCS=		pack.c \
		connection.c \
		request.c log.c \
		hashtable.c \
		genacl.c \
		utils.c \
		rfuncs.c \
		threadpool.c \
		transport/socket.c \
		backend/fs.c

INCS=		lib9p.h
CC=clang
CFLAGS=		-g -O2 -DL9P_DEBUG=L9P_DEBUG -DWITH_CASPER
# Note: to turn on debug, use -DL9P_DEBUG=L9P_DEBUG,
# and set env variable LIB9P_LOGGING to stderr or to
# the (preferably full path name of) the debug log file.

LIBADD=		sbuf libcasper libcap_pwd libcap_grp
SUBDIR=		example

cscope: .PHONY
	cd ${.CURDIR}; cscope -buq $$(find . -name '*.[ch]' -print)

.include <bsd.lib.mk>
