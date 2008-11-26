# $FreeBSD: src/usr.bin/csup/Makefile,v 1.3.2.1.6.1 2008/10/02 02:57:24 kensmith Exp $

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
