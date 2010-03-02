# $FreeBSD$

PREFIX?=	/usr/local
BINDIR?=	${PREFIX}/bin
MANDIR?=	${PREFIX}/man/man

UNAME!=		/usr/bin/uname -s

PROG=	csup
SRCS=	attrstack.c auth.c config.c detailer.c diff.c fattr.c fixups.c fnmatch.c \
	globtree.c idcache.c keyword.c lister.c main.c misc.c mux.c parse.y \
	pathcomp.c proto.c status.c stream.c threads.c token.l updater.c \
	rcsfile.c rcsparse.c lex.rcs.c rsyncfile.c

CFLAGS+=	-I. -I${.CURDIR} -g -pthread -DHAVE_FFLAGS -DNDEBUG
WARNS?=		1

DPADD=	${LIBCRYPTO} ${LIBZ}
LDADD=	-lcrypto -lz

SCRIPTS=	cpasswd.sh
MAN=		csup.1 cpasswd.1

.include <bsd.prog.mk>
