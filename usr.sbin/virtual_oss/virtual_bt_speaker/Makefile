PROG=		virtual_bt_speaker
MAN=		${PROG}.8

SRCS=		bt_speaker.c

CFLAGS+= 	-I${SRCTOP}/usr.sbin/virtual_oss/virtual_oss \
		-I${SRCTOP}/lib/virtual_oss/bt

LDFLAGS+= 	-lm -lbluetooth -lsdp

.include <bsd.prog.mk>
