# $FreeBSD: src/sys/modules/sound/driver/Makefile.inc,v 1.3.32.1 2008/11/25 02:59:29 kensmith Exp $

SRCS+=	ac97_if.h channel_if.h feeder_if.h mixer_if.h

.include "../Makefile.inc"
