.include <src.opts.mk>

SUBDIR= ibaddr ibnetdiscover ibping ibportstate ibroute ibsendtrap ibstat \
	ibsysstat ibtracert opensm perfquery saquery \
	sminfo smpdump smpquery vendstat

.if ${MK_TESTS} != "no"
SUBDIR+= osmtest
.endif

SUBDIR_PARALLEL=

.include <bsd.subdir.mk>
