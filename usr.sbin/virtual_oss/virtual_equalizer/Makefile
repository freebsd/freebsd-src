PROG=		virtual_equalizer
MAN=		${PROG}.8

SRCS=		equalizer.c

CFLAGS+= 	-I${SRCTOP}/usr.sbin/virtual_oss/virtual_oss \
		-I/usr/local/include

LDFLAGS+= 	-L/usr/local/lib -lm -lfftw3

.include <bsd.prog.mk>
