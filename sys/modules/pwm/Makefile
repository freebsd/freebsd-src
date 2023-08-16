
# Modules for dev/pwm drivers.

SUBDIR = \
	pwmbus \
	pwmc \

.if ${MACHINE_ARCH} == "armv7" || ${MACHINE_ARCH} == "aarch64"
.if !empty(OPT_FDT)
SUBDIR += pwm_backlight
.endif
.endif

.include <bsd.subdir.mk>
