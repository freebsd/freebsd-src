.include <src.opts.mk>
.include "${SRCTOP}/secure/ssh.mk"

PROG=	ssh-sk-helper
SRCS=	ssh-sk-helper.c ssh-sk.c sk-usbhid.c
MAN=	ssh-sk-helper.8
CFLAGS+=-I${SRCTOP}/contrib/libfido2/src

LIBADD=	ssh crypto z
.if ${MK_USB} != "no"
LIBADD+= fido2 cbor
.endif

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
