#
# $FreeBSD$
#

USE_CHERI=	yes
USE_CHERI_STACK=	yes

.include <src.opts.mk>

PROG=	tcpdump-helper
SRCS=	tcpdump-helper.c			\
	pcap_datalink_val_to_name.c		\
	pcap-eproto_db.c			\
	stub.c					\
	util.c

.include "${.CURDIR}/../../usr.sbin/tcpdump/tcpdump/Makefile.print"

INCS=	tcpdump-helper.h

CFLAGS+=	-DHAVE_CONFIG_H \
		-DTCPDUMP_PRINT_ONLY \
		-DTCPDUMP_HELPER \
		-DCHERI_TCPDUMP_VULNERABILITY \
		-D_U_="__attribute__((unused))" \
		-I${.CURDIR}/../../contrib/tcpdump \
		-I${.CURDIR}/../../lib/libcheri \
		-I${.CURDIR}/../../usr.sbin/tcpdump/tcpdump \
		-I${.CURDIR}/../../usr.sbin/tcpdump/cheri_tcpdump
LDFLAGS+=	-Wl,--script=${.CURDIR}/../../lib/libc_cheri/sandbox.ld \
		-nostdlib

# Contains a table of formats selected by a function so safe-ish
CFLAGS.print-icmp.c= -Wno-format-nonliteral
CFLAGS.util-print.c= -Wno-format-nonliteral

.if ${MACHINE_CPUARCH} != "i386"
CFLAGS+=	-DLBL_ALIGN
.endif

MAN=
#STRIP=
DPADD=	${LIBC_CHERI}
LDADD=	-lc_cheri

NO_SHARED=	yes

FILESOWN=       ${LIBOWN}
FILESGRP=       ${LIBGRP}
FILESMODE=      ${LIBMODE}
FILESDIR=       ${LIBEXECDIR}
FILES=		tcpdump-helper.bin
CLEANFILES=	tcpdump-helper.bin

chsbrt.o:
	${AS} -G0 -o ${.TARGET} ${.ALLSRC}

tcpdump-helper.bin: tcpdump-helper
	objcopy -S -O binary ${.ALLSRC} ${.TARGET}
	@echo created ${.TARGET}

.PATH: ${.CURDIR}/${MACHINE_ARCH}
.PATH: ${.CURDIR}/../../lib/libc/${MACHINE}/string
.PATH: ${.CURDIR}/../../lib/libc/string
.PATH: ${.CURDIR}/../../contrib/tcpdump

WANT_DUMP=yes

.include <src.opts.mk>
.include <bsd.prog.mk>

CFLAGS:=	${CFLAGS:N-Wcast-align}
CFLAGS+= ${CFLAGS.${.IMPSRC:T}}
