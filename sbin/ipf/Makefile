.include <src.opts.mk>

SUBDIR=		libipf .WAIT
SUBDIR+=	ipf ipfstat ipmon ipnat ippool
.if ${MK_IPFILTER_IPFS} != "no"
SUBDIR+=	ipfs
.endif
# XXX Temporarily disconnected.
# SUBDIR+=	ipftest ipresend ipsend
SUBDIR_PARALLEL=

.include <bsd.subdir.mk>
