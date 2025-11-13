.PATH:	${SRCTOP}/sys/dev/aq

KMOD	= if_aq
SRCS	= aq_main.c aq_media.c aq_irq.c
SRCS	+= aq_ring.c aq_hw.c aq_hw_llh.c
SRCS	+= aq_fw.c aq_fw1x.c aq_fw2x.c aq_dbg.c
SRCS	+= device_if.h bus_if.h pci_if.h
SRCS	+= ifdi_if.h opt_inet.h opt_inet6.h opt_platform.h opt_rss.h
SRCS	+= miibus_if.h

.include <bsd.kmod.mk>

