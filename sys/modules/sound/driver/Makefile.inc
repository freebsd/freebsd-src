# $FreeBSD: src/sys/modules/sound/driver/Makefile.inc,v 1.3.36.1.2.1 2009/10/25 01:10:29 kensmith Exp $

SRCS+=	ac97_if.h channel_if.h feeder_if.h mixer_if.h

.include "../Makefile.inc"
