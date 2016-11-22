#
# $FreeBSD$
#

NEED_CHERI=	sandbox

.include <src.opts.mk>

PROG=	tcpdump-helper
SRCS=	ascii_strcasecmp.c			\
	netdissect.c				\
	tcpdump-helper.c			\
	pcap_datalink_val_to_name.c		\
	pcap-eproto_db.c			\
	stub.c

.include "${.CURDIR}/../../usr.sbin/tcpdump/tcpdump/Makefile.print"

INCS=	tcpdump-helper.h

CFLAGS+=	-DHAVE_CONFIG_H \
		-DTCPDUMP_PRINT_ONLY \
		-DTCPDUMP_HELPER \
		-DCHERI_TCPDUMP_INTERNAL \
		-D_U_="__attribute__((unused))" \
		-I${.CURDIR}/../../contrib/tcpdump \
		-I${.CURDIR}/../../lib/libcheri \
		-I${.CURDIR}/../../usr.sbin/tcpdump/tcpdump \
		-I${.CURDIR}/../../usr.sbin/tcpdump/cheri_tcpdump \
		-mxgot -mllvm -mxmxgot
.if ${MK_DEMO_VULNERABILITIES} != "no"
CFLAGS+=	-DCHERI_TCPDUMP_VULNERABILITY
.endif
LDFLAGS+=	-Wl,-T,${.CURDIR}/../../lib/libc_cheri/sandbox.ld \
		-nostdlib
WARNS=	3

# Contains a table of formats selected by a function so safe-ish
CFLAGS.print-icmp.c=	-Wno-format-nonliteral
CFLAGS.util-print.c=	-Wno-format-nonliteral

# Seemingly safe(ish) generated sscanf
CFLAGS.print-rx.c=	-Wno-format-nonliteral

.if ${MACHINE_CPUARCH} != "i386"
CFLAGS+=	-DLBL_ALIGN
.endif

MAN=
#STRIP=

LIBADD=	c_cheri

NO_SHARED=	yes

.PATH: ${.CURDIR}/${MACHINE_ARCH}
.PATH: ${.CURDIR}/../../lib/libc/${MACHINE}/string
.PATH: ${.CURDIR}/../../lib/libc/string
.PATH: ${.CURDIR}/../../contrib/tcpdump

WANT_DUMP=yes

.include <src.opts.mk>
.include <bsd.prog.mk>

CFLAGS+=	-Wno-cast-qual
CFLAGS:=	${CFLAGS:N-Wcast-align}
CFLAGS+= ${CFLAGS.${.IMPSRC:T}}
