#
# $FreeBSD$
#

# Temporary include path hack - to be removed once I clean lib9p.h up
CFLAGS+=-I${.CURDIR}/../../contrib/lib9p
.PATH:  ${.CURDIR}/../../contrib/lib9p

PROG=	bhyve

MAN=	bhyve.8

SRCS=	\
	atkbdc.c		\
	acpi.c			\
	bhyverun.c		\
	block_if.c		\
	bootrom.c		\
	consport.c		\
	dbgport.c		\
	fwctl.c			\
	inout.c			\
	ioapic.c		\
	mem.c			\
	mevent.c		\
	mptbl.c			\
	pci_ahci.c		\
	pci_emul.c		\
	pci_hostbridge.c	\
	pci_irq.c		\
	pci_lpc.c		\
	pci_passthru.c		\
	pci_virtio_9p.c		\
	pci_virtio_block.c	\
	pci_virtio_net.c	\
	pci_virtio_rnd.c	\
	pci_uart.c		\
	pm.c			\
	post.c			\
	rtc.c			\
	smbiostbl.c		\
	task_switch.c		\
	uart_emul.c		\
	virtio.c		\
	xmsr.c			\
	spinup_ap.c

.PATH:	${.CURDIR}/../../sys/amd64/vmm
SRCS+=	vmm_instruction_emul.c

LIBADD=	vmmapi md pthread 9p

WARNS?=	2

.include <bsd.prog.mk>
