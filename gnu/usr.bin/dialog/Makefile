PROG=   dialog
MAN1=   dialog.1

CFLAGS+= -Wall -Wstrict-prototypes -DHAVE_NCURSES -DLOCALE
DPADD= $(LIBNCURSES) $(LIBMYTINFO)
LDADD= -lncurses -lmytinfo

SRCS = dialog.c rc.c checklist.c inputbox.c menubox.c msgbox.c \
radiolist.c textbox.c yesno.c

.include "bsd.prog.mk"
