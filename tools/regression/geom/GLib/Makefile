# $FreeBSD$

LIB=	G
SRCS= geom.c geom_bsd.c geom_simdev.c geom_dump.c geom_event.c geom_io.c \
	geom_kernsim.c geom_mbr.c geom_simdisk.c geom_enc.c \
	geom_simdisk_xml.c geom_slice.c geom_subr.c subr_sbuf.c \
	geom_sunlabel.c geom_pc98.c


CFLAGS+=	-g -static -W -Wall
CFLAGS+=	-Wstrict-prototypes -Wmissing-prototypes -Wpointer-arith
CFLAGS +=	-I/usr/local/include/xml -I/usr/src/sys -I..
NOPIC =		static
NOPROFILE =	bingo
NOOBJ =		youbet
WARNS =		2
LDADD +=r	-L/usr/local/lib -lexpat

VPATH+=	/sys/geom:..:/sys/kern

.include <bsd.lib.mk>

test:
