#
# Makefile for the drm device driver.  This driver provides support for the
# Direct Rendering Infrastructure (DRI) in XFree86 4.1.0 and higher.

O_TARGET	:= drm.o
list-multi	:= gamma.o tdfx.o r128.o mga.o i810.o i830.o radeon.o ffb.o sis.o savage.o

gamma-objs  := gamma_drv.o gamma_dma.o
tdfx-objs   := tdfx_drv.o
r128-objs   := r128_drv.o r128_cce.o r128_state.o
mga-objs    := mga_drv.o mga_dma.o mga_state.o mga_warp.o
i810-objs   := i810_drv.o i810_dma.o
i830-objs   := i830_drv.o i830_dma.o i830_irq.o
savage-objs := savage_dma.o savage_drv.o savage_state.o
radeon-objs := radeon_drv.o radeon_cp.o radeon_state.o radeon_mem.o radeon_irq.o
ffb-objs    := ffb_drv.o ffb_context.o
sis-objs    := sis_drv.o sis_ds.o sis_mm.o
via-objs    := via_drv.o via_ds.o via_map.o via_mm.o


obj-$(CONFIG_DRM_GAMMA) += gamma.o
obj-$(CONFIG_DRM_TDFX)	+= tdfx.o
obj-$(CONFIG_DRM_R128)	+= r128.o
obj-$(CONFIG_DRM_RADEON)+= radeon.o
obj-$(CONFIG_DRM_MGA)	+= mga.o
obj-$(CONFIG_DRM_I810)	+= i810.o
obj-$(CONFIG_DRM_I830)	+= i830.o
obj-$(CONFIG_DRM_FFB)   += ffb.o
obj-$(CONFIG_DRM_SIS)   += sis.o
obj-$(CONFIG_DRM_SAVAGE)	+= savage.o
obj-$(CONFIG_DRM_VIA)	+= via.o

include $(TOPDIR)/Rules.make

gamma.o: $(gamma-objs) $(lib)
	$(LD) -r -o $@ $(gamma-objs) $(lib)

tdfx.o: $(tdfx-objs) $(lib)
	$(LD) -r -o $@ $(tdfx-objs) $(lib)

mga.o: $(mga-objs) $(lib)
	$(LD) -r -o $@ $(mga-objs) $(lib)

i810.o: $(i810-objs) $(lib)
	$(LD) -r -o $@ $(i810-objs) $(lib)

i830.o: $(i830-objs) $(lib)
	$(LD) -r -o $@ $(i830-objs) $(lib)

r128.o: $(r128-objs) $(lib)
	$(LD) -r -o $@ $(r128-objs) $(lib)

radeon.o: $(radeon-objs) $(lib)
	$(LD) -r -o $@ $(radeon-objs) $(lib)

ffb.o: $(ffb-objs) $(lib)
	$(LD) -r -o $@ $(ffb-objs) $(lib)

sis.o: $(sis-objs) $(lib)
	$(LD) -r -o $@ $(sis-objs) $(lib)

savage.o: $(savage-objs) $(lib)
	$(LD) -r -o $@ $(savage-objs) $(lib)

via.o: $(via-objs) $(lib)
	$(LD) -r -o $@ $(via-objs) $(lib)
