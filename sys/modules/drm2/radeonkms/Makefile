# $FreeBSD$

.include <bsd.own.mk>

.PATH:	${.CURDIR}/../../../dev/drm2/radeon

KMOD	= radeonkms
SRCS	=								\
	rn50_reg_safe.h							\
	r100_reg_safe.h							\
	r200_reg_safe.h							\
	rv515_reg_safe.h						\
	r300_reg_safe.h							\
	r420_reg_safe.h							\
	rs600_reg_safe.h						\
	r600_reg_safe.h							\
	evergreen_reg_safe.h						\
	cayman_reg_safe.h
SRCS	+=								\
	radeon_acpi.c							\
	radeon_agp.c							\
	radeon_asic.c							\
	radeon_atombios.c						\
	radeon_atpx_handler.c						\
	radeon_benchmark.c						\
	radeon_bios.c							\
	radeon_clocks.c							\
	radeon_combios.c						\
	radeon_connectors.c						\
	radeon_cp.c							\
	radeon_cs.c							\
	radeon_cursor.c							\
	radeon_device.c							\
	radeon_display.c						\
	radeon_drv.c							\
	radeon_encoders.c						\
	radeon_fb.c							\
	radeon_fence.c							\
	radeon_gart.c							\
	radeon_gem.c							\
	radeon_i2c.c							\
	radeon_irq.c							\
	radeon_irq_kms.c						\
	radeon_kms.c							\
	radeon_legacy_crtc.c						\
	radeon_legacy_encoders.c					\
	radeon_legacy_tv.c						\
	radeon_mem.c							\
	radeon_object.c							\
	radeon_pm.c							\
	radeon_ring.c							\
	radeon_sa.c							\
	radeon_semaphore.c						\
	radeon_state.c							\
	radeon_test.c							\
	radeon_ttm.c							\
	atom.c								\
	atombios_crtc.c							\
	atombios_dp.c							\
	atombios_encoders.c						\
	atombios_i2c.c							\
	r100.c								\
	r200.c								\
	r300.c								\
	r300_cmdbuf.c							\
	r420.c								\
	rs400.c								\
	rs600.c								\
	rs690.c								\
	rv515.c								\
	r520.c								\
	r600.c								\
	r600_audio.c							\
	r600_blit.c							\
	r600_blit_kms.c							\
	r600_blit_shaders.c						\
	r600_cp.c							\
	r600_cs.c							\
	r600_hdmi.c							\
	rv770.c								\
	evergreen.c							\
	evergreen_blit_kms.c						\
	evergreen_blit_shaders.c					\
	evergreen_cs.c							\
	evergreen_hdmi.c						\
	cayman_blit_shaders.c						\
	ni.c								\
	si.c								\
	si_blit_shaders.c

#radeon_ioc32.c
#radeon_prime.c
#--radeon_trace_points.c

SRCS	+=								\
	opt_acpi.h							\
	opt_compat.h							\
	opt_drm.h							\
	opt_syscons.h							\
	acpi_if.h							\
	bus_if.h							\
	fb_if.h								\
	device_if.h							\
	iicbb_if.h							\
	iicbus_if.h							\
	pci_if.h

CFLAGS  += -I${.CURDIR}/../../../dev/drm2/radeon ${GCC_MS_EXTENSIONS}

.include <bsd.kmod.mk>
