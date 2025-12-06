BHYVE_GDB_SUPPORT=

SRCS+=	\
	atkbdc.c	\
	e820.c		\
	fwctl.c		\
	inout.c		\
	ioapic.c	\
	kernemu_dev.c	\
	mem_x86.c	\
	mptbl.c		\
	pci_fbuf.c	\
	pci_gvt-d.c	\
	pci_lpc.c	\
	pci_passthru.c	\
	pci_passthru_quirks.c	\
	pctestdev.c	\
	pm.c		\
	post.c		\
	ps2kbd.c	\
	ps2mouse.c	\
	rfb.c		\
	rtc.c		\
	spinup_ap.c	\
	task_switch.c	\
	vga.c		\
	xmsr.c

.PATH:  ${BHYVE_SYSDIR}/sys/amd64/vmm
SRCS+=	vmm_instruction_emul.c

CFLAGS.kernemu_dev.c+=	-I${SRCTOP}/sys/amd64

SUBDIR+=	kbdlayout
