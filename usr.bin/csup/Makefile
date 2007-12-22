# $FreeBSD$

.PATH: ${.CURDIR}/../../contrib/csup

PROG=	csup
SRCS=	attrstack.c \
	config.c \
	detailer.c \
	diff.c \
	fattr.c \
	fixups.c \
	fnmatch.c \
	globtree.c \
	idcache.c \
	keyword.c \
	lister.c \
	main.c \
	misc.c \
	mux.c \
	parse.y \
	pathcomp.c \
	proto.c \
	status.c \
	stream.c \
	threads.c \
	token.l \
	updater.c

CFLAGS+= -I. -I${.CURDIR}/../../contrib/csup
CFLAGS+= -DHAVE_FFLAGS -DNDEBUG
WARNS?=	6

DPADD=	${LIBCRYPTO} ${LIBZ} ${LIBPTHREAD}
LDADD=	-lcrypto -lz -lpthread

.include <bsd.prog.mk>
