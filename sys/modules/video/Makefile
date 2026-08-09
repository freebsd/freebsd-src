S=	${SRCTOP}/sys

.PATH: $S/dev/video

KMOD=	video
SRCS=	video.c \
	video_if.h video_if.c \
	device_if.h bus_if.h

EXPORT_SYMS=	YES

.include <bsd.kmod.mk>
