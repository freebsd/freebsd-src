SRCS+=	\
	atkbdc.c	\
	fwctl.c		\
	post.c		\
	ps2kbd.c	\
	ps2mouse.c	\
	task_switch.c


.PATH:  ${BHYVE_SYSDIR}/sys/amd64/vmm
SRCS+=	vmm_instruction_emul.c

SUBDIR+=	kbdlayout
