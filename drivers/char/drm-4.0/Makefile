#
# Makefile for the drm device driver.  This driver provides support for
# the Direct Rendering Infrastructure (DRI) in XFree86 4.x.
#

O_TARGET	:= drm.o


list-multi  := drmlib.o gamma.o tdfx.o r128.o ffb.o mga.o i810.o
gamma-objs  := gamma_drv.o  gamma_dma.o
tdfx-objs   := tdfx_drv.o                 tdfx_context.o
r128-objs   := r128_drv.o   r128_cce.o    r128_context.o r128_bufs.o r128_state.o
ffb-objs    := ffb_drv.o                  ffb_context.o
mga-objs    := mga_drv.o    mga_dma.o     mga_context.o  mga_bufs.o  mga_state.o
i810-objs   := i810_drv.o   i810_dma.o    i810_context.o i810_bufs.o
radeon-objs := radeon_drv.o radeon_cp.o   radeon_context.o radeon_bufs.o radeon_state.o

drmlib-objs	:= init.o memory.o proc.o auth.o context.o drawable.o bufs.o \
			lists.o lock.o ioctl.o fops.o vm.o dma.o ctxbitmap.o

ifneq ($(CONFIG_AGP),)
ifneq ($(CONFIG_AGP),n)
drmlib-objs 	+= agpsupport.o
endif
endif

obj-$(CONFIG_DRM40_GAMMA)	+= gamma.o	drmlib.o
obj-$(CONFIG_DRM40_TDFX)	+= tdfx.o	drmlib.o
obj-$(CONFIG_DRM40_R128)	+= r128.o	drmlib.o
obj-$(CONFIG_DRM40_RADEON)	+= radeon.o	drmlib.o
obj-$(CONFIG_DRM40_FFB)		+= ffb.o	drmlib.o
obj-$(CONFIG_DRM40_MGA)		+= mga.o	drmlib.o
obj-$(CONFIG_DRM40_I810)	+= i810.o	drmlib.o


include $(TOPDIR)/Rules.make

drmlib.o: $(drmlib-objs)
	$(LD) -r -o $@ $(drmlib-objs)

gamma.o: $(gamma-objs)
	$(LD) -r -o $@ $(gamma-objs)

tdfx.o: $(tdfx-objs)
	$(LD) -r -o $@ $(tdfx-objs)

mga.o: $(mga-objs)
	$(LD) -r -o $@ $(mga-objs)

i810.o: $(i810-objs)
	$(LD) -r -o $@ $(i810-objs)

r128.o: $(r128-objs)
	$(LD) -r -o $@ $(r128-objs)

radeon.o: $(radeon-objs)
	$(LD) -r -o $@ $(radeon-objs)

ffb.o: $(ffb-objs)
	$(LD) -r -o $@ $(ffb-objs)
