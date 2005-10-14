# $FreeBSD$

.PATH:  ${.CURDIR}/../../net

KMOD=	if_ppp
SRCS=	if_ppp.c ppp_tty.c slcompress.c \
	opt_inet.h opt_ipx.h opt_mac.h opt_ppp.h

PPP_BSDCOMP?=	1	# 0/1
PPP_DEFLATE?=	1	# 0/1
PPP_FILTER?=	1	# 0/1 - requires bpf to be configured in kernel
PPP_INET?=	1	# 0/1 - requires INET to be configured in kernel
PPP_IPX?=	0	# 0/1 - requires IPX to be configured in kernel

.if ${PPP_BSDCOMP} > 0
SRCS+=	bsd_comp.c
.endif
.if ${PPP_DEFLATE} > 0
SRCS+=	ppp_deflate.c zlib.c
.endif

.if !defined(KERNBUILDDIR)
.if ${PPP_INET} > 0
opt_inet.h:
	echo "#define INET 1" >> ${.TARGET}
.endif

.if ${PPP_IPX} > 0
opt_ipx.h:
	echo "#define IPX ${PPP_IPX}" >> ${.TARGET}
.endif

opt_ppp.h:
	touch ${.TARGET}
.if ${PPP_BSDCOMP} > 0
	echo "#define PPP_BSDCOMP ${PPP_BSDCOMP}" >> ${.TARGET}
.endif
.if ${PPP_DEFLATE} > 0
	echo "#define PPP_DEFLATE ${PPP_DEFLATE}" >> ${.TARGET}
.endif
.if ${PPP_FILTER} > 0
	echo "#define PPP_FILTER ${PPP_FILTER}" >> ${.TARGET}
.endif
.endif

.include <bsd.kmod.mk>
