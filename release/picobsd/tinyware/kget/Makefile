#	$Id: Makefile,v 1.2 1998/09/19 21:40:38 abial Exp $
#
PROG=kget
CFLAGS+= -I/usr/src/sys -DUC_PRIVATE -DKERN_NO_SYMBOLS
.PATH: ../../../sysinstall
SRCS= 	uc_isa.c uc_kmem.c uc_list.c uc_main.c uc_main.h \
	uc_eisa.c uc_pci.c
NOMAN=yes

.include <bsd.prog.mk>


