# $FreeBSD$

# Modules for dev/pwm drivers.

SUBDIR = \
	pwmbus \
	pwmc \

.if !empty(OPT_FDT)
SUBDIR += pwm_backlight
.endif

.include <bsd.subdir.mk>
