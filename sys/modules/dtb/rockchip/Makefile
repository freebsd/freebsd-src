# All the dts files for rockchip systems we support.

.if ${MACHINE_ARCH} == "armv7"
DTS=	\
	rk3288-tinker.dts		\
	rk3288-tinker-s.dts
.elif ${MACHINE_ARCH} == "aarch64"
DTS=	\
	rockchip/rk3399-khadas-edge-captain.dts \
	rockchip/rk3399-khadas-edge.dts \
	rockchip/rk3399-khadas-edge-v.dts \
	rockchip/rk3399-nanopc-t4.dts \
	rockchip/rk3328-nanopi-r2s.dts \
	rockchip/rk3399-nanopi-r4s.dts \
	rockchip/rk3399-rock-pi-4.dts \
	rockchip/rk3328-rock-pi-e.dts \
	rockchip/rk3328-rock64.dts \
	rockchip/rk3399-firefly.dts \
	rockchip/rk3399-rockpro64.dts

DTSO=	rk3328-analog-sound.dtso \
	rk3328-i2c0.dtso \
	rk3328-uart1.dtso \
	rk3399-mmc0-disable.dtso \
	rk3399-mmc1-disable.dtso \
	rk3399-sdhci-disable.dtso
.endif

.include <bsd.dtb.mk>
