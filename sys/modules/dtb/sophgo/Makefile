.if ${MACHINE_ARCH} == "aarch64"
DTS=	\
	sophgo/sg2000_milkv_duos_glibc_arm64_emmc.dts \
	sophgo/sg2000_milkv_duos_glibc_arm64_sd.dts \
	sophgo/sg2000_wevb_arm64_sd.dts

.elif ${MACHINE_ARCH} == "riscv"
DTS=	\
	sophgo/sg2000_milkv_duos_musl_riscv64_emmc.dts \
	sophgo/sg2000_milkv_duos_musl_riscv64_sd.dts \
	sophgo/sg2000_wevb_riscv64_sd.dts

.endif

.include <bsd.dtb.mk>
