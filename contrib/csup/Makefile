# $FreeBSD$

PREFIX?=	/usr/local
BINDIR?=	${PREFIX}/bin
MANDIR?=	${PREFIX}/man/man

UNAME!=		/usr/bin/uname -s

PROG=	csup
SRCS=	attrstack.c attrstack.h \
	config.c config.h \
	detailer.c detailer.h \
	diff.c diff.h \
	fattr.c fattr.h fattr_bsd.h \
	fixups.c fixups.h \
	fnmatch.c fnmatch.h \
	globtree.c globtree.h \
	keyword.c keyword.h \
	lister.c lister.h \
	main.c main.h \
	misc.c misc.h \
	mux.c mux.h \
	parse.h parse.y \
	pathcomp.c pathcomp.h \
	proto.c proto.h \
	status.c status.h \
	stream.c stream.h \
	threads.c threads.h \
	token.h token.l \
	updater.c updater.h

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
