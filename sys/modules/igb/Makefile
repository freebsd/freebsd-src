#$FreeBSD$
.PATH:  ${.CURDIR}/../../dev/igb
KMOD    = if_igb
SRCS    = device_if.h bus_if.h pci_if.h
SRCS    += if_igb.c $(SHARED_SRCS)
SHARED_SRCS = e1000_api.c e1000_phy.c e1000_nvm.c e1000_mac.c e1000_manage.c
SHARED_SRCS += e1000_osdep.c e1000_82575.c

CFLAGS += -I${.CURDIR}/../../dev/igb

# DEVICE_POLLING gives you non-interrupt handling
# not advisable since MSIX gives better results
#CFLAGS  += -DDEVICE_POLLING

clean:
	rm -f device_if.h bus_if.h pci_if.h setdef*
	rm -f *.o *.kld *.ko
	rm -f @ machine
	rm -f ${CLEANFILES}

man:
	mv /usr/share/man/man4/igb.4.gz /usr/share/man/man4/igbSAVE.4.gz
	cp igb.4 /usr/share/man/man4/

.include <bsd.kmod.mk>
