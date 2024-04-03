SRCS+=	\
	fdt.c		\
	uart_pl011.c

.PATH:  ${BHYVE_SYSDIR}/sys/arm64/vmm
SRCS+=	vmm_instruction_emul.c

BHYVE_FDT_SUPPORT=
