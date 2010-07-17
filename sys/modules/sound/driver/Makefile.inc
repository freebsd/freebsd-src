# $FreeBSD: src/sys/modules/sound/driver/Makefile.inc,v 1.3.36.1.4.1 2010/06/14 02:09:06 kensmith Exp $

SRCS+=	ac97_if.h channel_if.h feeder_if.h mixer_if.h

.include "../Makefile.inc"
