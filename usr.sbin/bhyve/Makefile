#
#

.include <src.opts.mk>

PROG=	bhyve
PACKAGE=	bhyve

MAN=	bhyve.8 bhyve_config.5

BHYVE_SYSDIR?=${SRCTOP}

.PATH:	${.CURDIR}/${MACHINE_CPUARCH}	\
	${SRCTOP}/sys/libkern		\
	${SRCTOP}/sys/cam/ctl

SRCS=	\
	acpi.c			\
	acpi_device.c		\
	audio.c			\
	basl.c			\
	bhyvegc.c		\
	bhyverun.c		\
	bhyverun_machdep.c	\
	block_if.c		\
	bootrom.c		\
	config.c		\
	console.c		\
	crc16.c			\
	ctl_scsi_all.c		\
	ctl_util.c		\
	hda_codec.c		\
	iov.c			\
	mem.c			\
	mevent.c		\
	net_backend_netmap.c	\
	net_backend_slirp.c	\
	net_backends.c		\
	net_utils.c		\
	pci_emul.c		\
	pci_hostbridge.c	\
	pci_nvme.c		\
	pci_passthru.c		\
	pci_virtio_9p.c		\
	pci_virtio_block.c	\
	pci_virtio_console.c	\
	pci_virtio_input.c	\
	pci_virtio_net.c	\
	pci_virtio_rnd.c	\
	pci_virtio_scsi.c	\
	qemu_fwcfg.c		\
	qemu_loader.c		\
	smbiostbl.c		\
	sockstream.c		\
	tpm_device.c		\
	tpm_emul_passthru.c	\
	tpm_intf_crb.c		\
	tpm_ppi_qemu.c		\
	uart_emul.c		\
	usb_emul.c		\
	usb_mouse.c		\
	virtio.c		\
	vmexit.c		\
	vmgenc.c

.if ${MK_BHYVE_SNAPSHOT} != "no"
SRCS+=	snapshot.c
.endif

.include "${MACHINE_CPUARCH}/Makefile.inc"

.if defined(BHYVE_GDB_SUPPORT)
SRCS+=	gdb.c
CFLAGS+= -DBHYVE_GDB
.ifdef GDB_LOG
CFLAGS+=-DGDB_LOG
.endif
.endif

CFLAGS+=-I${.CURDIR}		\
	-I${.CURDIR}/../../contrib/lib9p \
	-I${SRCTOP}/sys

LIBADD=	vmmapi md nv pthread z util sbuf cam 9p

.if ${MK_BHYVE_SNAPSHOT} != "no"
LIBADD+= ucl xo
.endif

.if ${MK_INET_SUPPORT} != "no"
CFLAGS+=-DINET
.endif
.if ${MK_INET6_SUPPORT} != "no"
CFLAGS+=-DINET6
.endif
.if ${MK_NETGRAPH_SUPPORT} != "no"
SRCS+=	net_backend_netgraph.c
LIBADD+=	netgraph
.endif
.if ${MK_OPENSSL} == "no"
CFLAGS+=-DNO_OPENSSL
.else
LIBADD+=	crypto
CFLAGS+=-DOPENSSL_API_COMPAT=0x10100000L
.endif

CFLAGS+= -I${BHYVE_SYSDIR}/sys/dev/e1000
CFLAGS+= -I${BHYVE_SYSDIR}/sys/dev/mii
CFLAGS+= -I${BHYVE_SYSDIR}/sys/dev/usb/controller
.if ${MK_BHYVE_SNAPSHOT} != "no"
CFLAGS+= -I${SRCTOP}/contrib/libucl/include
CFLAGS+= -DBHYVE_SNAPSHOT
.endif

# Disable thread safety analysis since it only finds very simple bugs and
# yields many false positives.
NO_WTHREAD_SAFETY=

NO_WCAST_ALIGN=

.include <bsd.prog.mk>
