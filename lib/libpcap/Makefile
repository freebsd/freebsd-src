# Makefile for libpcap
# $FreeBSD$

LIB=	pcap
SRCS=	grammar.y tokdefs.h version.h pcap-bpf.c \
	pcap.c inet.c fad-getad.c gencode.c optimize.c nametoaddr.c \
	etherent.c savefile.c bpf_filter.c bpf_image.c bpf_dump.c \
	scanner.l version.c
INCS=	pcap.h pcap-int.h pcap-namedb.h
MAN=	pcap.3
CLEANFILES=tokdefs.h version.h version.c

YFLAGS+=-p pcapyy
LFLAGS+=-Ppcapyy
CFLAGS+=-DHAVE_CONFIG_H -Dyylval=pcapyylval -I${.CURDIR} -I.
CFLAGS+=-D_U_="__attribute__((unused))"
CFLAGS+=-DHAVE_SNPRINTF -DHAVE_VSNPRINTF
.if !defined(NOINET6)
CFLAGS+=-DINET6
.endif

SHLIB_MAJOR=3

#
# Magic to grab sources out of src/contrib
#
PCAP_DISTDIR?=${.CURDIR}/../../contrib/libpcap
CFLAGS+=-I${PCAP_DISTDIR}
.PATH:	${PCAP_DISTDIR}
.PATH:	${PCAP_DISTDIR}/bpf/net

version.c: ${PCAP_DISTDIR}/VERSION
	@rm -f $@
	sed 's/.*/char pcap_version[] = "&";/' ${PCAP_DISTDIR}/VERSION > $@

version.h: ${PCAP_DISTDIR}/VERSION
	@rm -f $@
	sed 's/.*/char pcap_version_string[] = "libpcap version &";/' ${PCAP_DISTDIR}/VERSION > $@

tokdefs.h: grammar.h
	ln -sf grammar.h tokdefs.h

.include <bsd.lib.mk>
