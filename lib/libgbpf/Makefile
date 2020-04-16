# $FreeBSD$

LIB=	gbpf
SRCS=	gbpf_driver.c \
	gbpf_elf_walker.c \
	ebpf_dev_driver.c \

SHLIB_MAJOR=1
WARNS=	3
INCS=	gbpf/gbpf.h \
	gbpf/core/gbpf_driver.h \
	gbpf/core/gbpf_elf_walker.h \
	gbpf/drivers/ebpf_dev_driver.h \

GBPFDIR=	${SRCTOP}/contrib/libgbpf
LIBADD=	elf

.PATH:	${GBPFDIR}/src/core ${GBPFDIR}/src/drivers \
	${GBPFDIR}/include \

.include <bsd.lib.mk>
