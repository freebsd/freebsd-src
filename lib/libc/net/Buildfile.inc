# $FreeBSD$

# machine-independent net sources
.PATH: ${.CURDIR}/net

SRCS += \
	base64.c \
	ether_addr.c \
	eui64.c \
	gai_strerror.c \
	getaddrinfo.c \
	gethostbydns.c \
	gethostbyht.c \
	gethostbynis.c \
	gethostnamadr.c \
	getifaddrs.c \
	getifmaddrs.c \
	getnameinfo.c \
	getnetbydns.c \
	getnetbyht.c \
	getnetbynis.c \
	getnetnamadr.c \
	getproto.c \
	getprotoent.c \
	getprotoname.c \
	getservent.c \
	hesiod.c \
	if_indextoname.c \
	if_nameindex.c \
	if_nametoindex.c \
	ip6opt.c \
	linkaddr.c \
	map_v4v6.c \
	name6.c \
	nscache.c \
	nscachedcli.c \
	nsdispatch.c \
	nslexer.c \
	nsparser.y \
	nss_compat.c \
	ntoh.c \
	rcmd.c \
	rcmdsh.c \
	recv.c \
	rthdr.c \
	sctp_sys_calls.c \
	send.c \
	sockatmark.c \
	sourcefilter.c \
	vars.c

SYM_MAPS += ${.CURDIR}/net/Symbol.map

CFLAGS += -DINET6

CFLAGS += -I${.OBJDIR}

# name6.c refers res_private.h
CFLAGS += -I${.CURDIR}/resolv

YFLAGS += -p_nsyy
LFLAGS += -P_nsyy

GENSRCS += nslexer.c

nslexer.c: nslexer.l
	${LEX} ${LFLAGS} -o${.TARGET}.tmp ${.ALLSRC}; \
	sed -e '/YY_BUF_SIZE/s/16384/1024/' ${.TARGET}.tmp > ${.TARGET}; \
	rm -f ${.TARGET}.tmp
