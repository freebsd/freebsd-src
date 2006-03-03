# $FreeBSD$

.PATH: ${.CURDIR}/../../contrib/csup

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

CFLAGS+=-I${.CURDIR} -I${.CURDIR}/../../contrib/csup
CFLAGS+=-DHAVE_FFLAGS -DNDEBUG -pthread
WARNS?=	6

DPADD=	${LIBCRYPTO} ${LIBZ}
LDADD=	-lcrypto -lz

.include <bsd.prog.mk>
