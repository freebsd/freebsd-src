PROG=	ncftp

SRCS= cmds.c cmdtab.c ftp.c ftprc.c getpass.c glob.c main.c open.c set.c \
tips.c util.c

DPADD= $(LIBREADLINE) $(LIBTERMCAP)
LDADD= -lreadline -ltermcap

CFLAGS+= -DGZCAT=\"/usr/bin/gzcat\" -DREADLINE -DCURSES -DNO_CURSES_H \
	 -DSYSLOG -DTRY_ABOR -DGATEWAY \
	 -DFTP_DATA_BOTTOM=40000 -DFTP_DATA_TOP=44999

MK= $(CC) $(CFLAGS) $(LDADD)

cmds.o:
	$(CC) $(CFLAGS) -DMK='"$(MK)"' -c ${.CURDIR}/cmds.c

.include <bsd.prog.mk>
