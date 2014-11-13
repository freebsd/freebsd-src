# $FreeBSD$

USE_CHERI=
NO_SHARED=

.include <src.opts.mk>

TCPDUMP_DISTDIR?= ${.CURDIR}/../../../contrib/tcpdump
.PATH: ${TCPDUMP_DISTDIR}

PROG=	cheri_tcpdump
MAN=

SRCS=	cheri_tcpdump_system.c \
	gmt2local.c \
	machdep.c \
	setsignal.c \
	tcpdump.c \
	util.c \
	util-print.c \
	print.c \
	print-ascii.c \
	version.c
CLEANFILES+=	version.c

#.include "Makefile.print"

CFLAGS+= -I${.CURDIR} -I${TCPDUMP_DISTDIR}
CFLAGS+= -I${.CURDIR}/../../../lib/libcheri
CFLAGS+= -DHAVE_CONFIG_H
CFLAGS+= -D_U_="__attribute__((unused))"

CFLAGS+= -DCOLORIZE

.if ${MACHINE_CPUARCH} != "i386"
CFLAGS+=	-DLBL_ALIGN
.endif

DPADD=	${LIBL} ${LIBPCAP} ${LIBCHERI}
LDADD=	-ll -lpcap -lcheri
.if ${MK_OPENSSL} != "no" && !defined(RELEASE_CRUNCH)
DPADD+= ${LIBCRYPTO}
LDADD+= -lcrypto
CFLAGS+= -I${DESTDIR}/usr/include/openssl
CFLAGS+= -DHAVE_LIBCRYPTO -DHAVE_OPENSSL_EVP_H
.endif

version.c: ${TCPDUMP_DISTDIR}/VERSION
	rm -f version.c ; \
	    sed 's/.*/char version[] = "&";/' ${TCPDUMP_DISTDIR}/VERSION \
		> version.c

WANT_DUMP=yes

.include <src.opts.mk>
.include <bsd.prog.mk>
