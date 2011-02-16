# $FreeBSD: src/sys/modules/sound/driver/Makefile.inc,v 1.3.36.1.6.1 2010/12/21 17:09:25 kensmith Exp $

SRCS+=	ac97_if.h channel_if.h feeder_if.h mixer_if.h

.include "../Makefile.inc"
