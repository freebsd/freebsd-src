# $FreeBSD$

SYSDIR?=${SRCTOP}/sys
.PATH: ${SYSDIR}/dev/ebpf
.PATH: ${SYSDIR}/contrib/generic-ebpf/sys/dev/ebpf


KMOD=	ebpf

# contrib sources
SRCS= \
	ebpf_allocator.c \
	ebpf_env.c \
	ebpf_freebsd.c \
	ebpf_interpreter.c \
	ebpf_map.c \
	ebpf_map_array.c \
	ebpf_map_hashtable.c \
	ebpf_obj.c \
	ebpf_prog.c \

# FreeBSD-specific sources
SRCS+= \
	ebpf_dev.c \
	ebpf_dev_freebsd.c \
	ebpf_map_progarray.c \
	ebpf_module.c \
	ebpf_probe.c \
	ebpf_probe_syscall.c \
	ebpf_prog_vfs.c \
	ebpf_prog_xdp.c\

CFLAGS += -I${SYSDIR}/contrib/generic-ebpf/sys

.include <bsd.kmod.mk>

