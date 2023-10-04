SRCS+=	\
	atkbdc.c	\
	e820.c		\
	fwctl.c		\
	kernemu_dev.c	\
	mptbl.c		\
	pm.c		\
	post.c		\
	ps2kbd.c	\
	ps2mouse.c	\
	rtc.c		\
	task_switch.c	\
	xmsr.c

.PATH:  ${BHYVE_SYSDIR}/sys/amd64/vmm
SRCS+=	vmm_instruction_emul.c

CFLAGS.kernemu_dev.c+=	-I${SRCTOP}/sys/amd64

SUBDIR+=	kbdlayout
