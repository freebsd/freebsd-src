LIB=	compat
NOPIC=	we dont need PIC for these puppies

SRCS=	sgtty.c ftime.c cftime.c regex.c setrgid.c setruid.c

MAN3=	libcompat.3

MLINKS= libcompat.3 stty.3 libcompat.3 gtty.3 \
	libcompat.3 ftime.3 \
	libcompat.3 cftime.3 libcompat.3 ascftime.3 \
	libcompat.3 re_exec.3 libcompat.3 re_comp.3 \
	libcompat.3 setrgid.3 libcompat.3 setruid.3

# Henry Spencer's regexp library from the UoT is
# kept in a separate subdirectory
.include "${.CURDIR}/regexp/Makefile.inc"

.include <bsd.lib.mk>
