# $FreeBSD: src/sys/modules/sound/driver/Makefile.inc,v 1.3.36.1.8.1 2012/03/03 06:15:13 kensmith Exp $

SRCS+=	ac97_if.h channel_if.h feeder_if.h mixer_if.h

.include "../Makefile.inc"
