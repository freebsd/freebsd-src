.PATH: ${SRCTOP}/sys/dev/sound/pci/hda

KMOD=	snd_hda
SRCS=	device_if.h bus_if.h pci_if.h channel_if.h mixer_if.h hdac_if.h
SRCS+=	hdaa.c hdaa.h hdaa_patches.c hdacc.c hdac.c hdac_if.c
SRCS+=	hdac_private.h hdac_reg.h hda_reg.h hdac.h

.include <bsd.kmod.mk>
