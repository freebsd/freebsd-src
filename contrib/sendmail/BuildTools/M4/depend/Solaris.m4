#	@(#)Solaris.m4	8.1	(Berkeley)	3/5/98
depend: ${BEFORE}
	@mv Makefile Makefile.old
	@sed -e '/^# Do not edit or remove this line or anything below it.$$/,$$d' < Makefile.old > Makefile
	@echo "# Do not edit or remove this line or anything below it." >> Makefile
	${CC} -xM ${COPTS} *.c >> Makefile

#	End of Solaris.m4
