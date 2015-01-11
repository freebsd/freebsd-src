.include <bsd.own.mk>

SUBDIR= ibaddr ibnetdiscover ibping ibportstate ibroute ibsendtrap ibstat \
	ibsysstat ibtracert opensm perfquery saquery \
	sminfo smpdump smpquery vendstat

.if ${MK_TESTS} != "no"
SUBDIR+= osmtest
.endif

.include <bsd.subdir.mk>
