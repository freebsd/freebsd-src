# Makefile for libpcap
# $FreeBSD$

SHLIBDIR?= /lib

.include <bsd.own.mk>

LIB=	pcap
SRCS=	grammar.y tokdefs.h version.h pcap-bpf.c \
	pcap.c inet.c fad-getad.c gencode.c optimize.c nametoaddr.c \
	etherent.c savefile.c bpf_filter.c bpf_image.c bpf_dump.c \
	scanner.l version.c
INCS=	pcap.h pcap-int.h pcap-namedb.h pcap-bpf.h
MAN=	pcap.3
MLINKS=	pcap.3 pcap_open_live.3 \
	pcap.3 pcap_open_dead.3 \
	pcap.3 pcap_open_offline.3 \
	pcap.3 pcap_fopen_offline.3 \
	pcap.3 pcap_dump_open.3 \
	pcap.3 pcap_dump_fopen.3 \
	pcap.3 pcap_setnonblock.3 \
	pcap.3 pcap_getnonblock.3 \
	pcap.3 pcap_findalldevs.3 \
	pcap.3 pcap_freealldevs.3 \
	pcap.3 pcap_lookupdev.3 \
	pcap.3 pcap_lookupnet.3 \
	pcap.3 pcap_dispatch.3 \
	pcap.3 pcap_loop.3 \
	pcap.3 pcap_dump.3 \
	pcap.3 pcap_compile.3 \
	pcap.3 pcap_setfilter.3 \
	pcap.3 pcap_freecode.3 \
	pcap.3 pcap_setdirection.3 \
	pcap.3 pcap_next.3 \
	pcap.3 pcap_next_ex.3 \
	pcap.3 pcap_breakloop.3 \
	pcap.3 pcap_inject.3 \
	pcap.3 pcap_sendpacket.3 \
	pcap.3 pcap_datalink.3 \
	pcap.3 pcap_list_datalinks.3 \
	pcap.3 pcap_set_datalink.3 \
	pcap.3 pcap_datalink_name_to_val.3 \
	pcap.3 pcap_datalink_val_to_name.3 \
	pcap.3 pcap_datalink_val_to_description.3 \
	pcap.3 pcap_snapshot.3 \
	pcap.3 pcap_is_swapped.3 \
	pcap.3 pcap_major_version.3 \
	pcap.3 pcap_minor_version.3 \
	pcap.3 pcap_stats.3 \
	pcap.3 pcap_file.3 \
	pcap.3 pcap_fileno.3 \
	pcap.3 pcap_get_selectable_fd.3 \
	pcap.3 pcap_perror.3 \
	pcap.3 pcap_geterr.3 \
	pcap.3 pcap_strerror.3 \
	pcap.3 pcap_lib_version.3 \
	pcap.3 pcap_close.3 \
	pcap.3 pcap_dump_flush.3 \
	pcap.3 pcap_dump_ftell.3 \
	pcap.3 pcap_dump_file.3 \
	pcap.3 pcap_dump_close.3

CLEANFILES=tokdefs.h version.h version.c

YFLAGS+=-p pcapyy
LFLAGS+=-Ppcapyy
CFLAGS+=-DHAVE_CONFIG_H -Dyylval=pcapyylval -I${.CURDIR} -I.
CFLAGS+=-D_U_="__attribute__((unused))"
CFLAGS+=-DHAVE_SNPRINTF -DHAVE_VSNPRINTF
.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+=-DINET6
.endif
.if ${MK_PF} != "no"
CFLAGS+=-DHAVE_NET_PFVAR_H
.endif

SHLIB_MAJOR=5

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
