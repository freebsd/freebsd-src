PROG=	set2061
SRCS=	main.c ICD2061Aalt.c
CFLAGS+= -DGCCUSESGAS -DPCVT_STANDALONE
NOMAN=

all: $(PROG)

install:
	@${ECHO} "set2061 is not installed automatically ...."

.include <bsd.prog.mk>

$(PROG):	compiler.h
