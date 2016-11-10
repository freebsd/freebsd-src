# $FreeBSD$

NEED_CHERI=	hybrid
NO_SHARED=

.include <src.opts.mk>

TCPDUMP_DISTDIR?= ${.CURDIR}/../../../contrib/tcpdump
.PATH: ${TCPDUMP_DISTDIR}

PROG=	cheri_tcpdump
MAN=

SRCS=	\
	gmt2local.c \
	machdep.c \
	setsignal.c \
	tcpdump.c \
	util.c \
	print.c \
	print-ascii.c \
	ts_print.c \
	version.c
CLEANFILES+=	version.c

#.include "Makefile.print"

CFLAGS+=	-DCHERI_TCPDUMP

CFLAGS+= -I${.CURDIR} -I${TCPDUMP_DISTDIR}
CFLAGS+= -I${.CURDIR}/../../../lib/libcheri
CFLAGS+= -DHAVE_CONFIG_H
CFLAGS+= -D_U_="__attribute__((unused))"

CFLAGS+= -DCOLORIZE -DCHERI_HACK

.if ${MACHINE_CPUARCH} != "i386"
CFLAGS+=	-DLBL_ALIGN
.endif

LIBADD=	cheri l pcap
#.if ${MK_CASPER} != "no"
#LIBADD+=	capsicum
#CFLAGS+=	-DHAVE_CAPSICUM
#.endif
.if ${MK_OPENSSL} != "no"
LIBADD+=	crypto
CFLAGS+=	-I${DESTDIR}/usr/include/openssl
CFLAGS+=	-DHAVE_LIBCRYPTO -DHAVE_OPENSSL_EVP_H
.endif

.ifdef OAKLAND2015
CFLAGS+=	-DTCPDUMP_BENCHMARKING
.endif


version.c: ${TCPDUMP_DISTDIR}/VERSION
	rm -f version.c ; \
	    sed 's/.*/char version[] = "&";/' ${TCPDUMP_DISTDIR}/VERSION \
		> version.c

WANT_DUMP=yes

.include <src.opts.mk>
.include <bsd.prog.mk>
