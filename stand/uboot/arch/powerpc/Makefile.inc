
SRCS+=		start.S conf.c ppc64_elf_freebsd.c
.PATH:		${SYSDIR}/libkern
SRCS+=		ucmpdi2.c

MK_PIE=		no
