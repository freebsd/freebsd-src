#	$Id: $

.PATH:	${.CURDIR}/../../../sys/i386/isa
KMOD	=	vesa_mod
SRCS	=	vesa.c scvesactl.c sc.h opt_vesa.h opt_vm86.h
NOMAN	=
CFLAGS	+=	-DVESA_MODULE
CLEANFILES +=	sc.h opt_vesa.h opt_vm86.h

sc.h:
	echo "#define NSC 1" > sc.h

opt_vesa.h:
	echo "#define VESA 1" > opt_vesa.h

opt_vm86.h:
	echo "#define VM86 1" > opt_vm86.h

.include <bsd.kmod.mk>
