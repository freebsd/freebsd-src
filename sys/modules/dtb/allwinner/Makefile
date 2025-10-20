# All the dts files for allwinner systems we support.

.if ${MACHINE_ARCH} == "armv7"
DTS=	\
	allwinner/sun4i-a10-cubieboard.dts \
	allwinner/sun4i-a10-olinuxino-lime.dts \
	allwinner/sun6i-a31s-sinovoip-bpi-m2.dts \
	allwinner/sun5i-a13-olinuxino.dts \
	allwinner/sun5i-r8-chip.dts \
	allwinner/sun7i-a20-bananapi.dts \
	allwinner/sun7i-a20-cubieboard2.dts \
	allwinner/sun7i-a20-lamobo-r1.dts \
	allwinner/sun7i-a20-olimex-som-evb.dts \
	allwinner/sun7i-a20-pcduino3.dts \
	allwinner/sun8i-a83t-bananapi-m3.dts \
	allwinner/sun8i-h2-plus-orangepi-zero.dts \
	allwinner/sun8i-h3-nanopi-m1.dts \
	allwinner/sun8i-h3-nanopi-m1-plus.dts \
	allwinner/sun8i-h3-nanopi-neo.dts \
	allwinner/sun8i-h3-orangepi-one.dts \
	allwinner/sun8i-h3-orangepi-pc.dts \
	allwinner/sun8i-h3-orangepi-pc-plus.dts \
	allwinner/sun8i-h3-orangepi-plus2e.dts

DTSO=	sun8i-a83t-sid.dtso \
	sun8i-h3-i2c0.dtso \
	sun8i-h3-i2c1.dtso \
	sun8i-h3-mmc0-disable.dtso \
	sun8i-h3-mmc1-disable.dtso \
	sun8i-h3-mmc2-disable.dtso \
	sun8i-h3-spi0.dtso \
	sun8i-h3-usb1.dtso \
	sun8i-h3-usb2.dtso

LINKS= \
	${DTBDIR}/sun4i-a10-cubieboard.dtb ${DTBDIR}/cubieboard.dtb \
	${DTBDIR}/sun4i-a10-olinuxino-lime.dtb ${DTBDIR}/olinuxino-lime.dtb \
	${DTBDIR}/sun6i-a31s-sinovoip-bpi-m2.dtb ${DTBDIR}/bananapim2.dtb \
	${DTBDIR}/sun7i-a20-bananapi.dtb ${DTBDIR}/bananapi.dtb \
	${DTBDIR}/sun7i-a20-cubieboard2.dtb ${DTBDIR}/cubieboard2.dtb \
	${DTBDIR}/sun7i-a20-olimex-som-evb.dtb ${DTBDIR}/olimex-a20-som-evb.dtb \
	${DTBDIR}/sun7i-a20-pcduino3.dtb ${DTBDIR}/pcduino3.dtb \
	${DTBDIR}/sun8i-a83t-bananapi-m3.dtb ${DTBDIR}/sinovoip-bpi-m3.dtb \
	${DTBDIR}/sun8i-a83t-bananapi-m3.dtb ${DTBDIR}/sun8i-a83t-sinovoip-bpi-m3.dtb
.elif ${MACHINE_ARCH} == "aarch64"
DTS=	\
	allwinner/sun50i-a64-nanopi-a64.dts \
	allwinner/sun50i-a64-olinuxino.dts \
	allwinner/sun50i-a64-pine64-lts.dts \
	allwinner/sun50i-a64-pine64-plus.dts \
	allwinner/sun50i-a64-pine64.dts \
	allwinner/sun50i-a64-pinebook.dts \
	allwinner/sun50i-a64-sopine-baseboard.dts \
	allwinner/sun50i-h5-orangepi-pc2.dts \
	allwinner/sun50i-h5-nanopi-neo2.dts \
	allwinner/sun50i-h6-pine-h64.dts \
	allwinner/sun50i-h6-pine-h64-model-b.dts

DTSO=	sun50i-a64-mmc0-disable.dtso \
	sun50i-a64-mmc1-disable.dtso \
	sun50i-a64-mmc2-disable.dtso \
	sun50i-a64-pwm.dtso \
	sun50i-a64-rpwm.dtso \
	sun50i-a64-spi0-spigen.dtso \
	sun50i-a64-timer.dtso \
	sun50i-h5-opp.dtso \
	sun50i-h5-nanopi-neo2-opp.dtso
.elif ${MACHINE_CPUARCH} == "riscv"
DTS=	\
	allwinner/sun20i-d1-dongshan-nezha-stu.dts \
	allwinner/sun20i-d1-lichee-rv.dts \
	allwinner/sun20i-d1-mangopi-mq-pro.dts \
	allwinner/sun20i-d1-nezha.dts
.endif

.include <bsd.dtb.mk>
