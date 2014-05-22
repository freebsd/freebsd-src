#	$Id: QNX6.m4,v 1.1 2007-03-21 23:56:17 ca Exp $
#	This can go away (use CC-M in devel/OS/QNX.6.x) with newer qcc (PR 26458)
depend: ${BEFORE} ${LINKS}
	@mv Makefile Makefile.old
	@sed -e '/^# Do not edit or remove this line or anything below it.$$/,$$d' < Makefile.old > Makefile
	@echo "# Do not edit or remove this line or anything below it." >> Makefile
	${CC} -E -Wp,-M ${COPTS} ${SRCS} >> Makefile

