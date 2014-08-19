# $FreeBSD$

LIB=	jpeg
SHLIB_MAJOR= 11

.PATH: ${.CURDIR}/../../contrib/jpeg

CFLAGS+=	-I${.CURDIR}

SRCS=	\
	cdjpeg.c \
	jaricom.c \
	jcapimin.c \
	jcapistd.c \
	jcarith.c \
	jccoefct.c \
	jccolor.c \
	jcdctmgr.c \
	jchuff.c \
	jcinit.c \
	jcmainct.c \
	jcmarker.c \
	jcmaster.c \
	jcomapi.c \
	jcparam.c \
	jcprepct.c \
	jcsample.c \
	jctrans.c \
	jdapimin.c \
	jdapistd.c \
	jdarith.c \
	jdatadst.c \
	jdatasrc.c \
	jdcoefct.c \
	jdcolor.c \
	jddctmgr.c \
	jdhuff.c \
	jdinput.c \
	jdmainct.c \
	jdmarker.c \
	jdmaster.c \
	jdmerge.c \
	jdpostct.c \
	jdsample.c \
	jdtrans.c \
	jerror.c \
	jfdctflt.c \
	jfdctfst.c \
	jfdctint.c \
	jidctflt.c \
	jidctfst.c \
	jidctint.c \
	jmemmgr.c \
	jmemnobs.c \
	jquant1.c \
	jquant2.c \
	jutils.c \
	rdbmp.c \
	rdcolmap.c \
	rdgif.c \
	rdppm.c \
	rdrle.c \
	rdswitch.c \
	rdtarga.c \
	transupp.c \
	wrbmp.c \
	wrgif.c \
	wrppm.c \
	wrrle.c \
	wrtarga.c

INCS=	jconfig.h \
	jerror.h \
	jinclude.h \
	jmorecfg.h \
	jpegint.h \
	jpeglib.h

# XXXBD: broken with gcc due to -Wshadow
WARNS?=	6

USE_CHERI=yes

.include <src.opts.mk>
.include <bsd.lib.mk>

CFLAGS+=	-Wno-unused-parameter
