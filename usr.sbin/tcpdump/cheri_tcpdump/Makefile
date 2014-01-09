# $FreeBSD$

USE_CHERI=
NO_SHARED=

.include <bsd.own.mk>

TCPDUMP_DISTDIR?= ${.CURDIR}/../../../contrib/tcpdump
.PATH: ${TCPDUMP_DISTDIR}

PROG=	cheri_tcpdump
MAN=

SRCS=	gmt2local.c \
	machdep.c \
	setsignal.c \
	tcpdump.c \
	util.c \
	print.c \
	print-ascii.c \
	version.c
CLEANFILES+=	version.c

#.include "Makefile.print"

CFLAGS+= -I${.CURDIR} -I${TCPDUMP_DISTDIR}
CFLAGS+= -DHAVE_CONFIG_H
CFLAGS+= -D_U_="__attribute__((unused))"

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

.include <bsd.prog.mk>
