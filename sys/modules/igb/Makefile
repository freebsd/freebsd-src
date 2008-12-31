#$FreeBSD: src/sys/modules/igb/Makefile,v 1.3.2.2.2.1 2008/11/25 02:59:29 kensmith Exp $
.PATH:  ${.CURDIR}/../../dev/e1000
KMOD    = if_igb
SRCS    = device_if.h bus_if.h pci_if.h
SRCS    += if_igb.c $(SHARED_SRCS)
SHARED_SRCS = e1000_api.c e1000_phy.c e1000_nvm.c e1000_mac.c e1000_manage.c
SHARED_SRCS += e1000_80003es2lan.c e1000_82542.c e1000_82541.c e1000_82543.c
SHARED_SRCS += e1000_82540.c e1000_ich8lan.c e1000_82571.c e1000_osdep.c
SHARED_SRCS += e1000_82575.c

CFLAGS += -I${.CURDIR}/../../dev/e1000

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
