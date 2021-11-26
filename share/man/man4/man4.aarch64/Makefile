# $FreeBSD$

.PATH: ${.CURDIR}/../man4.arm # Some manpages are common to arm and aarch64

MAN=	\
	armv8crypto.4 \
	enetc.4 \
	felix.4 \
	rk_gpio.4 \
	rk_grf.4 \
	rk_i2c.4 \
	rk_pinctrl.4 \

# Install manpages shared with arm only if not installing manpages
# for all architectures, otherwise arm takes care of installing them.
.if !empty(MAN_ARCH) && ${MAN_ARCH} != "all"
MAN+=	\
	aw_gpio.4 \
	aw_mmc.4 \
	aw_rtc.4 \
	aw_sid.4 \
	aw_spi.4 \
	aw_syscon.4 \
	bcm283x_pwm.4 \

.endif

# Link files to the architecture directory.
_ARCH_SUBDIR=aarch64
.for _manpage in ${MAN}
MLINKS+=${_manpage} ${_ARCH_SUBDIR}/${_manpage}
.endfor

.include <bsd.prog.mk>
