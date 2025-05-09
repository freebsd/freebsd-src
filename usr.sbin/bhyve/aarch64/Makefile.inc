SRCS+=	\
	fdt.c		\
	mem_aarch64.c	\
	rtc_pl031.c	\
	uart_pl011.c

.PATH:  ${BHYVE_SYSDIR}/sys/arm64/vmm
SRCS+=	vmm_instruction_emul.c

BHYVE_FDT_SUPPORT=
BHYVE_GDB_SUPPORT=
