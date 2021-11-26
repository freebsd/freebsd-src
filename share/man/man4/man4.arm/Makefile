# $FreeBSD$

MAN=	\
	aw_gpio.4 \
	aw_mmc.4 \
	aw_rtc.4 \
	aw_sid.4 \
	aw_spi.4 \
	aw_syscon.4 \
	bcm283x_pwm.4 \
	devcfg.4 \
	imx6_ahci.4 \
	imx6_snvs.4 \
	imx_wdog.4 \
	mge.4 \
	ti_adc.4

MLINKS= imx_wdog.4 imxwdt.4
MLINKS+= mge.4 if_mge.4

# Link files to the architecture directory
_ARCH_SUBDIR=arm
.for _manpage _link in ${MLINKS}
MLINKS+=${_link} ${_ARCH_SUBDIR}/${_link}
.endfor
.for _manpage in ${MAN}
MLINKS+=${_manpage} ${_ARCH_SUBDIR}/${_manpage}
.endfor

.include <bsd.prog.mk>
