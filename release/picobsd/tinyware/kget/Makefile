#	$Id: Makefile,v 1.1.1.1 1998/07/14 07:30:54 abial Exp $
#
PROG=kget
CFLAGS+= -I/usr/src/sys -DUC_PRIVATE -DKERN_NO_SYMBOLS
SRCS= 	uc_isa.c uc_kmem.c uc_list.c uc_main.c uc_main.h \
	uc_eisa.c uc_pci.c uc_scsi.c
NOMAN=yes

.include <bsd.prog.mk>


