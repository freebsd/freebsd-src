PACKAGE=	sound

.include "../Makefile.inc"

LDFLAGS+=	-L${.OBJDIR:H:H}/libsamplerate
