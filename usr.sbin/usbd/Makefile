# FIXME have a look at all the other files and align it

PROG=	usbd
MAN=	usbd.8

# for FreeBSD we need MAN8 instead of MAN
MAN8=	usbd.8

# This hard coded path is not necessary as soon as we are in the
# base FreeBSD system. The .h files will be in /usr/include by then.
#
CFLAGS	+= -I../../sys

.include <bsd.prog.mk>
.include <bsd.subdir.mk>
