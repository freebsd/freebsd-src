#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	kvm_mkdb
SRCS=	kvm_mkdb.c nlist.c testdb.c
MAN8=	kvm_mkdb.8
CFLAGS+=-DDO_AOUT
.if defined(BINFORMAT) && ${BINFORMAT} == elf
CFLAGS+=-DDO_ELF
.endif

.include <bsd.prog.mk>
