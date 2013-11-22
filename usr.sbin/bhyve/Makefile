#
# $FreeBSD$
#

PROG=	bhyve

DEBUG_FLAGS= -g -O0 

SRCS=	acpi.c atpic.c bhyverun.c block_if.c consport.c dbgport.c elcr.c
SRCS+=  inout.c legacy_irq.c mem.c mevent.c mptbl.c pci_ahci.c
SRCS+=	pci_emul.c pci_hostbridge.c pci_lpc.c pci_passthru.c pci_virtio_block.c
SRCS+=	pci_virtio_net.c pci_uart.c pit_8254.c pmtmr.c post.c rtc.c
SRCS+=	uart_emul.c virtio.c xmsr.c spinup_ap.c

.PATH:	${.CURDIR}/../../sys/amd64/vmm
SRCS+=	vmm_instruction_emul.c

NO_MAN=

DPADD=	${LIBVMMAPI} ${LIBMD} ${LIBUTIL} ${LIBPTHREAD}
LDADD=	-lvmmapi -lmd -lutil -lpthread

WARNS?=	2

.include <bsd.prog.mk>
