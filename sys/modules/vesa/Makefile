#	$Id: Makefile,v 1.4 1998/12/30 11:21:07 yokota Exp $

.PATH:	${.CURDIR}/../../dev/syscons ${.CURDIR}/../../i386/isa
KMOD	=	vesa
SRCS	=	vesa.c scvesactl.c sc.h vga.h opt_syscons.h opt_vga.h \
		opt_vesa.h opt_vm86.h opt_fb.h
NOMAN	=
CLEANFILES +=	sc.h vga.h opt_syscons.h opt_vga.h opt_vesa.h opt_vm86.h \
		opt_fb.h

sc.h:
	echo "#define NSC 1" > sc.h

vga.h:
	echo "#define NVGA 1" > vga.h

opt_syscons.h:
	touch opt_syscons.h

opt_vga.h:
	touch opt_vga.h

opt_vesa.h:
	echo "#define VESA 1" > opt_vesa.h

opt_vm86.h:
	echo "#define VM86 1" > opt_vm86.h

opt_fb.h:
	touch opt_fb.h

.include <bsd.kmod.mk>
