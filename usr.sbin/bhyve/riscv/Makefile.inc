SRCS+=			\
	fdt.c		\
	mem_md.c

.PATH:  ${BHYVE_SYSDIR}/sys/riscv/vmm
SRCS+=	vmm_instruction_emul.c

BHYVE_FDT_SUPPORT=
