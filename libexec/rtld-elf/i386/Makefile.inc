# $FreeBSD$

CFLAGS+=	-elf
LDFLAGS+=	-elf
# Uncomment this to build the dynamic linker as an executable instead
# of a shared library:
#LDSCRIPT=	${.CURDIR}/${MACHINE_CPUARCH}/elf_rtld.x
