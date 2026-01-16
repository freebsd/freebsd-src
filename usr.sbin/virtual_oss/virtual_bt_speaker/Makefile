PROG=		virtual_bt_speaker
MAN=		${PROG}.8

SRCS=		bt_speaker.c

CFLAGS+= 	-I${SRCTOP}/usr.sbin/virtual_oss/virtual_oss \
		-I${SRCTOP}/lib/virtual_oss/bt

LIBADD=		bluetooth m sdp

.include <bsd.prog.mk>
