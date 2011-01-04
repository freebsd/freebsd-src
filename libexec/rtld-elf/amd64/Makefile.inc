# $FreeBSD$

CFLAGS+=	-mno-sse -mno-sse2 -mno-sse3 -mno-mmx -mno-3dnow -msoft-float
# Uncomment this to build the dynamic linker as an executable instead
# of a shared library:
#LDSCRIPT=	${.CURDIR}/${MACHINE_CPUARCH}/elf_rtld.x
