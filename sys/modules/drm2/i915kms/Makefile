# $FreeBSD$

.PATH:	${SRCTOP}/sys/dev/drm2/i915
KMOD	= i915kms
SRCS	= \
	dvo_ch7017.c \
	dvo_ch7xxx.c \
	dvo_ivch.c \
	dvo_ns2501.c \
	dvo_sil164.c \
	dvo_tfp410.c \
	i915_debug.c \
	i915_dma.c \
	i915_drv.c \
	i915_gem.c \
	i915_gem_context.c \
	i915_gem_evict.c \
	i915_gem_execbuffer.c \
	i915_gem_gtt.c \
	i915_gem_stolen.c \
	i915_gem_tiling.c \
	i915_irq.c \
	i915_suspend.c \
	intel_acpi.c \
	intel_bios.c \
	intel_crt.c \
	intel_ddi.c \
	intel_display.c \
	intel_dp.c \
	intel_dvo.c \
	intel_fb.c \
	intel_hdmi.c \
	intel_iic.c \
	intel_lvds.c \
	intel_modes.c \
	intel_opregion.c \
	intel_overlay.c \
	intel_panel.c \
	intel_pm.c \
	intel_ringbuffer.c \
	intel_sdvo.c \
	intel_sprite.c \
	intel_tv.c

.if ${MACHINE_CPUARCH} == "amd64"
SRCS	+= i915_ioc32.c
.endif

SRCS	+=								\
	opt_acpi.h							\
	opt_drm.h							\
	opt_syscons.h							\
	acpi_if.h							\
	bus_if.h							\
	fb_if.h								\
	device_if.h							\
	iicbb_if.h							\
	iicbus_if.h							\
	pci_if.h

.include <bsd.kmod.mk>

CWARNFLAGS.i915_debug.c=	-Wno-unused-function
CWARNFLAGS.intel_lvds.c=	-Wno-unused
CWARNFLAGS.intel_tv.c=		-Wno-unused
