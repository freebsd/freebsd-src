#
# $FreeBSD$
#

USE_CHERI=	yes

.include <bsd.own.mk>

PROG=	tcpdump-helper
SRCS=	tcpdump-helper.c			\
	pcap_datalink_val_to_name.c		\
	pcap-eproto_db.c			\
	stub.c					\
	chsbrt.S

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
LDFLAGS+=	-Wl,--script=${.CURDIR}/sandbox.ld -nostdlib

.if ${MACHINE_CPUARCH} != "i386"
CFLAGS+=	-DLBL_ALIGN
.endif

MAN=
#STRIP=
DPADD=	${LIBC_CHERI}
LDADD=	-lc_cheri

NO_SHARED=	yes

#
# For libc files!
#
NO_WERROR=	yes

FILESOWN=       ${LIBOWN}
FILESGRP=       ${LIBGRP}
FILESMODE=      ${LIBMODE}
FILESDIR=       ${LIBEXECDIR}
FILES=		tcpdump-helper.bin tcpdump-helper.dump chsbrt.o
CLEANFILES=	tcpdump-helper.bin tcpdump-helper.dump chsbrt.o

chsbrt.o:
	${AS} -G0 -o ${.TARGET} ${.ALLSRC}

tcpdump-helper.bin: tcpdump-helper
	objcopy -S -O binary ${.ALLSRC} ${.TARGET}
	@echo created ${.TARGET}

tcpdump-helper.dump: tcpdump-helper
	objdump -xsSD ${.ALLSRC} > ${.TARGET}

.PATH: ${.CURDIR}/${MACHINE_ARCH}
.PATH: ${.CURDIR}/../../lib/libc/${MACHINE}/string
.PATH: ${.CURDIR}/../../lib/libc/string
.PATH: ${.CURDIR}/../../contrib/tcpdump

.include <bsd.prog.mk>
