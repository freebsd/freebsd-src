.include <kmod.opts.mk>

SUBDIR=	${_hpts} \
        ${_tcp_bbr} \
        ${_tcp_rack} \
	${_tcpmd5} \

.if (${MK_INET_SUPPORT} != "no" || ${MK_INET6_SUPPORT} != "no") || \
	defined(ALL_MODULES)
_hpts=		hpts
_tcp_bbr= 	bbr
_tcp_rack= 	rack
.if ${KERN_OPTS:MIPSEC_SUPPORT} && !${KERN_OPTS:MTCP_SIGNATURE}
_tcpmd5=	tcpmd5
.endif
.endif

.include <bsd.subdir.mk>
