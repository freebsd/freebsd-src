SRCS+=	\
	fwctl.c		\
	task_switch.c

.PATH:  ${BHYVE_SYSDIR}/sys/amd64/vmm
SRCS+=	vmm_instruction_emul.c
