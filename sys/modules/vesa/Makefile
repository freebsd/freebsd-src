#	$Id: Makefile,v 1.5 1999/01/11 03:18:56 yokota Exp $

.PATH:	${.CURDIR}/../../dev/syscons ${.CURDIR}/../../i386/isa
KMOD	=	vesa
SRCS	=	vesa.c scvesactl.c sc.h vga.h opt_syscons.h opt_vga.h \
		opt_vesa.h opt_fb.h
NOMAN	=
CLEANFILES +=	sc.h vga.h opt_syscons.h opt_vga.h opt_vesa.h opt_fb.h

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

opt_fb.h:
	touch opt_fb.h

.include <bsd.kmod.mk>
