# $FreeBSD$

PREFIX?=	/usr/local
BINDIR?=	${PREFIX}/bin
MANDIR?=	${PREFIX}/man/man

UNAME!=		/usr/bin/uname -s

PROG=	csup
SRCS=	attrstack.c config.c detailer.c diff.c fattr.c fixups.c fnmatch.c \
	globtree.c idcache.c keyword.c lister.c main.c misc.c mux.c parse.y \
	pathcomp.c proto.c status.c stream.c threads.c token.l updater.c

CFLAGS+=	-I. -I${.CURDIR} -g -pthread -DHAVE_FFLAGS -DNDEBUG
WARNS?=		6

# A bit of tweaking is needed to get this Makefile working
# with the bsd.prog.mk of all the *BSD OSes...
.if (${UNAME} == "NetBSD")
LDFLAGS+=	-pthread
YHEADER=	yes

.elif (${UNAME} == "OpenBSD")
# I bet there's a better way to do this with the OpenBSD mk
# framework but well, this works and I got bored.
LDFLAGS+=	-pthread
YFLAGS=		-d
CLEANFILES+=	parse.c parse.h y.tab.h

config.c:	parse.h

token.l:	parse.h

y.tab.h:	parse.c

parse.h:	y.tab.h
	cp ${.ALLSRC} ${.TARGET}

.endif

DPADD=	${LIBCRYPTO} ${LIBZ}
LDADD=	-lcrypto -lz

.include <bsd.prog.mk>
