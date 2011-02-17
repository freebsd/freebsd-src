# $FreeBSD$

.include <bsd.own.mk>

LIB=		pkg

SHLIBDIR?=	/usr/lib
SHLIB_MAJOR=	0

SRCS=		deps.c		\
		exec.c		\
		file.c		\
		global.c	\
		match.c		\
		msg.c		\
		pen.c		\
		pkgwrap.c	\
		plist.c		\
		str.c		\
		url.c		\
		version.c
INCS=		pkg.h

CFLAGS+=	-DYES_I_KNOW_THE_API_IS_RUBBISH_AND_IS_DOOMED_TO_CHANGE

DPADD=		${LIBFETCH} ${LIBMD} ${LIBUTIL}
LDADD=		-lfetch -lmd -lutil

.if ${MK_OPENSSL} != "no"
DPADD+=         ${LIBSSL} ${LIBCRYPTO}
LDADD+=         -lssl -lcrypto
.endif

WARNS?=		3

DATE!= grep LIBPKG_VERSION ${.CURDIR}/pkg.h | sed 's|.*[      ]||'

distfile: clean
	@(cd ${.CURDIR}/..; \
		cp -r libpkg libpkg-${DATE}; \
		tar -czf libpkg/libpkg-${DATE}.tar.gz \
			--exclude .#* --exclude *~ --exclude CVS \
			--exclude .svn --exclude libpkg-*.tar.gz \
			libpkg-${DATE}; \
		rm -rf libpkg-${DATE})

.include <bsd.lib.mk>
