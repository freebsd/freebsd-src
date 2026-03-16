.PATH:	${SRCTOP}/sys/netinet

KMOD=	tcpstats
SRCS=	tcp_statsdev.c tcp_statsdev_filter.c

# Uncomment to enable DTrace SDT probes
#CFLAGS+=	-DTCPSTATS_DTRACE

# Uncomment to enable detailed statistics counters
#CFLAGS+=	-DTCPSTATS_STATS

.if !empty(CFLAGS:M*TCPSTATS_DTRACE*)
CFLAGS+=	-DKDTRACE_HOOKS
.endif

.include <bsd.kmod.mk>
