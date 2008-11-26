# $FreeBSD: src/sys/modules/sound/driver/Makefile.inc,v 1.3.30.1 2008/10/02 02:57:24 kensmith Exp $

SRCS+=	ac97_if.h channel_if.h feeder_if.h mixer_if.h

.include "../Makefile.inc"
